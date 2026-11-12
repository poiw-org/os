import 'dart:async';

import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:material_ui/material_ui.dart';
import 'package:supabase_flutter/supabase_flutter.dart';

import 'auth/auth_backend.dart';
import 'auth/auth_controller.dart';
import 'auth/auth_screens.dart';
import 'config.dart';
import 'door/ble_door_transport.dart';
import 'door/door_coordinator.dart';
import 'door/door_notifier.dart';
import 'door/door_scanner.dart';
import 'door/door_unlocker.dart';
import 'door/unlock_sheet.dart';
import 'keys/credential_store.dart';
import 'keys/key_backend.dart';
import 'keys/key_controller.dart';
import 'keys/keys_screen.dart';
import 'platform/biometrics.dart';
import 'platform/device_key.dart';
import 'platform/system.dart';
import 'theme/theme.dart';
import 'theme/widgets.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  if (!Config.isComplete) {
    runApp(const DooraApp.unconfigured());
    return;
  }

  // Before any other Bluetooth call: lets iOS relaunch the app when the door is seen.
  await FlutterBluePlus.setOptions(restoreState: true);
  await Supabase.initialize(
    url: Config.supabaseUrl,
    publishableKey: Config.supabasePublishableKey,
  );
  final client = Supabase.instance.client;

  const deviceKey = PlatformDeviceKey();
  final store = CredentialStore();
  final auth = AuthController(SupabaseAuthBackend(client));
  final keys = KeyController(
    backend: SupabaseKeyBackend(client),
    store: store,
    deviceKey: deviceKey,
  );
  const system = PlatformSystemBridge();
  final coordinator = DoorCoordinator(
    keys: keys,
    scanner: DoorScanner(),
    unlocker: DoorUnlocker(
      transport: const BleDoorTransport(),
      deviceKey: deviceKey,
    ),
    notifier: LocalDoorNotifier(),
    system: system,
    store: store,
  );

  runApp(
    DooraApp(auth: auth, keys: keys, coordinator: coordinator, system: system),
  );
}

class DooraApp extends StatefulWidget {
  const DooraApp({
    super.key,
    required AuthController this.auth,
    required KeyController this.keys,
    required DoorCoordinator this.coordinator,
    required SystemBridge this.system,
  });

  /// A build without `SUPABASE_URL` / `SUPABASE_PUBLISHABLE_KEY`.
  const DooraApp.unconfigured({super.key})
    : auth = null,
      keys = null,
      coordinator = null,
      system = null;

  final AuthController? auth;
  final KeyController? keys;
  final DoorCoordinator? coordinator;
  final SystemBridge? system;

  @override
  State<DooraApp> createState() => _DooraAppState();
}

class _DooraAppState extends State<DooraApp> {
  final GlobalKey<NavigatorState> _navigator = GlobalKey<NavigatorState>();
  final Biometrics _biometrics = Biometrics();
  AuthStage? _lastStage;

  @override
  void initState() {
    super.initState();
    final auth = widget.auth;
    if (auth == null) return;
    auth.addListener(_onAuth);
    widget.coordinator!.sheet.addListener(_onSheet);
    unawaited(auth.start());
  }

  /// The keys and the door watcher run only while the member is signed in and cleared for the app.
  void _onAuth() {
    final stage = widget.auth!.stage;
    if (stage == _lastStage) return;
    final was = _lastStage;
    _lastStage = stage;
    final keys = widget.keys!;
    final coordinator = widget.coordinator!;
    if (stage == AuthStage.ready && was != AuthStage.ready) {
      unawaited(keys.load().then((_) => coordinator.start()));
    } else if (stage == AuthStage.signedOut && was != null) {
      unawaited(keys.clearForSignOut());
    }
  }

