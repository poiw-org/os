import 'package:flutter/foundation.dart';

import '../door/ble_protocol.dart';
import '../platform/device_key.dart';
import 'credential_store.dart';
import 'key_backend.dart';
import 'virtual_key.dart';

/// What a key row offers (brief section 4.2).
enum KeyRow {
  /// Active, no phone yet: "Set up this phone".
  notSetUp,

  /// Active, a key pair here but no credential (setup was interrupted): "Set up this phone" again.
  needsCredential,

  /// Active, credential stored: "Ready".
  ready,

  /// Active, bound to a phone that is not this one.
  anotherPhone,

  /// Disabled, expired or not yet active: the status only.
  statusOnly,
}

/// The member's virtual keys, the credentials on this phone and everything that keeps them current.
class KeyController extends ChangeNotifier {
  KeyController({
    required this.backend,
    required this.store,
    required this.deviceKey,
    DateTime Function()? clock,
  }) : _clock = clock ?? DateTime.now;

  final KeyBackend backend;
  final CredentialStore store;
  final DeviceKey deviceKey;
  final DateTime Function() _clock;

  List<VirtualKey> _keys = const [];
  Map<String, StoredCredential> _credentials = const {};
  Set<String> _pairs = const {};
  List<DoorInfo> _doors = const [];
  bool _loaded = false;
  bool _refreshing = false;
  bool _offline = false;
  String? _error;
  String? _settingUp;
  final Map<String, String> _setupErrors = {};

  /// Keys sorted active first, then by expiry.
  List<VirtualKey> get keys => _keys;
  bool get loaded => _loaded;
  bool get refreshing => _refreshing;

  /// The last refresh could not reach the server; the list is what was saved.
  bool get offline => _offline;

  /// The server answered with a failure the member can read.
  String? get error => _error;
  String? get settingUp => _settingUp;
  String? setupError(String keyId) => _setupErrors[keyId];
  bool get hasPairs => _pairs.isNotEmpty;

  KeyRow rowFor(VirtualKey key) {
    if (key.status(_clock()) != KeyStatus.active) return KeyRow.statusOnly;
    if (_credentials.containsKey(key.id)) return KeyRow.ready;
    if (_pairs.contains(key.id)) return KeyRow.needsCredential;
    if (key.deviceName != null) return KeyRow.anotherPhone;
    return KeyRow.notSetUp;
  }

  /// The door's display name: `public.doors` when cached, else null (the challenge's door id).
  String? doorName(String doorId) {
    for (final door in _doors) {
      if (door.id == doorId) return door.name;
    }
    return null;
  }

  /// The one name to show before the challenge arrives: the only cached door, if there is one.
  String? get onlyDoorName => _doors.length == 1 ? _doors.first.name : null;

  /// The credential to unlock with: usable now, and the one that expires last.
  StoredCredential? bestCredential() {
    final now = _clock();
    StoredCredential? best;
    for (final credential in _credentials.values) {
      if (!credential.usableAt(now)) continue;
      final key = _keyById(credential.keyId);
      if (key != null && key.status(now) != KeyStatus.active) continue;
      if (best == null || credential.expiresAt.isAfter(best.expiresAt)) {
        best = credential;
      }
    }
    return best;
  }

  bool get hasUsableCredential => bestCredential() != null;

  VirtualKey? _keyById(String id) {
    for (final key in _keys) {
      if (key.id == id) return key;
    }
    return null;
  }

  void _sort() {
    final now = _clock();
    _keys = [..._keys]..sort((a, b) => VirtualKey.compare(a, b, now));
  }

  /// Cache first, then the network.
  Future<void> load() async {
    _keys = await store.keys() ?? const [];
    _credentials = await store.credentials();
    _pairs = await store.pairs();
    _doors = await store.doors();
    _sort();
    _loaded = true;
    notifyListeners();
    await refresh();
  }

  /// Lists the keys, renews the credentials of keys set up on this phone and drops the ones that
  /// are disabled, expired or gone. Offline, keeps everything.
  Future<void> refresh() async {
    if (_refreshing) return;
    _refreshing = true;
    notifyListeners();
    try {
      final keys = await backend.fetchKeys();
      _offline = false;
      _error = null;
      _keys = keys;
      _sort();
      await store.saveKeys(_keys);
      await _syncCredentials();
      try {
        final doors = await backend.fetchDoors();
        if (doors.isNotEmpty) {
          _doors = doors;
          await store.saveDoors(doors);
        }
      } on Object {
        // Door names are a nicety; the door's own id is the fallback.
      }
    } on OfflineException {
      _offline = true;
    } on KeyBackendException catch (e) {
      _error = e.message;
    } finally {
      _loaded = true;
      _refreshing = false;
      notifyListeners();
    }
  }

