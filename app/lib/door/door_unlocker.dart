// The unlock sequence of docs/ble-protocol.md, over an abstract transport so the same code runs
// against Bluetooth on a phone and against an in-process fake door in tests.
import 'dart:async';
import 'dart:typed_data';

import '../keys/credential_store.dart';
import '../platform/device_key.dart';
import 'ble_protocol.dart';

/// An open, subscribed link to the door's characteristic.
abstract class DoorConnection {
  /// Notifications from the door, in order, buffered from the moment the link is open (the
  /// challenge arrives right after subscribing).
  Stream<List<int>> get notifications;

  /// The negotiated ATT MTU. Each write carries at most `mtu - 3` bytes.
  int get mtu;

  /// One write with response.
  Future<void> write(Uint8List chunk);

  /// Disconnects. Never throws.
  Future<void> close();
}

/// Opens connections to doors.
abstract class DoorTransport {
  /// Connects, negotiates the MTU (Android: 247), finds the characteristic and subscribes. Throws
  /// [TimeoutException] when [timeout] passes, and anything else on a connection-level failure.
  Future<DoorConnection> open(String deviceId, {required Duration timeout});
}

sealed class UnlockOutcome {
  const UnlockOutcome();
}

/// The door answered 0. A remote open confirms first and unlocks [after] later.
class Opened extends UnlockOutcome {
  const Opened({this.after = Duration.zero});
  final Duration after;
}

/// The door answered and said no.
class Refused extends UnlockOutcome {
  const Refused(this.result);
  final DoorResult result;

  String get sentence => result.sentence;
  bool get canRetry => result.canRetry;
}

enum FailureKind {
  connection,
  timeout,
  protocol,
  deviceKey,
  noKey,
  notConfirmed,
}

/// The server could not open the door remotely; [sentence] is its reason, for the member.
class RemoteFailed extends UnlockOutcome {
  const RemoteFailed(this.sentence);
  final String sentence;

  bool get canRetry => true;
}

/// The unlock never reached an answer.
class Failed extends UnlockOutcome {
  const Failed(this.kind);
  final FailureKind kind;

  String get sentence => switch (kind) {
    FailureKind.connection || FailureKind.timeout =>
      "Couldn't reach the door. Move closer and try again.",
    FailureKind.protocol => 'The door sent something unexpected. Try again.',
    FailureKind.deviceKey => "This phone couldn't use its key. Try again.",
    FailureKind.noKey => 'This phone has no key that works right now.',
    FailureKind.notConfirmed => "The unlock wasn't confirmed.",
  };

  bool get canRetry => kind != FailureKind.noKey;
}

class DoorUnlocker {
  DoorUnlocker({
    required this.transport,
    required this.deviceKey,
    this.connectTimeout = const Duration(seconds: 5),
    this.overall = const Duration(seconds: 10),
  });

  final DoorTransport transport;
  final DeviceKey deviceKey;
  final Duration connectTimeout;

  /// The whole operation, both attempts included.
  final Duration overall;

  /// Runs the sequence. [onDoorId] reports the door's id as soon as its challenge is read. One
  /// automatic retry on a connection-level failure before the request was sent, never after the
  /// door has answered. Always disconnects.
  Future<UnlockOutcome> unlock({
    required String deviceId,
    required StoredCredential credential,
    void Function(String doorId)? onDoorId,
  }) async {
    final clock = Stopwatch()..start();
    Duration remaining() => overall - clock.elapsed;

    var outcome = await _attempt(deviceId, credential, remaining, onDoorId);
    if (outcome is _Retry && remaining() > const Duration(seconds: 2)) {
      outcome = await _attempt(deviceId, credential, remaining, onDoorId);
    }
    return switch (outcome) {
      _Retry(:final failure) => failure,
      final UnlockOutcome done => done,
    };
  }

  Future<UnlockOutcome> _attempt(
    String deviceId,
    StoredCredential credential,
    Duration Function() remaining,
    void Function(String doorId)? onDoorId,
  ) async {
    DoorConnection? connection;
    StreamIterator<List<int>>? messages;
    var requestSent = false;

    Duration within(Duration cap) {
      final left = remaining();
      if (left <= Duration.zero) {
        throw TimeoutException('The unlock took too long');
      }
      return left < cap ? left : cap;
    }

    try {
      connection = await transport.open(
        deviceId,
        timeout: within(connectTimeout),
      );
      final iterator = messages = StreamIterator(connection.notifications);

      Future<List<int>> next() async {
        final more = await iterator.moveNext().timeout(within(overall));
        if (!more) throw const _Disconnected();
        return iterator.current;
      }

      final challenge = Challenge.parse(await next());
      onDoorId?.call(challenge.doorId);

      final Uint8List signature;
      try {
        signature = derToRaw(
          await deviceKey.sign(credential.keyId, signedMessage(challenge)),
        );
      } on DeviceKeyException {
        return const Failed(FailureKind.deviceKey);
      }

      final body = requestBody(signature: signature, jwt: credential.jwt);
      for (final chunk in chunkRequest(body, connection.mtu)) {
        await connection.write(chunk).timeout(within(overall));
      }
      requestSent = true;

      final result = DoorResult.parse(await next());
      return result.opened ? const Opened() : Refused(result);
    } on TimeoutException {
      final failure = const Failed(FailureKind.timeout);
      return requestSent ? failure : _Retry(failure);
    } on ProtocolException {
      return const Failed(FailureKind.protocol);
    } on _Disconnected {
      final failure = const Failed(FailureKind.connection);
      return requestSent ? failure : _Retry(failure);
    } catch (_) {
      // A connection-level failure: the link, the stack or the radio. Details never reach the UI.
      final failure = const Failed(FailureKind.connection);
      return requestSent ? failure : _Retry(failure);
    } finally {
      await connection?.close();
      unawaited(messages?.cancel());
    }
  }
}

class _Disconnected implements Exception {
  const _Disconnected();
}

/// Internal: a failure worth one more attempt.
class _Retry extends UnlockOutcome {
  const _Retry(this.failure);
  final Failed failure;
}
