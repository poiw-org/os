/// "Near" is RSSI above this for two consecutive scan results. Tune it at the real door: the bridge's
/// antenna is weak, so a good reading there may be well below a phone-to-phone one.
const int nearRssiThreshold = -75;

/// Decides when a door is near from a stream of scan results, one door at a time.
class ProximityTracker {
  ProximityTracker({
    this.threshold = nearRssiThreshold,
    this.requiredResults = 2,
    this.staleAfter = const Duration(seconds: 6),
  });

  final int threshold;

  /// Consecutive results above [threshold]. One in the background, where the system reports a
  /// discovery once.
  final int requiredResults;

  /// A door that has not been heard for this long is no longer near.
  final Duration staleAfter;

  String? _deviceId;
  int _streak = 0;
  int _rssi = -127;
  DateTime? _lastHeard;

  /// Feeds one scan result.
  void add(String deviceId, int rssi, DateTime now) {
    if (_deviceId != deviceId) {
      _deviceId = deviceId;
      _streak = 0;
    }
    _streak = rssi > threshold ? _streak + 1 : 0;
    _rssi = rssi;
    _lastHeard = now;
  }

  /// The door that is near now, or null.
  String? nearAt(DateTime now) {
    final heard = _lastHeard;
    if (_deviceId == null || heard == null) return null;
    if (now.difference(heard) > staleAfter) return null;
    return _streak >= requiredResults ? _deviceId : null;
  }

  int get lastRssi => _rssi;

  void reset() {
    _deviceId = null;
    _streak = 0;
    _lastHeard = null;
  }
}
