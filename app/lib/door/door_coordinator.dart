import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:flutter/widgets.dart';

import '../keys/credential_store.dart';
import '../keys/key_backend.dart';
import '../keys/key_controller.dart';
import '../platform/system.dart';
import 'ble_protocol.dart';
import 'door_notifier.dart';
import 'door_scanner.dart';
import 'door_unlocker.dart';

/// Asks the screen to present the unlock sheet for [sighting], or for a remote unlock when it is
/// null.
class SheetRequest {
  const SheetRequest(this.sighting, {this.autoStart = false});
  const SheetRequest.remote() : sighting = null, autoStart = false;
  final DoorSighting? sighting;

  bool get remote => sighting == null;

  /// The member already tapped Unlock (the notification's action): start at once.
  final bool autoStart;
}

/// After the sheet is dismissed, do not prompt for that door again for this long.
const Duration promptCooldown = Duration(seconds: 60);

/// How long after a notification's Unlock tap the app waits for the door to be in range.
const Duration _autoUnlockWindow = Duration(seconds: 30);

/// Ties the keys, the scanner, the unlocker and the notification together: scans only while there
/// is a usable credential, prompts when the door is near (a sheet in the foreground, a
/// notification in the background) and never unlocks without a member's action.
class DoorCoordinator extends ChangeNotifier with WidgetsBindingObserver {
  DoorCoordinator({
    required this.keys,
    required this.scanner,
    required this.unlocker,
    required this.notifier,
    required this.system,
    required this.store,
    DateTime Function()? clock,
  }) : _clock = clock ?? DateTime.now;

  final KeyController keys;
  final DoorScanner scanner;
  final DoorUnlocker unlocker;
  final DoorNotifier notifier;
  final SystemBridge system;
  final CredentialStore store;
  final DateTime Function() _clock;

  /// The screen shows the sheet when this is set, and clears it when the sheet closes.
  final ValueNotifier<SheetRequest?> sheet = ValueNotifier<SheetRequest?>(null);

  final Map<String, DateTime> _quietUntil = {};
  DateTime? _autoUnlockUntil;
  bool _foreground = true;
  bool _primerSeen = false;
  bool _backgroundPrompts = false;
  bool _notificationsDenied = false;
  bool _started = false;
  bool _backgroundScanOn = false;

  /// The member has read why Bluetooth is needed, so the system dialog may appear.
  bool get primerSeen => _primerSeen;
  bool get backgroundPrompts => _backgroundPrompts;
  bool get notificationsDenied => _notificationsDenied;

  /// A door is near and there is a key to open it with: the row's Unlock button shows.
  bool get canUnlockNow =>
      scanner.near != null && keys.hasUsableCredential && _foreground;

  /// No door in Bluetooth range, but a key: the server can open it over the internet.
  bool get canUnlockRemotely =>
      scanner.near == null && keys.hasUsableCredential && _foreground;

  Future<void> start() async {
    if (_started) return;
    _started = true;
    WidgetsBinding.instance.addObserver(this);
    _foreground =
        WidgetsBinding.instance.lifecycleState != AppLifecycleState.paused &&
        WidgetsBinding.instance.lifecycleState != AppLifecycleState.hidden;
    _primerSeen = await store.bluetoothPrimerSeen();
    _backgroundPrompts = await store.backgroundPrompts();
    await notifier.init(onUnlock: _onUnlockAction);
    if (await system.takeUnlockLaunch()) _onUnlockAction();
    keys.addListener(_sync);
    scanner.addListener(_onScanner);
    await _sync();
  }

  /// The member read the Bluetooth explanation: scanning, and with it the system dialog, may start.
  Future<void> acceptBluetoothPrimer() async {
    _primerSeen = true;
    await store.setBluetoothPrimerSeen();
    notifyListeners();
    await _sync();
  }

  /// Background prompts on or off. Turning them on asks for notification permission first.
  Future<void> setBackgroundPrompts(bool on) async {
    if (on) {
      final granted = await notifier.requestPermission();
      _notificationsDenied = !granted;
      _backgroundPrompts = granted;
    } else {
      _backgroundPrompts = false;
      _notificationsDenied = false;
      await notifier.cancel();
    }
    await store.setBackgroundPrompts(_backgroundPrompts);
    notifyListeners();
    await _sync();
  }

