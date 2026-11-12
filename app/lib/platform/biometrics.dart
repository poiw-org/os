import 'package:local_auth/local_auth.dart';

/// Face ID, Touch ID or the fingerprint before a door opens, with the device passcode as the
/// system's own fallback. A phone with no lock set up is let through: there is nothing to ask.
class Biometrics {
  Biometrics([LocalAuthentication? auth])
    : _auth = auth ?? LocalAuthentication();

  final LocalAuthentication _auth;

  /// True when the member confirmed, or the phone has no way to ask.
  Future<bool> confirmUnlock(String doorName) async {
    try {
      if (!await _auth.isDeviceSupported()) return true;
      return await _auth.authenticate(
        localizedReason: 'Unlock $doorName',
        persistAcrossBackgrounding: true,
      );
    } on LocalAuthException catch (e) {
      return e.code == LocalAuthExceptionCode.noCredentialsSet;
    } on Object {
      return false;
    }
  }
}
