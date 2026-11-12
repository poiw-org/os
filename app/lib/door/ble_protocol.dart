// The phone side of docs/ble-protocol.md. Pure Dart: no Flutter, no Bluetooth, so it runs in unit
// tests against the shared test vectors.
import 'dart:convert';
import 'dart:typed_data';

const String doorServiceUuid = '7d0a0001-6f2b-4c1e-9a53-d00a00000001';
const String doorCharacteristicUuid = '7d0a0002-6f2b-4c1e-9a53-d00a00000001';

const int _challengeType = 0x01;
const int _requestType = 0x02;
const int _resultType = 0x03;
const int _version = 0x01;

const int nonceLength = 16;
const int maxDoorIdLength = 31;
const int signatureLength = 64;

/// The bridge refuses a reassembled request above this size.
const int maxRequestLength = 1024;

/// Largest credential the door accepts (`kMaxTokenLen`).
const int maxTokenLength = 860;

/// ATT adds 3 bytes of header to every write; the chunk header is one more.
const int _attOverhead = 3;

final Uint8List _signedPrefix = Uint8List.fromList(ascii.encode('doora-ble-1'));

/// The door sent something this app cannot read, or built something the door would refuse.
class ProtocolException implements Exception {
  const ProtocolException(this.message);
  final String message;

  @override
  String toString() => 'ProtocolException: $message';
}

/// The door's opening message: who it is and the nonce to sign.
class Challenge {
  const Challenge({required this.nonce, required this.doorId});

  final Uint8List nonce;

  /// The door's id as sent (ASCII, 1..31 bytes).
  final String doorId;

  /// `0x01 | version | nonce[16] | door_id_len | door_id`
  factory Challenge.parse(List<int> bytes) {
    if (bytes.length < 3 + nonceLength) {
      throw const ProtocolException('Challenge too short');
    }
    if (bytes[0] != _challengeType) {
      throw const ProtocolException('Not a challenge');
    }
    if (bytes[1] != _version) {
      throw const ProtocolException('Unknown challenge version');
    }
    final idLength = bytes[2 + nonceLength];
    if (idLength < 1 || idLength > maxDoorIdLength) {
      throw const ProtocolException('Bad door id length');
    }
    if (bytes.length != 3 + nonceLength + idLength) {
      throw const ProtocolException('Challenge has the wrong length');
    }
    final idBytes = bytes.sublist(3 + nonceLength);
    if (idBytes.any((b) => b < 0x20 || b > 0x7e)) {
      throw const ProtocolException('Door id is not ASCII');
    }
    return Challenge(
      nonce: Uint8List.fromList(bytes.sublist(2, 2 + nonceLength)),
      doorId: ascii.decode(idBytes),
    );
  }
}

/// What the key pair signs: `"doora-ble-1" | door_id_len | door_id | nonce[16]`. The keystore
/// hashes it with SHA-256.
Uint8List signedMessage(Challenge challenge) {
  final id = ascii.encode(challenge.doorId);
  return Uint8List.fromList([
    ..._signedPrefix,
    id.length,
    ...id,
    ...challenge.nonce,
  ]);
}

