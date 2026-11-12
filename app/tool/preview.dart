// Design preview: every screen with made-up data and no server.
// flutter run -t tool/preview.dart --dart-define=SCREEN=keys|signin|code|noaccess|sheet|signout
import 'dart:async';

import 'package:doora/auth/auth_backend.dart';
import 'package:doora/auth/auth_controller.dart';
import 'package:doora/auth/auth_screens.dart';
import 'package:doora/door/ble_door_transport.dart';
import 'package:doora/door/door_coordinator.dart';
import 'package:doora/door/door_notifier.dart';
import 'package:doora/door/door_scanner.dart';
import 'package:doora/door/door_unlocker.dart';
import 'package:doora/door/unlock_sheet.dart';
import 'package:doora/keys/credential_store.dart';
import 'package:doora/keys/key_backend.dart';
import 'package:doora/keys/key_controller.dart';
import 'package:doora/keys/keys_screen.dart';
import 'package:doora/keys/virtual_key.dart';
import 'package:doora/main.dart' show SignOutDialog;
import 'package:doora/platform/device_key.dart';
import 'package:doora/platform/system.dart';
import 'package:doora/theme/theme.dart';
import 'package:material_ui/material_ui.dart';

const _screen = String.fromEnvironment('SCREEN', defaultValue: 'keys');

void main() {
  WidgetsFlutterBinding.ensureInitialized();
  final now = DateTime.now();
  final keys = KeyController(
    backend: _Keys([
      VirtualKey(
        id: 'a',
        notBefore: now.subtract(const Duration(days: 30)),
        expiresAt: now.add(const Duration(days: 300)),
        disabled: false,
      ),
      VirtualKey(
        id: 'b',
        notBefore: now.subtract(const Duration(days: 90)),
        expiresAt: now.add(const Duration(days: 40)),
        disabled: false,
        deviceName: 'Pixel 8',
      ),
      VirtualKey(
        id: 'c',
        notBefore: now.add(const Duration(days: 10)),
        expiresAt: now.add(const Duration(days: 400)),
        disabled: false,
      ),
      VirtualKey(
        id: 'd',
        notBefore: now.subtract(const Duration(days: 400)),
        expiresAt: now.subtract(const Duration(days: 5)),
        disabled: false,
      ),
    ]),
    store: CredentialStore(),
    deviceKey: const PlatformDeviceKey(),
  );
  const system = PlatformSystemBridge();
  final coordinator = DoorCoordinator(
    keys: keys,
    scanner: DoorScanner(),
    unlocker: DoorUnlocker(
      transport: const BleDoorTransport(),
      deviceKey: const PlatformDeviceKey(),
    ),
    notifier: LocalDoorNotifier(),
    system: system,
    store: CredentialStore(),
  );
  final auth = AuthController(_Auth());
  if (_screen == 'code') unawaited(auth.signIn('member@poiw.org', 'x'));
  unawaited(keys.load());

  runApp(
    MaterialApp(
      debugShowCheckedModeBanner: false,
      theme: hostTheme(),
      builder: (context, child) =>
          DooraTheme(palette: Palette.dark, child: child!),
      home: Builder(
        builder: (context) => switch (_screen) {
          'signin' => SignInScreen(auth: auth),
          'code' => SecondStepScreen(auth: auth),
          'noaccess' => NoAccessScreen(onSignOut: () {}),
          'sheet' || 'signout' => _Launcher(),
          _ => KeysScreen(
            keys: keys,
            coordinator: coordinator,
            email: 'member@poiw.org',
            onSignOut: () {},
            onOpenSettings: () {},
          ),
        },
      ),
    ),
  );
}

class _Launcher extends StatefulWidget {
  @override
  State<_Launcher> createState() => _LauncherState();
}

class _LauncherState extends State<_Launcher> {
  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (_screen == 'signout') {
        showDialog<void>(
          context: context,
          builder: (_) => const SignOutDialog(),
        );
        return;
      }
      showUnlockSheet(
        context,
        initialDoorName: 'Front door',
        doorNameFor: (_) => 'Front door',
        keyExpiresAt: DateTime.now().add(const Duration(days: 300)),
        unlock: (_) async {
          await Future<void>.delayed(const Duration(seconds: 2));
          return const Opened();
        },
      );
    });
  }

  @override
  Widget build(BuildContext context) =>
      const StartingScreen(message: 'Behind the sheet.');
}

class _Keys implements KeyBackend {
  _Keys(this.keys);
  final List<VirtualKey> keys;

  @override
  Future<List<VirtualKey>> fetchKeys() async => keys;

  @override
  Future<List<DoorInfo>> fetchDoors() async => const [
    DoorInfo(id: 'front', name: 'Front door'),
  ];

  @override
  Future<StoredCredential> credential({
    required String keyId,
    required String devicePubkey,
    required String deviceName,
  }) => throw const KeyBackendException('Preview only.');
}

class _Auth implements AuthBackend {
  @override
  bool get hasSession => false;
  @override
  String? get email => 'member@poiw.org';
  @override
  Stream<void> get sessionEnded => const Stream.empty();
  @override
  Future<void> signInWithPassword(String email, String password) async {}
  @override
  Future<SecondStep?> pendingSecondStep() async => SecondStep.authenticator;
  @override
  Future<void> sendEmailCode(String email) async {}
  @override
  Future<void> confirmEmailCode(String code) async {}
  @override
  Future<void> verifyAuthenticatorCode(String code) async {}
  @override
  Future<bool> canUseApp() async => true;
  @override
  Future<void> signOut() async {}
}
