import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:flutter_local_notifications/flutter_local_notifications.dart';

/// The "You're at the door" notification for an app in the background.
abstract class DoorNotifier {
  /// Wires the notification's Unlock action. [onUnlock] runs when the member taps it: the tap is
  /// the consent, and the app has been opened by then.
  Future<void> init({required VoidCallback onUnlock});

  /// Asks the system for permission to notify. True when granted. Call it only after the member
  /// has turned background prompts on.
  Future<bool> requestPermission();

  /// Shows the notification (iOS; on Android the native scan receiver posts it, see
  /// DoorScanReceiver.kt).
  Future<void> showAtDoor();

  Future<void> cancel();
}

class LocalDoorNotifier implements DoorNotifier {
  LocalDoorNotifier();

  static const String unlockAction = 'unlock';
  static const String _category = 'door';
  static const int _id = 1;

  final FlutterLocalNotificationsPlugin _plugin =
      FlutterLocalNotificationsPlugin();
  bool _ready = false;

  bool get _ios => defaultTargetPlatform == TargetPlatform.iOS;
  bool get _android => defaultTargetPlatform == TargetPlatform.android;

  @override
  Future<void> init({required VoidCallback onUnlock}) async {
    void handle(NotificationResponse response) {
      if (response.actionId == unlockAction) onUnlock();
    }

    await _plugin.initialize(
      settings: InitializationSettings(
        android: const AndroidInitializationSettings('@mipmap/ic_launcher'),
        iOS: DarwinInitializationSettings(
          // Asked later, when the member turns background prompts on.
          requestAlertPermission: false,
          requestSoundPermission: false,
          requestBadgePermission: false,
          notificationCategories: [
            DarwinNotificationCategory(
              _category,
              actions: [
                DarwinNotificationAction.plain(
                  unlockAction,
                  'Unlock',
                  options: {DarwinNotificationActionOption.foreground},
                ),
              ],
            ),
          ],
        ),
      ),
      onDidReceiveNotificationResponse: handle,
    );
    _ready = true;

    // The action that launched a killed app.
    final launch = await _plugin.getNotificationAppLaunchDetails();
    final response = launch?.notificationResponse;
    if (launch?.didNotificationLaunchApp == true && response != null) {
      handle(response);
    }
  }

  @override
  Future<bool> requestPermission() async {
    if (!_ready) return false;
    if (_ios) {
      return await _plugin
              .resolvePlatformSpecificImplementation<
                IOSFlutterLocalNotificationsPlugin
              >()
              ?.requestPermissions(alert: true, sound: true) ??
          false;
    }
    if (_android) {
      return await _plugin
              .resolvePlatformSpecificImplementation<
                AndroidFlutterLocalNotificationsPlugin
              >()
              ?.requestNotificationsPermission() ??
          false;
    }
    return false;
  }

  @override
  Future<void> showAtDoor() async {
    if (!_ready || !_ios) return;
    await _plugin.show(
      id: _id,
      title: "You're at the door",
      body: 'Tap Unlock to open it.',
      notificationDetails: const NotificationDetails(
        iOS: DarwinNotificationDetails(
          categoryIdentifier: _category,
          interruptionLevel: InterruptionLevel.timeSensitive,
          threadIdentifier: 'door',
        ),
      ),
    );
  }

  @override
  Future<void> cancel() async {
    if (_ready) await _plugin.cancel(id: _id);
  }
}
