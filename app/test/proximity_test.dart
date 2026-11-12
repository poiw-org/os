import 'package:doora/door/proximity.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  final t0 = DateTime(2026);
  DateTime at(int ms) => t0.add(Duration(milliseconds: ms));

  test('arrives only above -70, then stays until the door goes quiet', () {
    final tracker = ProximityTracker();
    tracker
      ..add('d', -72, at(0))
      ..add('d', -71, at(100));
    expect(tracker.nearAt(at(100)), isNull);
    tracker
      ..add('d', -69, at(200))
      ..add('d', -65, at(300));
    expect(tracker.nearAt(at(300)), 'd');
    for (var i = 0; i < 10; i++) {
      tracker.add('d', -95, at(400 + i * 100));
    }
    expect(tracker.nearAt(at(1400)), 'd');
    expect(tracker.nearAt(at(1300 + 6001)), isNull);
  });
}
