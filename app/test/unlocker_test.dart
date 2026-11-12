import 'dart:convert';
import 'dart:typed_data';

import 'package:doora/door/ble_protocol.dart';
import 'package:doora/door/door_unlocker.dart';
import 'package:doora/keys/credential_store.dart';
import 'package:flutter_test/flutter_test.dart';

import 'support/fake_door.dart';

void main() {
  late FakeDeviceKey deviceKey;
  late FakeDoor door;
  late DoorUnlocker unlocker;
  late StoredCredential credential;

  final jwt = '${'a' * 400}.${'b' * 300}.${'c' * 100}';

  setUp(() async {
    deviceKey = FakeDeviceKey();
    door = FakeDoor();
    door.expectedPublicKey = await deviceKey.publicKey('key-1');
    unlocker = DoorUnlocker(
      transport: door,
      deviceKey: deviceKey,
      connectTimeout: const Duration(milliseconds: 200),
      overall: const Duration(seconds: 3),
    );
    credential = StoredCredential(
      keyId: 'key-1',
      jwt: jwt,
      notBefore: DateTime(2020),
      expiresAt: DateTime(2100),
    );
  });

  Future<UnlockOutcome> run({void Function(String)? onDoorId}) => unlocker
      .unlock(deviceId: 'AA:BB', credential: credential, onDoorId: onDoorId);

  test('opens the door with a signature the door verifies', () async {
    String? seen;
    final outcome = await run(onDoorId: (id) => seen = id);
    expect(outcome, isA<Opened>());
    expect(door.proofValid, isTrue);
    expect(seen, 'main');
    expect(door.disconnects, 1);
    final body = door.lastBody!;
    expect(body.sublist(0, 2), [0x02, 0x01]);
    expect(utf8.decode(body.sublist(66)), jwt);
  });

  test('chunks fit the MTU, in order, last one flagged', () async {
    for (final mtu in [23, 185, 247]) {
      final d = FakeDoor(mtu: mtu)..expectedPublicKey = door.expectedPublicKey;
      final u = DoorUnlocker(transport: d, deviceKey: deviceKey);
      expect(
        await u.unlock(deviceId: 'x', credential: credential),
        isA<Opened>(),
      );
      expect(d.chunks.last[0] & 0x80, 0x80);
      expect(d.chunks.every((c) => c.length <= mtu - 3), isTrue);
    }
  });

  test('every result code gives its sentence and the right retry', () async {
    for (var code = 1; code <= 8; code++) {
      door.verdict = code;
      final outcome = await run();
      expect(outcome, isA<Refused>());
      final refused = outcome as Refused;
      expect(refused.sentence, DoorResult(code).sentence);
      expect(refused.canRetry, {1, 7, 8}.contains(code));
    }
    expect(
      door.connections,
      8,
      reason: 'never retried after the door answered',
    );
  });

  test('a signature over another nonce is refused with code 3', () async {
    door.expectedPublicKey = await FakeDeviceKey(seed: 99).publicKey('other');
    final outcome = await run();
    expect((outcome as Refused).result.code, 3);
  });

  test('a malformed challenge fails without a request', () async {
    door.fault = DoorFault.badChallenge;
    final outcome = await run();
    expect((outcome as Failed).kind, FailureKind.protocol);
    expect(door.lastBody, isNull);
    expect(door.disconnects, 1);
  });

  test(
    'a link that never connects times out, retries once, and disconnects',
    () async {
      door.fault = DoorFault.neverConnects;
      final outcome = await run();
      expect((outcome as Failed).kind, FailureKind.timeout);
      expect(door.connections, 2);
    },
  );

  test('a link that drops before the request is retried once', () async {
    door.fault = DoorFault.dropsAfterChallenge;
    final outcome = await run();
    expect((outcome as Failed).kind, FailureKind.connection);
    expect(door.connections, 2);
    expect(door.disconnects, 2);
  });

  test('a silent door ends within the overall limit', () async {
    door.fault = DoorFault.silentAfterConnect;
    final u = DoorUnlocker(
      transport: door,
      deviceKey: deviceKey,
      overall: const Duration(milliseconds: 400),
    );
    final watch = Stopwatch()..start();
    final outcome = await u.unlock(deviceId: 'x', credential: credential);
    expect(outcome, isA<Failed>());
    expect(watch.elapsed, lessThan(const Duration(seconds: 2)));
    expect(door.disconnects, door.connections);
  });

  test('a keystore that cannot sign fails without sending anything', () async {
    deviceKey.failSigning = true;
    final outcome = await run();
    expect((outcome as Failed).kind, FailureKind.deviceKey);
    expect(door.lastBody, isNull);
  });

  test('derToRaw handles short and long integers and leading zeros', () {
    Uint8List der(List<int> r, List<int> s) => Uint8List.fromList([
      0x30,
      r.length + s.length + 4,
      0x02,
      r.length,
      ...r,
      0x02,
      s.length,
      ...s,
    ]);
    final short = der([0x05], [0x80 - 0x80 + 0x7f]);
    final raw = derToRaw(short);
    expect(raw.length, 64);
    expect(raw[31], 5);
    expect(raw[63], 0x7f);
    final long = der(
      [0x00, 0xff, ...List.filled(31, 1)],
      [0x7f, ...List.filled(31, 2)],
    );
    final rawLong = derToRaw(long);
    expect(rawLong[0], 0xff);
    expect(rawLong[32], 0x7f);
    expect(() => derToRaw([0x31, 0]), throwsA(isA<ProtocolException>()));
  });

  test('the signed message is prefix, id length, id, nonce', () {
    final challenge = Challenge(
      nonce: Uint8List.fromList(List.filled(16, 9)),
      doorId: 'main',
    );
    final bytes = signedMessage(challenge);
    expect(utf8.decode(bytes.sublist(0, 11)), 'doora-ble-1');
    expect(bytes[11], 4);
    expect(utf8.decode(bytes.sublist(12, 16)), 'main');
    expect(bytes.sublist(16), List.filled(16, 9));
  });
}
