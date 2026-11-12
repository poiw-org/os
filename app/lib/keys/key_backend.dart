import 'dart:async';

import 'package:supabase_flutter/supabase_flutter.dart';

import 'credential_store.dart';
import 'virtual_key.dart';

/// Thrown when the backend answered and said no; [message] is a sentence for the member.
class KeyBackendException implements Exception {
  const KeyBackendException(this.message, {this.conflict = false});
  final String message;

  /// The key is already bound to another phone (409): permanent.
  final bool conflict;

  @override
  String toString() => message;
}

/// Thrown when the server could not be reached.
class OfflineException implements Exception {
  const OfflineException();
}

abstract class KeyBackend {
  /// The member's virtual keys (`serial_number is null`; row-level security returns their own).
  Future<List<VirtualKey>> fetchKeys();

  /// `public.doors`, which row-level security shows to admins only.
  Future<List<DoorInfo>> fetchDoors();

  /// `ble-credential`: binds [devicePubkey] to [keyId] or, for the same device, returns the current
  /// credential.
  Future<StoredCredential> credential({
    required String keyId,
    required String devicePubkey,
    required String deviceName,
  });
}

class SupabaseKeyBackend implements KeyBackend {
  SupabaseKeyBackend(this._client);
  final SupabaseClient _client;

  /// A request that got no answer is offline; one that got an answer is a real error.
  Future<T> _guard<T>(Future<T> Function() run) async {
    try {
      return await run().timeout(const Duration(seconds: 15));
    } on PostgrestException catch (e) {
      throw KeyBackendException(_postgrestSentence(e));
    } on FunctionException catch (e) {
      throw KeyBackendException(
        _functionSentence(e),
        conflict: e.status == 409,
      );
    } on AuthException catch (e) {
      if (e is AuthRetryableFetchException) throw const OfflineException();
      throw const KeyBackendException('Your session has ended. Sign in again.');
    } on TimeoutException {
      throw const OfflineException();
    } on KeyBackendException {
      rethrow;
    } on Exception {
      throw const OfflineException();
    }
  }

  static String _postgrestSentence(PostgrestException e) =>
      e.code == '42501' || e.code == 'PGRST301'
      ? 'Your session has ended. Sign in again.'
      : 'Could not load your keys. Try again.';

  static String _functionSentence(FunctionException e) {
    final details = e.details;
    final reason = details is Map ? details['error'] : null;
    if (reason is String && reason.isNotEmpty) return reason;
    return switch (e.status) {
      409 => 'This key is set up on another phone.',
      403 || 404 => 'This key cannot be set up on a phone.',
      _ => 'Something went wrong. Try again.',
    };
  }

  @override
  Future<List<VirtualKey>> fetchKeys() => _guard(() async {
    // Row-level security lets admins read every key, so ask for this member's own.
    final userId = _client.auth.currentUser?.id;
    if (userId == null) return const <VirtualKey>[];
    final rows = await _client
        .from('keys')
        .select('id, not_before, expires_at, disabled, device_name')
        .eq('user_id', userId)
        .filter('serial_number', 'is', null);
    return [for (final row in rows) VirtualKey.fromRow(row)];
  });

  @override
  Future<List<DoorInfo>> fetchDoors() => _guard(() async {
    final rows = await _client.from('doors').select('id, name');
    return [
      for (final row in rows)
        DoorInfo(id: row['id'] as String, name: row['name'] as String?),
    ];
  });

  @override
  Future<StoredCredential> credential({
    required String keyId,
    required String devicePubkey,
    required String deviceName,
  }) => _guard(() async {
    final response = await _client.functions.invoke(
      'ble-credential',
      body: {
        'key_id': keyId,
        'device_pubkey': devicePubkey,
        'device_name': deviceName,
      },
    );
    final data = response.data;
    if (data is! Map) {
      throw const KeyBackendException('Something went wrong. Try again.');
    }
    return StoredCredential(
      keyId: keyId,
      jwt: data['jwt'] as String,
      notBefore: DateTime.parse(data['not_before'] as String),
      expiresAt: DateTime.parse(data['expires_at'] as String),
    );
  });
}
