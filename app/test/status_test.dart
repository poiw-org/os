import 'package:doora/keys/virtual_key.dart';
import 'package:flutter_test/flutter_test.dart';

// Mirrors web/src/lib/status.ts: disabled, then expired, then pending, else active.
void main() {
  final now = DateTime.utc(2027, 3, 3, 12);
  KeyStatus status({
    bool disabled = false,
    required DateTime from,
    required DateTime to,
  }) => keyStatus(disabled: disabled, notBefore: from, expiresAt: to, now: now);

  test('active inside the window', () {
    expect(
      status(from: DateTime.utc(2027), to: DateTime.utc(2028)),
      KeyStatus.active,
    );
  });
  test('disabled wins over everything', () {
    expect(
      status(disabled: true, from: DateTime.utc(2020), to: DateTime.utc(2021)),
      KeyStatus.disabled,
    );
  });
  test(
    'expired when the end is not after now, even if also not yet active',
    () {
      expect(status(from: DateTime.utc(2030), to: now), KeyStatus.expired);
    },
  );
  test('pending before the start', () {
    expect(
      status(from: DateTime.utc(2028), to: DateTime.utc(2029)),
      KeyStatus.pending,
    );
  });
  test('sorts active first, then by expiry', () {
    VirtualKey k(
      String id,
      DateTime from,
      DateTime to, {
      bool disabled = false,
    }) =>
        VirtualKey(id: id, notBefore: from, expiresAt: to, disabled: disabled);
    final keys = [
      k('off', DateTime.utc(2027), DateTime.utc(2030), disabled: true),
      k('late', DateTime.utc(2027), DateTime.utc(2029)),
      k('soon', DateTime.utc(2027), DateTime.utc(2028)),
    ]..sort((a, b) => VirtualKey.compare(a, b, now));
    expect(keys.map((k) => k.id), ['soon', 'late', 'off']);
  });
}
