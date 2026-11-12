// What the app asks of the account system. The Supabase implementation mirrors web/src/lib/auth.ts;
// tests use a fake.
import 'dart:async';

import 'package:supabase_flutter/supabase_flutter.dart';

/// The second step an account needs after its password.
enum SecondStep { authenticator, email }

/// A sign-in failure with a sentence a member can act on.
class AuthFailure implements Exception {
  const AuthFailure(this.message, {this.offline = false});
  final String message;

  /// The server could not be reached; nothing was decided.
  final bool offline;

  @override
  String toString() => message;
}

abstract class AuthBackend {
  bool get hasSession;
  String? get email;

  /// Emits when the session ends without the user asking (a revoked or expired refresh token).
  Stream<void> get sessionEnded;

  Future<void> signInWithPassword(String email, String password);

  /// The step still owed after a password sign-in, or null. Decided like the web's `needsCode`
  /// (authenticator: the session is aal1 and could be aal2) and `needsEmailCode`
  /// (`security_status().needs_email_code`).
  Future<SecondStep?> pendingSecondStep();

  Future<void> sendEmailCode(String email);
  Future<void> confirmEmailCode(String code);
  Future<void> verifyAuthenticatorCode(String code);

  /// `can_use_app()`.
  Future<bool> canUseApp();

  Future<void> signOut();
}

class SupabaseAuthBackend implements AuthBackend {
  SupabaseAuthBackend(this._client);
  final SupabaseClient _client;

  GoTrueClient get _auth => _client.auth;

  @override
  bool get hasSession => _auth.currentSession != null;

  @override
  String? get email => _auth.currentUser?.email;

  @override
  Stream<void> get sessionEnded => _auth.onAuthStateChange
      .where((state) => state.event == AuthChangeEvent.signedOut)
      .map((_) {});

  Future<T> _guard<T>(Future<T> Function() run) async {
    try {
      return await run();
    } on AuthApiException catch (e) {
      throw AuthFailure(_sentence(e));
    } on AuthRetryableFetchException {
      throw const AuthFailure(
        "Can't reach the server. Check your connection.",
        offline: true,
      );
    } on FunctionException catch (e) {
      final details = e.details;
      final reason = details is Map ? details['error'] : null;
      throw AuthFailure(
        reason is String && reason.isNotEmpty
            ? reason
            : 'That code is wrong or has expired.',
      );
    } on AuthException catch (e) {
      throw AuthFailure(_sentence(e));
    } on TimeoutException {
      throw const AuthFailure(
        "Can't reach the server. Check your connection.",
        offline: true,
      );
    } on Exception {
      throw const AuthFailure(
        "Can't reach the server. Check your connection.",
        offline: true,
      );
    }
  }

  static String _sentence(AuthException e) {
    switch (e.code) {
      case 'invalid_credentials':
        return "That email and password don't match.";
      case 'otp_expired':
      case 'invalid_otp':
      case 'mfa_verification_failed':
      case 'mfa_challenge_expired':
        return 'That code is wrong or has expired.';
      case 'over_request_rate_limit':
      case 'over_email_send_rate_limit':
      case 'too_many_requests':
        return 'Too many attempts. Wait a minute and try again.';
      case 'user_banned':
        return 'This account is suspended. Ask an admin.';
    }
    if (e.statusCode == '429') {
      return 'Too many attempts. Wait a minute and try again.';
    }
    return 'Something went wrong. Try again.';
  }

  @override
  Future<void> signInWithPassword(String email, String password) =>
      _guard(() => _auth.signInWithPassword(email: email, password: password));

  @override
  Future<SecondStep?> pendingSecondStep() => _guard(() async {
    final assurance = _auth.mfa.getAuthenticatorAssuranceLevel();
    if (assurance.currentLevel == AuthenticatorAssuranceLevels.aal1 &&
        assurance.nextLevel == AuthenticatorAssuranceLevels.aal2) {
      return SecondStep.authenticator;
    }
    final security = await _client.rpc('security_status');
    if (security is Map && security['needs_email_code'] == true) {
      return SecondStep.email;
    }
    return null;
  });

  @override
  Future<void> sendEmailCode(String email) =>
      _guard(() => _auth.signInWithOtp(email: email, shouldCreateUser: false));

  @override
  Future<void> confirmEmailCode(String code) => _guard(
    () => _client.functions.invoke('confirm-email-code', body: {'code': code}),
  );

  @override
  Future<void> verifyAuthenticatorCode(String code) => _guard(() async {
    final factors = await _auth.mfa.listFactors();
    if (factors.totp.isEmpty) {
      throw const AuthFailure(
        'No authenticator is registered. Set one up on the website.',
      );
    }
    await _auth.mfa.challengeAndVerify(
      factorId: factors.totp.first.id,
      code: code,
    );
  });

  @override
  Future<bool> canUseApp() =>
      _guard(() async => (await _client.rpc('can_use_app')) == true);

  @override
  Future<void> signOut() => _auth.signOut(scope: SignOutScope.local);
}
