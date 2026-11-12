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

/// The door keeps turning, so nothing ever settles: pump past the card's entrance instead.
Future<void> settle(WidgetTester tester) async {
  for (var i = 0; i < 10; i++) {
    await tester.pump(const Duration(milliseconds: 100));
  }
}

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
    await settle(tester);
    expect(find.text('Unlock'), findsOneWidget);
    expect(find.bySemanticsLabel('Close'), findsOneWidget);

    await tester.tap(find.text('Unlock'));
    await tester.pump();
    expect(find.text('Unlocking…'), findsOneWidget);
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
    await settle(tester);
    expect(find.text('The door is busy. Try again.'), findsOneWidget);
    expect(find.text('Try again'), findsOneWidget);
  });

  testWidgets('a refusal that cannot be retried offers Close only', (
    tester,
  ) async {
    await tester.pumpWidget(
      host((_) async => const Refused(DoorResult(4)), auto: true),
    );
    await tester.tap(find.text('open'));
    await settle(tester);
    expect(find.text('This key has been disabled.'), findsOneWidget);
    expect(find.text('Try again'), findsNothing);
    expect(find.text('Close'), findsOneWidget);
  });

  testWidgets('the sheet cannot be dismissed while unlocking', (tester) async {
    final done = Completer<UnlockOutcome>();
    await tester.pumpWidget(host((_) => done.future));
    await tester.tap(find.text('open'));
    await settle(tester);
    await tester.tap(find.text('Unlock'));
    await tester.pump();
    await tester.tapAt(const Offset(200, 40)); // the scrim
    await tester.pump(
      const Duration(milliseconds: 500),
    ); // the door turns forever: no settle
    expect(find.text('Unlocking…'), findsOneWidget);
    done.complete(const Opened());
    await tester.pump(const Duration(seconds: 2));
    await tester.pumpAndSettle();
  });
}
