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
          children: [
            Semantics(header: true, child: Text('Keys', style: type.heading)),
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
            const SizedBox(height: 20),
            _DoorState(
              coordinator: coordinator,
              onOpenSettings: onOpenSettings,
            ),
            const SizedBox(height: 24),
            Wrap(
              crossAxisAlignment: WrapCrossAlignment.center,
              spacing: 12,
              runSpacing: 8,
              children: [
                Text(email, style: type.muted),
                DooraButton(label: 'Sign out', onPressed: onSignOut),
              ],
            ),
          ],
        );
      },
    );
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

    return PanelRow(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          Semantics(
            container: true,
            label: spoken,
            excludeSemantics: true,
            child: Wrap(
              spacing: 8,
              runSpacing: 8,
              crossAxisAlignment: WrapCrossAlignment.center,
              children: [
                ReadoutChip(lamp: _lamp(status), label: status.label),
                const VirtualTag(),
              ],
            ),
          ),
          const SizedBox(height: 8),
          Text(
            '${formatDateTime(keyData.notBefore)} → ${formatDateTime(keyData.expiresAt)}',
            style: type.muted,
          ),
          if (row != KeyRow.statusOnly) const SizedBox(height: 10),
          switch (row) {
            KeyRow.notSetUp || KeyRow.needsCredential => Align(
              alignment: Alignment.centerLeft,
              child: DooraButton(
                label: settingUp ? 'Setting up…' : 'Set up this phone',
                kind: ButtonKind.primary,
                onPressed: settingUp || keys.settingUp != null
                    ? null
                    : () => keys.setUp(keyData.id),
              ),
            ),
            KeyRow.ready => Wrap(
              crossAxisAlignment: WrapCrossAlignment.center,
              spacing: 12,
              runSpacing: 8,
              children: [
                Text('Ready', style: type.body),
                if (coordinator.canUnlockNow)
                  DooraButton(
                    label: 'Unlock',
                    onPressed: coordinator.offerUnlock,
                  ),
              ],
            ),
            KeyRow.anotherPhone => Text(
              'Set up on ${keyData.deviceName}. Bindings are permanent. Ask an admin for a new key to use this phone.',
              style: type.body,
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

/// What the phone is doing about the door, and what blocks it. Each state has words and, where the
/// member can fix it, a way to.
class _DoorState extends StatelessWidget {
  const _DoorState({required this.coordinator, required this.onOpenSettings});

  final DoorCoordinator coordinator;
  final VoidCallback onOpenSettings;

  @override
  Widget build(BuildContext context) {
    final type = context.type;
    final keys = coordinator.keys;
    final scanner = coordinator.scanner;

    if (!keys.hasUsableCredential) {
      return Text(
        keys.keys.any((k) => k.status() == KeyStatus.active)
            ? 'Set up this phone for a key to unlock the door.'
            : 'No active key on this phone, so Doora is not looking for the door.',
        style: type.muted,
      );
    }

    if (!coordinator.primerSeen) {
      return Panel(
        children: [
          PanelRow(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                const Eyebrow('Bluetooth'),
                const SizedBox(height: 8),
                Text(
                  'Doora uses Bluetooth to find the door and unlock it.',
                  style: type.body,
                ),
                const SizedBox(height: 12),
                Align(
                  alignment: Alignment.centerLeft,
                  child: DooraButton(
                    label: 'Continue',
                    kind: ButtonKind.primary,
                    onPressed: coordinator.acceptBluetoothPrimer,
                  ),
                ),
              ],
            ),
          ),
        ],
      );
    }

    final problem = scanner.problem;
    final children = <Widget>[];
    if (problem == BluetoothProblem.off) {
      children.add(
        Text(
          'Bluetooth is off. Turn it on to unlock the door.',
          style: type.body,
        ),
      );
    } else if (problem == BluetoothProblem.permissionDenied) {
      children
        ..add(
          Text(
            "Doora can't use Bluetooth, so it can't find the door or unlock it.",
            style: type.body,
          ),
        )
        ..add(const SizedBox(height: 12))
        ..add(
          Align(
            alignment: Alignment.centerLeft,
            child: DooraButton(
              label: 'Open settings',
              onPressed: onOpenSettings,
            ),
          ),
        );
    } else if (problem == BluetoothProblem.unsupported) {
      children.add(
        Text("This phone doesn't support Bluetooth LE.", style: type.body),
      );
    } else {
      children.add(
        Text(
          scanner.near != null
              ? 'The door is near.'
              : 'Looking for the door. Walk up to it.',
          style: type.body,
        ),
      );
    }

    children
      ..add(const SizedBox(height: 16))
      ..add(
        _BackgroundPrompts(
          coordinator: coordinator,
          onOpenSettings: onOpenSettings,
        ),
      );

    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: children,
    );
  }
}

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
        const Eyebrow('Prompts at the door'),
        const SizedBox(height: 6),
        Text(
          on ? "When Doora is closed, a notification says you're at the door." : "Get a notification when you're at the door, even when Doora is closed. Doora then asks to send notifications.",
          style: type.body,
        ),
        const SizedBox(height: 10),
        Align(
          alignment: Alignment.centerLeft,
          child: DooraButton(
            label: on ? 'Turn off' : 'Turn on',
            onPressed: () => coordinator.setBackgroundPrompts(!on),
          ),
        ),
        if (coordinator.notificationsDenied) ...[
          const SizedBox(height: 10),
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
