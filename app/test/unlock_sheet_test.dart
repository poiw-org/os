import 'dart:async';

import 'package:doora/door/ble_protocol.dart';
import 'package:doora/door/door_unlocker.dart';
import 'package:doora/door/unlock_sheet.dart';
import 'package:doora/theme/theme.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:material_ui/material_ui.dart';

Widget host(
  Future<UnlockOutcome> Function(void Function(String)) unlock, {
  bool auto = false,
}) => MaterialApp(
  theme: hostTheme(),
  builder: (c, child) => DooraTheme(palette: Palette.dark, child: child!),
  home: Builder(
    builder: (context) => Center(
      child: TextButton(
        onPressed: () => showUnlockSheet(
          context,
          initialDoorName: 'Door',
          doorNameFor: (id) => id == 'main' ? 'Main door' : null,
          keyExpiresAt: DateTime(2030),
          unlock: unlock,
          autoStart: auto,
        ),
        child: const Text('open'),
      ),
    ),
  ),
);

void main() {
  testWidgets('ready, unlocking, then door open and closes itself', (
    tester,
  ) async {
    final done = Completer<UnlockOutcome>();
    await tester.pumpWidget(
      host((onDoorId) {
        onDoorId('main');
        return done.future;
      }),
    );
    await tester.tap(find.text('open'));
    await tester.pumpAndSettle();
    expect(find.text('UNLOCK'), findsOneWidget);
    expect(find.text('NOT NOW'), findsNothing); // text button, not uppercased
    expect(find.text('Not now'), findsOneWidget);

    await tester.tap(find.text('UNLOCK'));
    await tester.pump();
    expect(find.text('UNLOCKING…'), findsOneWidget);
    expect(find.text('Main door'), findsOneWidget);

    done.complete(const Opened());
    await tester.pump();
    expect(find.text('Door open'), findsOneWidget);
    await tester.pump(const Duration(milliseconds: 1600));
    await tester.pumpAndSettle();
    expect(find.text('Door open'), findsNothing);
  });

  testWidgets('a refusal shows the door\'s sentence and Try again', (
    tester,
  ) async {
    await tester.pumpWidget(
      host((_) async => const Refused(DoorResult(8)), auto: true),
    );
    await tester.tap(find.text('open'));
    await tester.pumpAndSettle();
    expect(find.text('The door is busy. Try again.'), findsOneWidget);
    expect(find.text('TRY AGAIN'), findsOneWidget);
  });

  testWidgets('a refusal that cannot be retried offers Close only', (
    tester,
  ) async {
    await tester.pumpWidget(
      host((_) async => const Refused(DoorResult(4)), auto: true),
    );
    await tester.tap(find.text('open'));
    await tester.pumpAndSettle();
    expect(find.text('This key has been disabled.'), findsOneWidget);
    expect(find.text('TRY AGAIN'), findsNothing);
    expect(find.text('CLOSE'), findsOneWidget);
  });

  testWidgets('the sheet cannot be dismissed while unlocking', (tester) async {
    final done = Completer<UnlockOutcome>();
    await tester.pumpWidget(host((_) => done.future));
    await tester.tap(find.text('open'));
    await tester.pumpAndSettle();
    await tester.tap(find.text('UNLOCK'));
    await tester.pump();
    await tester.tapAt(const Offset(200, 40)); // the scrim
    await tester.pump(
      const Duration(milliseconds: 500),
    ); // the lamp breathes forever: no settle
    expect(find.text('UNLOCKING…'), findsOneWidget);
    done.complete(const Opened());
    await tester.pump(const Duration(seconds: 2));
    await tester.pumpAndSettle();
  });
}
