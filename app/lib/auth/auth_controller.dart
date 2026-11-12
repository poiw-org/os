import 'dart:async';

import 'package:flutter/foundation.dart';

import 'auth_backend.dart';

enum AuthStage {
  /// Reading the stored session.
  starting,
  signedOut,

  /// Signed in with a password; the account owes a code ([AuthController.secondStep]).
  secondStep,

  /// Asking `can_use_app`.
  checking,

  /// Signed in but without a usable virtual key.
  noAccess,
  ready,
}

/// The sign-in flow as state: what the screens show and what they may do next.
class AuthController extends ChangeNotifier {
  AuthController(this._backend) {
    _ended = _backend.sessionEnded.listen((_) {
      if (_stage != AuthStage.signedOut) _set(AuthStage.signedOut);
    });
  }

  final AuthBackend _backend;
  late final StreamSubscription<void> _ended;

  AuthStage _stage = AuthStage.starting;
  SecondStep? _secondStep;
  String? _passwordError;
  String? _codeError;
  String? _notice;
  bool _busy = false;

  AuthStage get stage => _stage;
  SecondStep? get secondStep => _secondStep;
  bool get busy => _busy;

  /// Under the password field.
  String? get passwordError => _passwordError;

  /// Under the code field.
  String? get codeError => _codeError;

  /// A calm line that is not an error (a code was sent).
  String? get notice => _notice;
  String? get email => _backend.email;

  void _set(AuthStage stage) {
    _stage = stage;
    notifyListeners();
  }

  /// Reads the stored session. Offline with a session, the app is ready: the keys screen shows what
  /// is cached and unlocking never needs the network.
  Future<void> start() async {
    if (!_backend.hasSession) return _set(AuthStage.signedOut);
    await _afterPassword(offlineMeansReady: true);
  }

  Future<void> signIn(String email, String password) async {
    if (_busy) return;
    _passwordError = null;
    _busy = true;
    notifyListeners();
    try {
      await _backend.signInWithPassword(email.trim(), password);
    } on AuthFailure catch (e) {
      _passwordError = e.message;
      _busy = false;
      notifyListeners();
      return;
    }
    _busy = false;
    await _afterPassword();
  }

  Future<void> _afterPassword({bool offlineMeansReady = false}) async {
    _set(AuthStage.checking);
    try {
      final step = await _backend.pendingSecondStep();
      if (step != null) {
        _secondStep = step;
        _codeError = null;
        _notice = null;
        _set(AuthStage.secondStep);
        if (step == SecondStep.email) await sendCode();
        return;
      }
      _set(await _backend.canUseApp() ? AuthStage.ready : AuthStage.noAccess);
    } on AuthFailure catch (e) {
      if (offlineMeansReady && e.offline) return _set(AuthStage.ready);
      // The password worked; the check did not. Back to the form with the reason.
      _passwordError = e.message;
      await _backend.signOut();
      _set(AuthStage.signedOut);
    }
  }

  Future<void> sendCode() async {
    final email = _backend.email;
    if (email == null) return;
    _codeError = null;
    _busy = true;
    notifyListeners();
    try {
      await _backend.sendEmailCode(email);
      _notice = 'We emailed a code to $email.';
    } on AuthFailure catch (e) {
      _codeError = e.message;
    }
    _busy = false;
    notifyListeners();
  }

  Future<void> submitCode(String code) async {
    if (_busy) return;
    final trimmed = code.trim();
    if (trimmed.length < 6) {
      _codeError = 'Enter the 6-digit code.';
      notifyListeners();
      return;
    }
    _codeError = null;
    _busy = true;
    notifyListeners();
    try {
      if (_secondStep == SecondStep.email) {
        await _backend.confirmEmailCode(trimmed);
      } else {
        await _backend.verifyAuthenticatorCode(trimmed);
      }
    } on AuthFailure catch (e) {
      _codeError = e.message;
      _busy = false;
      notifyListeners();
      return;
    }
    _busy = false;
    await _afterPassword();
  }

  Future<void> signOut() async {
    await _backend.signOut();
    _secondStep = null;
    _passwordError = null;
    _codeError = null;
    _notice = null;
    _set(AuthStage.signedOut);
  }

  @override
  void dispose() {
    _ended.cancel();
    super.dispose();
  }
}
