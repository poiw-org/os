// The web app's look as plain Dart: colour tokens, type, motion. The few shared widgets live in
// widgets.dart. Source of truth: web/src/app.css and web/src/lib/console.css.
import 'package:material_ui/material_ui.dart';

import 'tokens.g.dart';

export 'tokens.g.dart';

const String grotesk = 'SpaceGrotesk';
const String mono = 'SpaceMono';

/// Provides the active [Palette]: the dark app by default, [Palette.sheet] inside the unlock sheet.
class DooraTheme extends InheritedWidget {
  const DooraTheme({super.key, required this.palette, required super.child});

  final Palette palette;

  static Palette of(BuildContext context) {
    final theme = context.dependOnInheritedWidgetOfExactType<DooraTheme>();
    assert(theme != null, 'No DooraTheme above this widget');
    return theme!.palette;
  }

  @override
  bool updateShouldNotify(DooraTheme oldWidget) => palette != oldWidget.palette;
}

/// Text styles from the web's rules: Space Grotesk for headings and body, Space Mono for labels and
/// buttons. Sizes are logical pixels and scale with the system text size.
class DooraType {
  const DooraType(this.palette);
  final Palette palette;

  TextStyle get body => TextStyle(
    fontFamily: grotesk,
    fontSize: 14,
    height: 1.5,
    color: palette.ink,
  );

  TextStyle get muted => body.copyWith(color: palette.gray400);

  TextStyle get heading => TextStyle(
    fontFamily: grotesk,
    fontWeight: FontWeight.w700,
    fontVariations: const [FontVariation('wght', 700)],
    fontSize: 20,
    height: 1.3,
    letterSpacing: -0.2, // -0.01em
    color: palette.ink,
  );

  /// The screen's title: larger, tighter tracking as it grows.
  TextStyle get display => heading.copyWith(
    fontSize: 30,
    height: 1.1,
    letterSpacing: -0.6, // -0.02em
  );

  /// A value printed by the console: dates, times, ids.
  TextStyle get reading => TextStyle(
    fontFamily: mono,
    fontSize: 13,
    height: 1.4,
    color: palette.ink,
  );

  /// `.eyebrow`: mono 11, uppercase (apply [String.toUpperCase]), 0.14em.
  TextStyle get eyebrow => TextStyle(
    fontFamily: mono,
    fontSize: 11,
    height: 1.3,
    letterSpacing: 1.54,
    color: palette.gray400,
  );

  /// `.btn`: mono 11, uppercase, 0.12em.
  TextStyle get button => TextStyle(
    fontFamily: mono,
    fontSize: 11,
    height: 1.3,
    letterSpacing: 1.32,
    color: palette.gray600,
  );

  /// Input text: mono for codes.
  TextStyle get input => body.copyWith(fontSize: 16, height: 1.3);
}

/// Durations and easing of the web app (`--dur-*`, `--ease-out`).
abstract final class Motion {
  static const Duration fast = Duration(milliseconds: 120);
  static const Duration base = Duration(milliseconds: 220);
  static const Duration page = Duration(milliseconds: 360);
  static const Curve curve = Cubic(0.23, 1, 0.32, 1);

  /// Zero when the system asks for reduced motion.
  static Duration of(BuildContext context, Duration duration) =>
      MediaQuery.disableAnimationsOf(context) ? Duration.zero : duration;

  static bool reduced(BuildContext context) =>
      MediaQuery.disableAnimationsOf(context);
}

extension DooraContext on BuildContext {
  Palette get colors => DooraTheme.of(this);
  DooraType get type => DooraType(colors);
}

/// The only Material theme: a host for text selection, the keyboard and the overlay machinery.
/// Everything visible is drawn by the widgets in widgets.dart.
ThemeData hostTheme() {
  const palette = Palette.dark;
  return ThemeData(
    useMaterial3: true,
    brightness: Brightness.dark,
    fontFamily: grotesk,
    scaffoldBackgroundColor: palette.voidSurface,
    canvasColor: palette.voidSurface,
    colorScheme: ColorScheme.dark(
      surface: palette.voidSurface,
      primary: palette.brand,
      onPrimary: const Color(0xFFFFFFFF),
    ),
    splashFactory: NoSplash.splashFactory,
    highlightColor: const Color(0x00000000),
    splashColor: const Color(0x00000000),
    textSelectionTheme: TextSelectionThemeData(
      cursorColor: palette.ink,
      selectionColor: palette.brand.withValues(alpha: 0.55),
      selectionHandleColor: palette.brandText,
    ),
    pageTransitionsTheme: const PageTransitionsTheme(
      builders: {
        TargetPlatform.android: _NoTransition(),
        TargetPlatform.iOS: _NoTransition(),
      },
    ),
  );
}

class _NoTransition extends PageTransitionsBuilder {
  const _NoTransition();

  @override
  Widget buildTransitions<T>(
    PageRoute<T> route,
    BuildContext context,
    Animation<double> animation,
    Animation<double> secondaryAnimation,
    Widget child,
  ) => child;
}
