import 'package:flutter/services.dart';
import 'package:material_ui/material_ui.dart';
import 'package:url_launcher/url_launcher.dart';

import '../config.dart';
import '../door/door_coordinator.dart';
import '../door/door_scanner.dart';
import '../theme/theme.dart';
import '../theme/widgets.dart';
import 'format.dart';
import 'key_controller.dart';
import 'virtual_key.dart';

/// The home screen: the member's virtual keys, what the phone can do for each, and the states
/// that stop the door from being found (Bluetooth off, denied, no keys, offline).
class KeysScreen extends StatelessWidget {
  const KeysScreen({
    super.key,
    required this.keys,
    required this.coordinator,
    required this.email,
    required this.onSignOut,
    required this.onOpenSettings,
  });

  final KeyController keys;
  final DoorCoordinator coordinator;
  final String email;
  final VoidCallback onSignOut;
  final VoidCallback onOpenSettings;

  @override
  Widget build(BuildContext context) {
    return ListenableBuilder(
      listenable: Listenable.merge([keys, coordinator, coordinator.scanner]),
      builder: (context, _) {
        final type = context.type;
        return Screen(
          onRefresh: keys.refresh,
          barStatus: _barStatus(),
          children: [
            _DoorState(
              coordinator: coordinator,
              onOpenSettings: onOpenSettings,
            ),
            const SizedBox(height: 36),
            Row(
              crossAxisAlignment: CrossAxisAlignment.baseline,
              textBaseline: TextBaseline.alphabetic,
              children: [
                Expanded(
                  child: Semantics(
                    header: true,
                    child: Text('Keys', style: type.display),
                  ),
                ),
                if (keys.loaded && keys.keys.isNotEmpty)
                  ExcludeSemantics(
                    child: Text(
                      keys.keys.length.toString().padLeft(2, '0'),
                      style: type.reading.copyWith(
                        color: context.colors.gray400,
                      ),
                    ),
                  ),
              ],
            ),
            const SizedBox(height: 16),
            if (keys.offline) ...[
              Text('Offline: showing saved keys', style: type.muted),
              const SizedBox(height: 12),
            ],
            if (keys.error != null) ...[
              Text(
                keys.error!,
                style: type.body.copyWith(color: context.colors.red400),
              ),
              const SizedBox(height: 12),
            ],
            if (!keys.loaded)
              const LoadingText()
            else if (keys.keys.isEmpty)
              Text('No virtual keys. Ask an admin.', style: type.muted)
            else
              Panel(
                children: [
                  for (final key in keys.keys)
                    _KeyRow(
                      key: ValueKey(key.id),
                      keyData: key,
                      keys: keys,
                      coordinator: coordinator,
                    ),
                ],
              ),
            const SizedBox(height: 36),
            Panel(
              children: [
                PanelRow(
                  child: Row(
                    children: [
                      Expanded(
                        child: Readout(label: 'Signed in as', value: email),
                      ),
                      DooraTextButton(
                        label: 'Sign out',
                        color: context.colors.gray600,
                        onPressed: onSignOut,
                      ),
                    ],
                  ),
                ),
              ],
            ),
          ],
        );
      },
    );
  }

  /// What the bar reports: whether the phone is watching for the door, and whether it is here.
  Widget? _barStatus() {
    if (!keys.hasUsableCredential || !coordinator.primerSeen) return null;
    final scanner = coordinator.scanner;
    if (scanner.problem != null) {
      return const BarStatus(lamp: LampState.redOff, label: 'Off');
    }
    final near = scanner.near;
    return near != null
        ? SignalStatus(rssi: near.rssi)
        : const BarStatus(lamp: LampState.blueWaiting, label: 'Scanning');
  }
}

class _KeyRow extends StatelessWidget {
  const _KeyRow({
    super.key,
    required this.keyData,
    required this.keys,
    required this.coordinator,
  });

  final VirtualKey keyData;
  final KeyController keys;
  final DoorCoordinator coordinator;

  static LampState _lamp(KeyStatus status) => switch (status) {
    KeyStatus.active => LampState.green,
    KeyStatus.expired => LampState.amber,
    KeyStatus.pending => LampState.blueWaiting,
    KeyStatus.disabled => LampState.redOff,
  };

