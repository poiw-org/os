// Dates as the console prints them, and as a screen reader says them.

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

String _two(int n) => n.toString().padLeft(2, '0');

/// "03 MAR 2027", the console's reading of a date.
String formatReadingDate(DateTime date) {
  final local = date.toLocal();
  final month = _months[local.month - 1].substring(0, 3).toUpperCase();
  return '${_two(local.day)} $month ${local.year}';
}

/// "14:05", 24-hour.
String formatReadingTime(DateTime date) {
  final local = date.toLocal();
  return '${_two(local.hour)}:${_two(local.minute)}';
}

/// "3 March 2027", for what a screen reader says.
String spokenDate(DateTime date) {
  final local = date.toLocal();
  return '${local.day} ${_months[local.month - 1]} ${local.year}';
}