  Future<void> _onSheet() async {
    final coordinator = widget.coordinator!;
    final request = coordinator.sheet.value;
    final context = _navigator.currentContext;
    if (request == null || context == null) return;
    final credential = widget.keys!.bestCredential();
    if (credential == null) {
      coordinator.sheetClosed(request.sighting.deviceId);
      return;
    }
    await showUnlockSheet(
      context,
      initialDoorName: widget.keys!.onlyDoorName ?? 'Door',
      doorNameFor: widget.keys!.doorName,
      keyExpiresAt: credential.expiresAt,
      autoStart: request.autoStart,
      unlock: (onDoorId) async {
        // Face ID before the radio: a phone in someone else's hand opens nothing.
        if (!await _biometrics.confirmUnlock(
          widget.keys!.onlyDoorName ?? 'the door',
        )) {
          return const Failed(FailureKind.notConfirmed);
        }
        return coordinator.unlock(request.sighting, onDoorId: onDoorId);
      },
    );
    coordinator.sheetClosed(request.sighting.deviceId);
  }

  Future<void> _confirmSignOut() async {
    final keys = widget.keys!;
    final choice = await showDialog<_SignOutChoice>(
      context: _navigator.currentContext!,
      builder: (context) => const SignOutDialog(),
    );
    if (choice == null) return;
    if (choice == _SignOutChoice.deleteKeys) await keys.deletePairs();
    await widget.auth!.signOut();
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      navigatorKey: _navigator,
      title: 'Doora',
      debugShowCheckedModeBanner: false,
      theme: hostTheme(),
      builder: (context, child) =>
          DooraTheme(palette: Palette.dark, child: child!),
      home: widget.auth == null
          ? const StartingScreen(
              message: 'This build has no server configured.',
            )
          : _Root(app: widget, onSignOut: _confirmSignOut),
    );
  }
}

class _Root extends StatelessWidget {
  const _Root({required this.app, required this.onSignOut});
  final DooraApp app;
  final VoidCallback onSignOut;

  @override
  Widget build(BuildContext context) {
    final auth = app.auth!;
    return ListenableBuilder(
      listenable: auth,
      builder: (context, _) {
        final Widget screen = switch (auth.stage) {
          AuthStage.starting || AuthStage.checking => const StartingScreen(),
          AuthStage.signedOut => SignInScreen(auth: auth),
          AuthStage.secondStep => SecondStepScreen(auth: auth),
          AuthStage.noAccess => NoAccessScreen(onSignOut: auth.signOut),
          AuthStage.ready => KeysScreen(
            keys: app.keys!,
            coordinator: app.coordinator!,
            email: auth.email ?? '',
            onSignOut: onSignOut,
            onOpenSettings: app.system!.openSettings,
          ),
        };
        // Screens change in 360 ms; instantly under reduce-motion.
        return AnimatedSwitcher(
          duration: Motion.of(context, Motion.page),
          switchInCurve: Motion.curve,
          child: KeyedSubtree(key: ValueKey(auth.stage), child: screen),
        );
      },
    );
  }
}

enum _SignOutChoice { keepKeys, deleteKeys }

/// Signing out removes the credentials. Deleting the key pairs as well is permanent, and the
/// dialog says so before it can be chosen.
class SignOutDialog extends StatelessWidget {
  const SignOutDialog({super.key});

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final type = context.type;
    return Dialog(
      backgroundColor: c.paper,
      shape: RoundedRectangleBorder(side: BorderSide(color: c.gray200)),
      insetPadding: const EdgeInsets.all(16),
      child: SingleChildScrollView(
        padding: const EdgeInsets.all(20),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            Semantics(
              header: true,
              child: Text('Sign out?', style: type.heading),
            ),
            const SizedBox(height: 8),
            Text(
              'This phone stops unlocking the door until you sign in again. Its keys stay on the phone.',
              style: type.body,
            ),
            const SizedBox(height: 16),
            DooraButton(
              label: 'Sign out',
              kind: ButtonKind.primary,
              onPressed: () =>
                  Navigator.of(context).pop(_SignOutChoice.keepKeys),
            ),
            const SizedBox(height: 16),
            Text(
              "Deleting this phone's keys is permanent. A deleted key can never be set up again for the same virtual key.",
              style: type.body,
            ),
            const SizedBox(height: 10),
            DooraButton(
              label: 'Sign out and delete keys',
              kind: ButtonKind.destructive,
              onPressed: () =>
                  Navigator.of(context).pop(_SignOutChoice.deleteKeys),
            ),
            const SizedBox(height: 4),
            Center(
              child: DooraTextButton(
                label: 'Cancel',
                onPressed: () => Navigator.of(context).pop(),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