  @override
  Widget build(BuildContext context) {
    final type = context.type;
    final status = keyData.status();
    final row = keys.rowFor(keyData);
    final setupError = keys.setupError(keyData.id);
    final settingUp = keys.settingUp == keyData.id;

    final spoken =
        'Key, ${status.label.toLowerCase()}, valid until ${spokenDate(keyData.expiresAt)}';
    final active = status == KeyStatus.active;
    final id = keyData.id.replaceAll('-', '');
    final tail = id.length > 4 ? id.substring(id.length - 4) : id;
    final span = keyData.expiresAt.difference(keyData.notBefore).inSeconds;
    final elapsed = DateTime.now().difference(keyData.notBefore).inSeconds;

    return PanelRow(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Semantics(
            container: true,
            label: spoken,
            excludeSemantics: true,
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                Row(
                  children: [
                    ReadoutChip(lamp: _lamp(status), label: status.label),
                    const Spacer(),
                    Text('KEY ··$tail'.toUpperCase(), style: type.eyebrow),
                  ],
                ),
                const SizedBox(height: 16),
                Opacity(
                  opacity: active ? 1 : 0.6,
                  child: Row(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Expanded(
                        child: Readout(
                          label: 'From',
                          value: formatReadingDate(keyData.notBefore),
                          detail: formatReadingTime(keyData.notBefore),
                        ),
                      ),
                      const SizedBox(width: 12),
                      Expanded(
                        child: Readout(
                          label: 'Until',
                          value: formatReadingDate(keyData.expiresAt),
                          detail: formatReadingTime(keyData.expiresAt),
                        ),
                      ),
                    ],
                  ),
                ),
                if (active && span > 0) ...[
                  const SizedBox(height: 14),
                  SpanMeter(
                    fraction: elapsed / span,
                    color: LampColors.green.withValues(alpha: 0.85),
                  ),
                ],
              ],
            ),
          ),
          if (row != KeyRow.statusOnly) const SizedBox(height: 16),
          switch (row) {
            KeyRow.notSetUp || KeyRow.needsCredential => DooraButton(
              label: settingUp ? 'Setting up…' : 'Set up this phone',
              kind: ButtonKind.primary,
              expand: true,
              onPressed: settingUp || keys.settingUp != null
                  ? null
                  : () => keys.setUp(keyData.id),
            ),
            KeyRow.ready => Row(
              children: [
                const Lamp(LampState.green),
                const SizedBox(width: 10),
                Text(
                  'ON THIS PHONE',
                  style: type.eyebrow.copyWith(color: context.colors.ink),
                ),
              ],
            ),
            KeyRow.anotherPhone => Text(
              'Set up on ${keyData.deviceName}. Bindings are permanent; ask an admin for a new key to use this phone.',
              style: type.muted,
            ),
            KeyRow.statusOnly => const SizedBox.shrink(),
          },
          if (setupError != null) ...[
            const SizedBox(height: 8),
            Semantics(
              liveRegion: true,
              child: Text(
                setupError,
                style: type.body.copyWith(color: context.colors.red400),
              ),
            ),
          ],
        ],
      ),
    );
  }
}

/// The door panel at the top: what the phone is doing about the door, what blocks it, and the
/// Unlock. Each state has a lamp, a headline and, where the member can act, one button.
class _DoorState extends StatelessWidget {
  const _DoorState({required this.coordinator, required this.onOpenSettings});

  final DoorCoordinator coordinator;
  final VoidCallback onOpenSettings;

