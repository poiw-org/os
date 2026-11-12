import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:flutter/services.dart';

/// What the phone's own OS does for the app that Flutter packages do not: open the app's system
/// settings, and on Android scan for the door in the background.
///
/// Channel `org.poiw.doora/system`, implemented in AppDelegate.swift and MainActivity.kt.
abstract class SystemBridge {
  /// The app's page in the system settings.
  Future<void> openSettings();

  /// Android only. Registers a `PendingIntent` BLE scan for the door's service UUID; the system
  /// wakes the app's receiver when the door is seen, even with the app killed, and the receiver
  /// posts the "You're at the door" notification itself. No-op on iOS (Core Bluetooth state
  /// restoration does this there).
  Future<void> startBackgroundScan(String serviceUuid);

  Future<void> stopBackgroundScan();

  /// True once if the app was opened by an Unlock action: the Android notification's, a tap on the
  /// door's reader (Android opens the app on its tag; elsewhere the website hands over with
  /// `doora://unlock`), or the iOS Live Activity's `doora://unlock` link.
  Future<bool> takeUnlockLaunch();

  /// iOS only. Shows the "door at hand" Live Activity, or updates it.
  Future<void> showDoorActivity(String doorName);

  /// iOS only. Removes it.
  Future<void> hideDoorActivity();
}

class PlatformSystemBridge implements SystemBridge {
  const PlatformSystemBridge();

  static const MethodChannel _channel = MethodChannel('org.poiw.doora/system');

  bool get _android => defaultTargetPlatform == TargetPlatform.android;

  Future<T?> _call<T>(String method, [Object? arguments]) async {
    try {
      return await _channel.invokeMethod<T>(method, arguments);
    } on PlatformException {
      return null;
    } on MissingPluginException {
      return null;
    }
  }

  @override
  Future<void> openSettings() => _call<void>('openSettings');

  @override
  Future<void> startBackgroundScan(String serviceUuid) async {
    if (_android) {
      await _call<void>('startBackgroundScan', {'serviceUuid': serviceUuid});
    }
  }

  @override
  Future<void> stopBackgroundScan() async {
    if (_android) await _call<void>('stopBackgroundScan');
  }

  @override
  Future<bool> takeUnlockLaunch() async =>
      (await _call<bool>('takeUnlockLaunch')) == true;

  @override
  Future<void> showDoorActivity(String doorName) async {
    if (defaultTargetPlatform == TargetPlatform.iOS) {
      await _call<void>('startDoorActivity', {'doorName': doorName});
    }
  }

  @override
  Future<void> hideDoorActivity() async {
    if (defaultTargetPlatform == TargetPlatform.iOS) {
      await _call<void>('endDoorActivity');
    }
  }
}