  Future<void> _syncCredentials() async {
    final now = _clock();
    final known = {for (final key in _keys) key.id: key};

    for (final id in {..._credentials.keys, ..._pairs}) {
      final key = known[id];
      if (key == null) {
        // The key is gone: nothing to renew, nothing to keep.
        await _dropCredential(id);
        await _forgetPair(id);
      } else if (key.status(now) != KeyStatus.active) {
        await _dropCredential(id);
      } else if (_pairs.contains(id)) {
        await _renew(id);
      }
    }
  }

  Future<void> _renew(String keyId) async {
    try {
      final credential = await _requestCredential(keyId);
      _credentials = {..._credentials, keyId: credential};
      await store.saveCredential(credential);
    } on OfflineException {
      // Keep what is stored: unlocking never needs the network.
    } on KeyBackendException {
      // The backend refused this key for this phone now (blocked user, another phone).
      await _dropCredential(keyId);
    } on DeviceKeyException {
      await _dropCredential(keyId);
    }
  }

  Future<StoredCredential> _requestCredential(String keyId) async {
    final name = await deviceKey.deviceName();
    final pub = await deviceKey.publicKey(keyId);
    return backend.credential(
      keyId: keyId,
      devicePubkey: base64UrlNoPad(pub),
      deviceName: name,
    );
  }

  Future<void> _dropCredential(String keyId) async {
    if (!_credentials.containsKey(keyId)) return;
    _credentials = {..._credentials}..remove(keyId);
    await store.removeCredential(keyId);
  }

  Future<void> _forgetPair(String keyId) async {
    if (!_pairs.contains(keyId)) return;
    try {
      await deviceKey.delete(keyId);
    } on DeviceKeyException {
      // Already gone.
    }
    _pairs = {..._pairs}..remove(keyId);
    await store.removePair(keyId);
  }

  /// "Set up this phone": creates the key pair, asks `ble-credential` to bind it and stores the
  /// credential. Errors are kept per key for the row to show.
  Future<void> setUp(String keyId) async {
    if (_settingUp != null) return;
    _settingUp = keyId;
    _setupErrors.remove(keyId);
    notifyListeners();
    try {
      final name = await deviceKey.deviceName();
      final pub = await deviceKey.publicKey(keyId);
      await store.markPair(keyId);
      _pairs = {..._pairs, keyId};
      final credential = await backend.credential(
        keyId: keyId,
        devicePubkey: base64UrlNoPad(pub),
        deviceName: name,
      );
      await store.saveCredential(credential);
      _credentials = {..._credentials, keyId: credential};
      _keys = [
        for (final key in _keys)
          if (key.id == keyId)
            VirtualKey(
              id: key.id,
              notBefore: key.notBefore,
              expiresAt: key.expiresAt,
              disabled: key.disabled,
              deviceName: key.deviceName ?? name,
            )
          else
            key,
      ];
      await store.saveKeys(_keys);
    } on OfflineException {
      _setupErrors[keyId] =
          "You're offline. Connect to the internet to set up this phone.";
    } on KeyBackendException catch (e) {
      _setupErrors[keyId] = e.message;
      if (e.conflict) {
        // Bound to another phone: this phone's pair can never be used for this key.
        await _forgetPair(keyId);
      }
    } on DeviceKeyException {
      _setupErrors[keyId] = "This phone couldn't create its key. Try again.";
    } finally {
      _settingUp = null;
      notifyListeners();
    }
  }

  /// Signing out: the credentials and the cache go. Key pairs stay, so signing in again picks this
  /// phone up for the same keys.
  Future<void> clearForSignOut() async {
    await store.clearAccount();
    _keys = const [];
    _credentials = const {};
    _doors = const [];
    _setupErrors.clear();
    _offline = false;
    _error = null;
    _loaded = false;
    notifyListeners();
  }

  /// Deletes every key pair on this phone. A deleted pair can never be set up again for its key;
  /// the screen asks first.
  Future<void> deletePairs() async {
    for (final id in {..._pairs}) {
      await _forgetPair(id);
    }
    await clearForSignOut();
  }
}