  @override
  Widget build(BuildContext context) {
    final keys = coordinator.keys;
    final scanner = coordinator.scanner;
    final name = keys.onlyDoorName ?? 'The door';

    final LampState lamp;
    final String title;
    final String? detail;
    Widget? action;
    var prompts = false;

    if (!keys.hasUsableCredential) {
      final anyActive = keys.keys.any((k) => k.status() == KeyStatus.active);
      lamp = LampState.redOff;
      title = anyActive ? 'Not set up' : 'No active key';
      detail = anyActive
          ? 'Set up this phone for one of your keys below to unlock the door.'
          : 'Doora is not looking for the door. Ask an admin for a key.';
    } else if (!coordinator.primerSeen) {
      lamp = LampState.blueWaiting;
      title = 'One more step';
      detail = 'Doora uses Bluetooth to find the door and unlock it.';
      action = DooraButton(
        label: 'Continue',
        kind: ButtonKind.primary,
        expand: true,
        tall: true,
        onPressed: coordinator.acceptBluetoothPrimer,
      );
    } else {
      prompts = true;
      switch (scanner.problem) {
        case BluetoothProblem.off:
          lamp = LampState.redOff;
          title = 'Bluetooth is off';
          detail = 'Turn it on to unlock the door.';
        case BluetoothProblem.permissionDenied:
          lamp = LampState.redOff;
          title = 'No Bluetooth access';
          detail = "Doora can't find the door or unlock it without Bluetooth.";
          action = DooraButton(
            label: 'Open settings',
            expand: true,
            onPressed: onOpenSettings,
          );
        case BluetoothProblem.unsupported:
          lamp = LampState.redOff;
          title = 'No Bluetooth LE';
          detail =
              "This phone doesn't support it, so it can't unlock the door.";
        case null:
          final near = scanner.near != null;
          lamp = near ? LampState.green : LampState.blueWaiting;
          title = near ? 'At the door' : 'Looking for the door';
          detail = near ? null : 'Walk up to it and Doora offers to unlock.';
          if (coordinator.canUnlockNow) {
            action = DooraButton(
              label: 'Unlock',
              kind: ButtonKind.primary,
              expand: true,
              tall: true,
              onPressed: coordinator.offerUnlock,
            );
          }
      }
    }

    final type = context.type;
    return Panel(
      children: [
        PanelRow(
          child: Semantics(
            container: true,
            liveRegion: true,
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                Row(
                  children: [
                    Lamp(lamp),
                    const SizedBox(width: 10),
                    Expanded(child: Eyebrow(name)),
                  ],
                ),
                const SizedBox(height: 14),
                Text(title, style: type.display.copyWith(fontSize: 26)),
                if (detail != null) ...[
                  const SizedBox(height: 6),
                  Text(detail, style: type.muted),
                ],
                if (action != null) ...[const SizedBox(height: 20), action],
              ],
            ),
          ),
        ),
        if (prompts)
          PanelRow(
            child: _BackgroundPrompts(
              coordinator: coordinator,
              onOpenSettings: onOpenSettings,
            ),
          ),
      ],
    );
  }
}

/// The background prompt switch: a line of words and a Turn on / Turn off beside it.
class _BackgroundPrompts extends StatelessWidget {
  const _BackgroundPrompts({
    required this.coordinator,
    required this.onOpenSettings,
  });

  final DoorCoordinator coordinator;
  final VoidCallback onOpenSettings;

  @override
  Widget build(BuildContext context) {
    final type = context.type;
    final on = coordinator.backgroundPrompts;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Row(
          children: [
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  const Eyebrow('Prompts at the door'),
                  const SizedBox(height: 4),
                  Text(
                    on
                        ? "A notification when you're at the door, even with Doora closed."
                        : "Off. Turn on for a notification at the door, even with Doora closed.",
                    style: type.muted,
                  ),
                ],
              ),
            ),
            const SizedBox(width: 12),
            DooraButton(
              label: on ? 'Turn off' : 'Turn on',
              onPressed: () => coordinator.setBackgroundPrompts(!on),
            ),
          ],
        ),
        if (coordinator.notificationsDenied) ...[
          const SizedBox(height: 12),
          Text(
            'Notifications are off for Doora, so there is nothing to show at the door.',
            style: type.body,
          ),
          const SizedBox(height: 10),
          Align(
            alignment: Alignment.centerLeft,
            child: DooraButton(
              label: 'Open settings',
              onPressed: onOpenSettings,
            ),
          ),
        ],
      ],
    );
  }
}

/// "Done on the website" with the link.
class WebsiteNote extends StatelessWidget {
  const WebsiteNote(this.text, {super.key});
  final String text;

  @override
  Widget build(BuildContext context) {
    return Wrap(
      crossAxisAlignment: WrapCrossAlignment.center,
      children: [
        Text('$text ', style: context.type.muted),
        DooraTextButton(
          label: Config.websiteHost,
          onPressed: () async {
            HapticFeedback.selectionClick();
            await launchUrl(
              Uri.parse(Config.websiteUrl),
              mode: LaunchMode.externalApplication,
            );
          },
        ),
      ],
    );
  }
}
