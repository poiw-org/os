import 'package:flutter/services.dart';

/// A P-256 key pair per virtual key, held in the Secure Enclave or Android Keystore. The private
/// key never leaves it.
abstract class DeviceKey {
  /// 65 bytes, `04 || X || Y`. Creates the pair on first use.
  Future<Uint8List> publicKey(String keyId);

  /// The DER ECDSA signature (SHA-256) of [message].
  Future<Uint8List> sign(String keyId, Uint8List message);

  /// Deletes the pair. A deleted pair can never be set up again for that key.
  Future<void> delete(String keyId);

  /// The user-visible device name, sent with the public key so an admin can tell phones apart.
  Future<String> deviceName();
}

class DeviceKeyException implements Exception {
  const DeviceKeyException(this.message);
  final String message;

  @override
  String toString() => 'DeviceKeyException: $message';
}

/// The platform channel `org.poiw.doora/device_key`, implemented in DeviceKey.swift and DeviceKey.kt.
class PlatformDeviceKey implements DeviceKey {
  const PlatformDeviceKey();

  static const MethodChannel _channel = MethodChannel(
    'org.poiw.doora/device_key',
  );

  Future<T> _call<T>(String method, [Map<String, Object?>? arguments]) async {
    try {
      final result = await _channel.invokeMethod<T>(method, arguments);
      if (result == null && method != 'delete') {
        throw DeviceKeyException('$method returned nothing');
      }
      return result as T;
    } on PlatformException catch (e) {
      // The message is the platform's reason; it never contains key material.
      throw DeviceKeyException('${e.code}: ${e.message ?? 'failed'}');
    } on MissingPluginException {
      throw const DeviceKeyException(
        'The device key is not available on this platform',
      );
    }
  }

  @override
  Future<Uint8List> publicKey(String keyId) async {
    final bytes = await _call<Uint8List>('publicKey', {'keyId': keyId});
    if (bytes.length != 65 || bytes[0] != 0x04) {
      throw const DeviceKeyException(
        'The public key is not an uncompressed P-256 point',
      );
    }
    return bytes;
  }

  @override
  Future<Uint8List> sign(String keyId, Uint8List message) =>
      _call<Uint8List>('sign', {'keyId': keyId, 'message': message});

  @override
  Future<void> delete(String keyId) => _call<void>('delete', {'keyId': keyId});

  @override
  Future<String> deviceName() async {
    final name = (await _call<String>('deviceName')).trim();
    if (name.isEmpty) return 'Phone';
    return name.length > 64 ? name.substring(0, 64) : name;
  }
}
