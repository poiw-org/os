// Dates as web/src/lib/format.ts shows them: `toLocaleDateString('el-GR')` and
// `toLocaleTimeString('el-GR')`, which read "3/3/2027 2:05:00 μ.μ.".

String formatDateTime(DateTime date) {
  final local = date.toLocal();
  final hour12 = local.hour % 12 == 0 ? 12 : local.hour % 12;
  final suffix = local.hour < 12 ? 'π.μ.' : 'μ.μ.';
  final mm = local.minute.toString().padLeft(2, '0');
  final ss = local.second.toString().padLeft(2, '0');
  return '${local.day}/${local.month}/${local.year} $hour12:$mm:$ss $suffix';
}

const _months = [
  'January',
  'February',
  'March',
  'April',
  'May',
  'June',
  'July',
  'August',
  'September',
  'October', 'November', 'December', //
];

/// "3 March 2027", for what a screen reader says.
String spokenDate(DateTime date) {
  final local = date.toLocal();
  return '${local.day} ${_months[local.month - 1]} ${local.year}';
}