/// Converts the DER ECDSA signature a keystore returns into the raw `r || s` (32 bytes each) the
/// door accepts.
Uint8List derToRaw(List<int> der) {
  var i = 0;
  int readByte() {
    if (i >= der.length) throw const ProtocolException('Truncated signature');
    return der[i++];
  }

  int readLength() {
    final first = readByte();
    if (first < 0x80) return first;
    final count = first & 0x7f;
    if (count == 0 || count > 2) {
      throw const ProtocolException('Unsupported DER length');
    }
    var value = 0;
    for (var k = 0; k < count; k++) {
      value = (value << 8) | readByte();
    }
    return value;
  }

  Uint8List readInteger() {
    if (readByte() != 0x02) {
      throw const ProtocolException('Expected a DER integer');
    }
    final length = readLength();
    if (length < 1 || i + length > der.length) {
      throw const ProtocolException('Bad DER integer length');
    }
    var value = der.sublist(i, i + length);
    i += length;
    // Strip the sign byte and any redundant leading zeros.
    var start = 0;
    while (start < value.length - 1 && value[start] == 0) {
      start++;
    }
    value = value.sublist(start);
    if (value.length > 32) {
      throw const ProtocolException('Signature integer too large');
    }
    final padded = Uint8List(32);
    padded.setRange(32 - value.length, 32, value);
    return padded;
  }

  if (readByte() != 0x30) {
    throw const ProtocolException('Expected a DER sequence');
  }
  final sequenceLength = readLength();
  if (i + sequenceLength != der.length) {
    throw const ProtocolException('Bad DER length');
  }
  final r = readInteger();
  final s = readInteger();
  if (i != der.length) {
    throw const ProtocolException('Trailing bytes after signature');
  }
  return Uint8List.fromList([...r, ...s]);
}

/// The request body: `0x02 | version | signature[64] | jwt`.
Uint8List requestBody({required Uint8List signature, required String jwt}) {
  if (signature.length != signatureLength) {
    throw const ProtocolException('Signature must be 64 bytes');
  }
  final token = ascii.encode(jwt);
  if (token.length > maxTokenLength) {
    throw const ProtocolException('Credential is too long');
  }
  final body = Uint8List.fromList([
    _requestType,
    _version,
    ...signature,
    ...token,
  ]);
  if (body.length > maxRequestLength) {
    throw const ProtocolException('Request is too long');
  }
  return body;
}

/// Splits a request body into the writes the bridge reassembles: `header | data`, where bit 7 of
/// the header marks the last chunk and bits 0-6 are the index from 0. Each write fits the
/// negotiated [mtu] (ATT payload = mtu - 3).
List<Uint8List> chunkRequest(Uint8List body, int mtu) {
  final dataSize = mtu - _attOverhead - 1;
  if (dataSize < 1) throw const ProtocolException('MTU too small');
  final count = (body.length + dataSize - 1) ~/ dataSize;
  if (count == 0 || count > 128) {
    throw const ProtocolException('Request does not fit in chunks');
  }
  return [
    for (var index = 0; index < count; index++)
      Uint8List.fromList([
        index | (index == count - 1 ? 0x80 : 0),
        ...body.sublist(
          index * dataSize,
          (index + 1) * dataSize > body.length
              ? body.length
              : (index + 1) * dataSize,
        ),
      ]),
  ];
}

/// The door's answer.
class DoorResult {
  const DoorResult(this.code);

  /// `0x03 | code`
  factory DoorResult.parse(List<int> bytes) {
    if (bytes.length != 2 || bytes[0] != _resultType) {
      throw const ProtocolException('Not a result');
    }
    return DoorResult(bytes[1]);
  }

  final int code;

  bool get opened => code == 0;

  /// Worth offering "Try again": malformed (1), no public key yet (7), busy (8).
  bool get canRetry => code == 1 || code == 7 || code == 8;

  /// The app's sentence for [code] (protocol section 3).
  String get sentence => switch (code) {
    0 => 'Door open',
    1 => 'Something went wrong. Try again.',
    2 => "This key isn't valid on this door.",
    3 => "This phone couldn't prove it holds the key.",
    4 => 'This key has been disabled.',
    5 => 'This key has expired.',
    6 => "This key isn't active yet.",
    7 => "The door isn't ready yet. Try again in a minute.",
    8 => 'The door is busy. Try again.',
    _ => 'Something went wrong. Try again.',
  };
}

/// base64url without padding, as the backend expects for `device_pubkey`.
String base64UrlNoPad(List<int> bytes) =>
    base64Url.encode(bytes).replaceAll('=', '');

/// Decodes base64url with or without padding.
Uint8List base64UrlDecode(String text) =>
    base64Url.decode(base64Url.normalize(text));