  /// Scan only while there is a usable credential; otherwise stop everything.
  Future<void> _sync() async {
    final wanted = keys.hasUsableCredential && _primerSeen;
    if (!wanted) {
      await system.hideDoorActivity();
      await scanner.stop();
      await _stopBackgroundScan();
      return;
    }
    final androidBackground =
        !_foreground && defaultTargetPlatform == TargetPlatform.android;
    scanner.background = !_foreground;
    if (androidBackground) {
      // No Dart scanning in the background on Android: a PendingIntent scan wakes the receiver.
      await scanner.stop();
      if (_backgroundPrompts && !_backgroundScanOn) {
        _backgroundScanOn = true;
        await system.startBackgroundScan(doorServiceUuid);
      }
    } else {
      await _stopBackgroundScan();
      await scanner.start();
    }
  }

  Future<void> _stopBackgroundScan() async {
    if (!_backgroundScanOn) return;
    _backgroundScanOn = false;
    await system.stopBackgroundScan();
  }

  void _onScanner() {
    notifyListeners();
    final near = scanner.near;
    // The Live Activity lives exactly as long as the door is near.
    if (near != null && keys.hasUsableCredential) {
      unawaited(system.showDoorActivity(keys.onlyDoorName ?? 'Door'));
    } else {
      unawaited(system.hideDoorActivity());
    }
    if (near == null) return;
    if (!keys.hasUsableCredential) return;
    if (_isQuiet(near.deviceId)) return;
    if (_foreground) {
      _present(near, autoStart: _autoUnlockPending());
    } else if (_backgroundPrompts) {
      _quietUntil[near.deviceId] = _clock().add(promptCooldown);
      unawaited(notifier.showAtDoor());
    }
  }

  bool _isQuiet(String deviceId) {
    final until = _quietUntil[deviceId];
    return until != null && _clock().isBefore(until);
  }

  bool _autoUnlockPending() {
    final until = _autoUnlockUntil;
    return until != null && _clock().isBefore(until);
  }

  void _present(DoorSighting? sighting, {bool autoStart = false}) {
    if (sheet.value != null) return;
    if (autoStart) _autoUnlockUntil = null;
    sheet.value = sighting == null
        ? const SheetRequest.remote()
        : SheetRequest(sighting, autoStart: autoStart);
  }

  /// The row's Unlock button: the same sheet, without the cooldown.
  void offerUnlock() {
    final near = scanner.near;
    if (near != null && keys.hasUsableCredential) _present(near);
  }

  /// The Unlock remotely button.
  void offerRemoteUnlock() {
    if (canUnlockRemotely) _present(null);
  }

  /// The notification's Unlock action was tapped: unlock as soon as the door is in range.
  void _onUnlockAction() {
    unawaited(notifier.cancel());
    final near = scanner.near;
    if (near != null && keys.hasUsableCredential && _foreground) {
      _present(near, autoStart: true);
    } else {
      _autoUnlockUntil = _clock().add(_autoUnlockWindow);
    }
  }

  /// The sheet closed, however it closed. The door stays silent for [promptCooldown]: the member
  /// said not now, or just opened it and is still standing there. A remote sheet sets no cooldown.
  void sheetClosed(String? deviceId) {
    sheet.value = null;
    if (deviceId != null) _quietUntil[deviceId] = _clock().add(promptCooldown);
  }

  /// Runs the unlock with the credential that expires last.
  Future<UnlockOutcome> unlock(
    DoorSighting sighting, {
    void Function(String doorId)? onDoorId,
  }) async {
    final credential = keys.bestCredential();
    if (credential == null) return const Failed(FailureKind.noKey);
    return unlocker.unlock(
      deviceId: sighting.deviceId,
      credential: credential,
      onDoorId: onDoorId,
    );
  }

  /// Asks the server to open the door over the internet.
  Future<UnlockOutcome> unlockRemotely() async {
    try {
      return Opened(after: await keys.backend.remoteUnlock());
    } on KeyBackendException catch (e) {
      return RemoteFailed(e.message);
    } on OfflineException {
      return const RemoteFailed(
        "Couldn't reach the server. Check your connection.",
      );
    }
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    final foreground =
        state == AppLifecycleState.resumed ||
        state == AppLifecycleState.inactive;
    final wasForeground = _foreground;
    _foreground = foreground;
    if (foreground == wasForeground) return;
    if (foreground) {
      unawaited(notifier.cancel());
      unawaited(keys.refresh());
      unawaited(() async {
        for (final delay in const [
          Duration.zero,
          Duration(milliseconds: 400),
        ]) {
          await Future<void>.delayed(delay);
          if (await system.takeUnlockLaunch()) return _onUnlockAction();
        }
      }());
    }
    unawaited(_sync());
    notifyListeners();
  }

  @override
  void dispose() {
    WidgetsBinding.instance.removeObserver(this);
    keys.removeListener(_sync);
    scanner.removeListener(_onScanner);
    sheet.dispose();
    super.dispose();
  }
}
