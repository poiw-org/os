import 'dart:async';
import 'dart:io' show Platform;
import 'dart:typed_data';

import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import 'ble_protocol.dart';
import 'door_unlocker.dart';

/// The real transport: flutter_blue_plus talking to the bridge's characteristic.
class BleDoorTransport implements DoorTransport {
  const BleDoorTransport();

  static final Guid _service = Guid(doorServiceUuid);
  static final Guid _characteristic = Guid(doorCharacteristicUuid);

  /// The protocol asks Android for 247; iOS negotiates by itself.
  static const int _androidMtu = 247;

  @override
  Future<DoorConnection> open(
    String deviceId, {
    required Duration timeout,
  }) async {
    final device = BluetoothDevice.fromId(deviceId);
    final stopwatch = Stopwatch()..start();
    Duration left() {
      final remaining = timeout - stopwatch.elapsed;
      return remaining > Duration.zero
          ? remaining
          : const Duration(milliseconds: 1);
    }

    final link = _BleDoorConnection(device);
    try {
      await device.connect(
        license: License.nonprofit,
        timeout: timeout,
        mtu: Platform.isAndroid ? _androidMtu : null,
      );
      link.watchDisconnect();

      final services = await device.discoverServices(
        timeout: left().inSeconds.clamp(1, 15),
      );
      BluetoothCharacteristic? found;
      for (final service in services) {
        if (service.uuid != _service) continue;
        for (final characteristic in service.characteristics) {
          if (characteristic.uuid == _characteristic) found = characteristic;
        }
      }
      if (found == null) {
        throw StateError('The door service is not on this device');
      }

      // Listen before subscribing: the challenge arrives right after.
      link.attach(found);
      await found.setNotifyValue(true, timeout: left().inSeconds.clamp(1, 15));
      return link;
    } on Object {
      await link.close();
      rethrow;
    }
  }
}

class _BleDoorConnection implements DoorConnection {
  _BleDoorConnection(this._device);

  final BluetoothDevice _device;
  final StreamController<List<int>> _messages = StreamController<List<int>>();
  BluetoothCharacteristic? _characteristic;
  StreamSubscription<List<int>>? _valueSubscription;
  StreamSubscription<BluetoothConnectionState>? _stateSubscription;

  @override
  Stream<List<int>> get notifications => _messages.stream;

  @override
  int get mtu => _device.mtuNow;

  void attach(BluetoothCharacteristic characteristic) {
    _characteristic = characteristic;
    _valueSubscription = characteristic.onValueReceived.listen((value) {
      if (!_messages.isClosed) _messages.add(value);
    });
  }

  /// A link that drops ends the notification stream, which the unlocker reads as a disconnect.
  void watchDisconnect() {
    _stateSubscription = _device.connectionState.listen((state) {
      if (state == BluetoothConnectionState.disconnected &&
          !_messages.isClosed) {
        _messages.close();
      }
    });
  }

  @override
  Future<void> write(Uint8List chunk) {
    final characteristic = _characteristic;
    if (characteristic == null) throw StateError('Not connected');
    return characteristic.write(chunk, withoutResponse: false);
  }

  @override
  Future<void> close() {
    _valueSubscription?.cancel();
    _stateSubscription?.cancel();
    if (!_messages.isClosed) _messages.close();
    // Disconnecting can wait seconds on Android; the result must not wait for it.
    unawaited(_device.disconnect().then((_) {}, onError: (_) {}));
    return Future.value();
  }
}
