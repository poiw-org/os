// An in-process door: the door's side of docs/ble-protocol.md, including a real ECDSA check, so
// the unlock flow is tested without hardware.
import 'dart:async';
import 'dart:math';
import 'dart:typed_data';

import 'package:doora/door/ble_protocol.dart';
import 'package:doora/door/door_unlocker.dart';
import 'package:doora/platform/device_key.dart';
import 'package:pointycastle/export.dart';

final _params = ECDomainParameters('secp256r1');

BigInt _int(List<int> bytes) =>
    bytes.fold(BigInt.zero, (acc, b) => (acc << 8) | BigInt.from(b));

Uint8List _bytes(BigInt value, int length) {
  final out = Uint8List(length);
  for (var i = length - 1; i >= 0; i--) {
    out[i] = (value & BigInt.from(0xff)).toInt();
    value >>= 8;
  }
  return out;
}

/// DER for r and s, with the sign byte DER requires.
Uint8List derEncode(BigInt r, BigInt s) {
  List<int> integer(BigInt v) {
    var bytes = _bytes(
      v,
      (v.bitLength + 7) ~/ 8 == 0 ? 1 : (v.bitLength + 7) ~/ 8,
    ).toList();
    if (bytes.first & 0x80 != 0) bytes = [0, ...bytes];
    return [0x02, bytes.length, ...bytes];
  }

  final body = [...integer(r), ...integer(s)];
  return Uint8List.fromList([0x30, body.length, ...body]);
}

/// A phone's keystore, in memory.
class FakeDeviceKey implements DeviceKey {
  final Map<String, ECPrivateKey> _private = {};
  final Map<String, Uint8List> _public = {};
  FakeDeviceKey({int seed = 7}) : _random = Random(seed);
  final Random _random;
  bool failSigning = false;

  @override
  Future<Uint8List> publicKey(String keyId) async {
    if (!_public.containsKey(keyId)) {
      final generator = ECKeyGenerator()
        ..init(
          ParametersWithRandom(
            ECKeyGeneratorParameters(_params),
            FortunaRandom()..seed(
              KeyParameter(
                Uint8List.fromList(
                  List.generate(32, (_) => _random.nextInt(256)),
                ),
              ),
            ),
          ),
        );
      final pair = generator.generateKeyPair();
      _private[keyId] = pair.privateKey;
      _public[keyId] = (pair.publicKey).Q!.getEncoded(false);
    }
    return _public[keyId]!;
  }

  @override
  Future<Uint8List> sign(String keyId, Uint8List message) async {
    if (failSigning) throw const DeviceKeyException('locked');
    await publicKey(keyId);
    final signer = ECDSASigner(SHA256Digest(), HMac(SHA256Digest(), 64))
      ..init(true, PrivateKeyParameter<ECPrivateKey>(_private[keyId]!));
    final signature = signer.generateSignature(message) as ECSignature;
    return derEncode(signature.r, signature.s);
  }

  @override
  Future<void> delete(String keyId) async {
    _private.remove(keyId);
    _public.remove(keyId);
  }

  @override
  Future<String> deviceName() async => 'Test phone';
}

/// How the fake door misbehaves.
enum DoorFault {
  none,
  neverConnects,
  silentAfterConnect,
  badChallenge,
  dropsAfterChallenge,
}

/// The door and its bridge. [verdict] is what it answers to a request whose proof checks out.
class FakeDoor implements DoorTransport {
  FakeDoor({
    this.doorId = 'main',
    this.verdict = 0,
    this.fault = DoorFault.none,
    this.mtu = 247,
    Uint8List? nonce,
  }) : nonce = nonce ?? Uint8List.fromList(List.generate(16, (i) => i + 1));

  final String doorId;
  int verdict;
  DoorFault fault;
  final int mtu;
  final Uint8List nonce;

  /// What the door saw, for assertions.
  Uint8List? lastBody;
  List<Uint8List> chunks = [];
  int connections = 0;
  int disconnects = 0;
  bool? proofValid;

  /// The phone's public key the door will check against (from the credential's `cnf`).
  Uint8List? expectedPublicKey;

  @override
  Future<DoorConnection> open(
    String deviceId, {
    required Duration timeout,
  }) async {
    connections++;
    if (fault == DoorFault.neverConnects) {
      await Future<void>.delayed(timeout + const Duration(milliseconds: 50));
      throw TimeoutException('connect');
    }
    final link = _Link(this);
    scheduleMicrotask(link.sendChallenge);
    return link;
  }

  Uint8List challengeBytes() => Uint8List.fromList([
    0x01,
    0x01,
    ...nonce,
    doorId.length,
    ...doorId.codeUnits,
  ]);

  void _verify(Uint8List body) {
    final signature = body.sublist(2, 66);
    final message = signedMessage(Challenge(nonce: nonce, doorId: doorId));
    final q = _params.curve.decodePoint(expectedPublicKey!)!;
    final verifier = ECDSASigner(SHA256Digest())
      ..init(false, PublicKeyParameter(ECPublicKey(q, _params)));
    proofValid = verifier.verifySignature(
      message,
      ECSignature(_int(signature.sublist(0, 32)), _int(signature.sublist(32))),
    );
  }
}

class _Link implements DoorConnection {
  _Link(this._door);
  final FakeDoor _door;
  final StreamController<List<int>> _out = StreamController<List<int>>();
  final BytesBuilder _body = BytesBuilder();
  int _next = 0;

  @override
  Stream<List<int>> get notifications => _out.stream;

  @override
  int get mtu => _door.mtu;

  void sendChallenge() {
    switch (_door.fault) {
      case DoorFault.silentAfterConnect:
        return;
      case DoorFault.badChallenge:
        _out.add([0x01, 0x02, 0, 0]);
      case DoorFault.dropsAfterChallenge:
        _out.add(_door.challengeBytes());
        _out.close();
      case _:
        _out.add(_door.challengeBytes());
    }
  }

  @override
  Future<void> write(Uint8List chunk) async {
    if (_out.isClosed) throw StateError('link closed');
    final dataSize = mtu - 3 - 1;
    if (chunk.length - 1 > dataSize) throw StateError('chunk exceeds the MTU');
    _door.chunks.add(chunk);
    final index = chunk[0] & 0x7f;
    if (index != _next++) throw StateError('out of order');
    _body.add(chunk.sublist(1));
    if (chunk[0] & 0x80 != 0) {
      final body = _body.toBytes();
      _door.lastBody = body;
      _door._verify(body);
      final code = _door.proofValid! ? _door.verdict : 3;
      scheduleMicrotask(() => _out.add([0x03, code]));
    }
  }

  @override
  Future<void> close() async {
    _door.disconnects++;
    if (!_out.isClosed) unawaited(_out.close());
  }
}
