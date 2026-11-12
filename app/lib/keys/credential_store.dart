import 'dart:convert';

import 'package:flutter_secure_storage/flutter_secure_storage.dart';

import 'virtual_key.dart';

/// The credential the backend signed for a key bound to this phone (a JWT, `aud: "ble"`).
class StoredCredential {
  const StoredCredential({
    required this.keyId,
    required this.jwt,
    required this.notBefore,
    required this.expiresAt,
  });

  factory StoredCredential.fromJson(Map<String, dynamic> json) =>
      StoredCredential(
        keyId: json['key_id'] as String,
        jwt: json['jwt'] as String,
        notBefore: DateTime.parse(json['not_before'] as String),
        expiresAt: DateTime.parse(json['expires_at'] as String),
      );

  Map<String, dynamic> toJson() => {
    'key_id': keyId,
    'jwt': jwt,
    'not_before': notBefore.toUtc().toIso8601String(),
    'expires_at': expiresAt.toUtc().toIso8601String(),
  };

  final String keyId;
  final String jwt;
  final DateTime notBefore;
  final DateTime expiresAt;

  /// Inside its validity window, judged by this phone's clock. The door judges again.
  bool usableAt(DateTime now) =>
      !now.isBefore(notBefore) && now.isBefore(expiresAt);
}

/// A door the app has heard of. `public.doors` is readable by admins only, so a member's cache
/// usually holds none and the door's id from its challenge is shown instead.
class DoorInfo {
  const DoorInfo({required this.id, this.name});
  final String id;
  final String? name;
}

/// Everything the app keeps on the phone, in the Keychain / Keystore: credentials, the markers of
/// which keys have a key pair here, the offline key list and the background-prompts setting. It
/// never holds a private key (those never leave the Secure Enclave or Keystore) and never logs.
class CredentialStore {
  CredentialStore([FlutterSecureStorage? storage])
    : _storage =
          storage ??
          // Readable while the phone is locked after the first unlock: a notification tap or a
          // background launch must be able to sign.
          const FlutterSecureStorage(
            iOptions: IOSOptions(
              accessibility: KeychainAccessibility.first_unlock_this_device,
            ),
          );

  final FlutterSecureStorage _storage;

  static const _credential = 'credential.';
  static const _pair = 'pair.';
  static const _keysCache = 'cache.keys';
  static const _doorsCache = 'cache.doors';
  static const _prompts = 'setting.background_prompts';
  static const _primer = 'setting.bluetooth_primer';

  // ---------------------------------------------------------------- credentials

  Future<void> saveCredential(StoredCredential credential) => _storage.write(
    key: '$_credential${credential.keyId}',
    value: jsonEncode(credential.toJson()),
  );

  Future<Map<String, StoredCredential>> credentials() async {
    final all = await _storage.readAll();
    final out = <String, StoredCredential>{};
    for (final entry in all.entries) {
      if (!entry.key.startsWith(_credential)) continue;
      try {
        final credential = StoredCredential.fromJson(
          jsonDecode(entry.value) as Map<String, dynamic>,
        );
        out[credential.keyId] = credential;
      } on Object {
        await _storage.delete(key: entry.key);
      }
    }
    return out;
  }

  Future<void> removeCredential(String keyId) =>
      _storage.delete(key: '$_credential$keyId');

  // ---------------------------------------------------------------- key pairs on this phone

  /// Marks that a key pair exists on this phone for [keyId]. A pair cannot be queried without being
  /// created, so the marker is how the app tells "set up on this phone" from "on another phone"
  /// after the credential is gone (signing out, a lost cache).
  Future<void> markPair(String keyId) =>
      _storage.write(key: '$_pair$keyId', value: '1');

  Future<Set<String>> pairs() async {
    final all = await _storage.readAll();
    return {
      for (final key in all.keys)
        if (key.startsWith(_pair)) key.substring(_pair.length),
    };
  }

  Future<void> removePair(String keyId) => _storage.delete(key: '$_pair$keyId');

  // ---------------------------------------------------------------- offline cache

  Future<void> saveKeys(List<VirtualKey> keys) => _storage.write(
    key: _keysCache,
    value: jsonEncode([for (final k in keys) k.toJson()]),
  );

  Future<List<VirtualKey>?> keys() async {
    final raw = await _storage.read(key: _keysCache);
    if (raw == null) return null;
    try {
      return [
        for (final row in jsonDecode(raw) as List<dynamic>)
          VirtualKey.fromRow(row as Map<String, dynamic>),
      ];
    } on Object {
      return null;
    }
  }

  Future<void> saveDoors(List<DoorInfo> doors) => _storage.write(
    key: _doorsCache,
    value: jsonEncode({for (final d in doors) d.id: d.name}),
  );

  Future<List<DoorInfo>> doors() async {
    final raw = await _storage.read(key: _doorsCache);
    if (raw == null) return const [];
    try {
      return [
        for (final e in (jsonDecode(raw) as Map<String, dynamic>).entries)
          DoorInfo(id: e.key, name: e.value as String?),
      ];
    } on Object {
      return const [];
    }
  }

  // ---------------------------------------------------------------- settings

  Future<bool> backgroundPrompts() async =>
      (await _storage.read(key: _prompts)) == '1';

  Future<void> setBackgroundPrompts(bool on) =>
      _storage.write(key: _prompts, value: on ? '1' : '0');

  /// The member has read why the app needs Bluetooth (shown before the system dialog).
  Future<bool> bluetoothPrimerSeen() async =>
      (await _storage.read(key: _primer)) == '1';

  Future<void> setBluetoothPrimerSeen() =>
      _storage.write(key: _primer, value: '1');

  // ---------------------------------------------------------------- sign-out

  /// Signing out: credentials and the cache go; key pairs stay (their markers too), so signing in
  /// again picks the same phone up.
  Future<void> clearAccount() async {
    final all = await _storage.readAll();
    for (final key in all.keys) {
      if (key.startsWith(_credential) ||
          key == _keysCache ||
          key == _doorsCache) {
        await _storage.delete(key: key);
      }
    }
  }
}
