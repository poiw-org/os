// A member's virtual key and its status, mirroring web/src/lib/status.ts.

enum KeyStatus {
  active('ENABLED'),
  expired('EXPIRED'),
  pending('NOT YET ACTIVE'),
  disabled('DISABLED');

  const KeyStatus(this.label);
  final String label;
}

/// Same order of checks as `keyStatus` in web/src/lib/status.ts.
KeyStatus keyStatus({
  required bool disabled,
  required DateTime notBefore,
  required DateTime expiresAt,
  DateTime? now,
}) {
  final at = now ?? DateTime.now();
  if (disabled) return KeyStatus.disabled;
  if (!expiresAt.isAfter(at)) return KeyStatus.expired;
  if (notBefore.isAfter(at)) return KeyStatus.pending;
  return KeyStatus.active;
}

class VirtualKey {
  const VirtualKey({
    required this.id,
    required this.notBefore,
    required this.expiresAt,
    required this.disabled,
    this.deviceName,
  });

  factory VirtualKey.fromRow(Map<String, dynamic> row) => VirtualKey(
    id: row['id'] as String,
    notBefore: DateTime.parse(row['not_before'] as String),
    expiresAt: DateTime.parse(row['expires_at'] as String),
    disabled: row['disabled'] as bool,
    deviceName: row['device_name'] as String?,
  );

  Map<String, dynamic> toJson() => {
    'id': id,
    'not_before': notBefore.toUtc().toIso8601String(),
    'expires_at': expiresAt.toUtc().toIso8601String(),
    'disabled': disabled,
    'device_name': deviceName,
  };

  final String id;
  final DateTime notBefore;
  final DateTime expiresAt;
  final bool disabled;

  /// Set once a phone is bound to the key (the backend's `device_name`).
  final String? deviceName;

  KeyStatus status([DateTime? now]) => keyStatus(
    disabled: disabled,
    notBefore: notBefore,
    expiresAt: expiresAt,
    now: now,
  );

  /// Active keys first, then by expiry (soonest first), as the brief asks.
  static int compare(VirtualKey a, VirtualKey b, [DateTime? now]) {
    final aActive = a.status(now) == KeyStatus.active;
    final bActive = b.status(now) == KeyStatus.active;
    if (aActive != bActive) return aActive ? -1 : 1;
    return a.expiresAt.compareTo(b.expiresAt);
  }
}
