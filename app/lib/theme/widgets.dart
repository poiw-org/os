// The shared widgets: the web's `.panel`, `.row`, `.btn`, inputs, the readout chip with its lamp and
// the `VIRTUAL` tag, rebuilt as small widgets with square corners, bevels and no gradients.
import 'package:material_ui/material_ui.dart';
import 'package:flutter/services.dart';

import 'theme.dart';

const double minTarget = 44;

/// 1 px highlight lines just inside the border: `inset 1px 1px 0 <topLeft>` and, when given,
/// `inset -1px -1px 0 <bottomRight>`.
class _BevelPainter extends CustomPainter {
  const _BevelPainter({required this.topLeft, this.bottomRight});

  final Color topLeft;
  final Color? bottomRight;

  @override
  void paint(Canvas canvas, Size size) {
    final light = Paint()..color = topLeft;
    canvas.drawRect(Rect.fromLTWH(0, 0, size.width, 1), light);
    canvas.drawRect(Rect.fromLTWH(0, 1, 1, size.height - 1), light);
    final dark = bottomRight;
    if (dark != null) {
      final paint = Paint()..color = dark;
      canvas.drawRect(
        Rect.fromLTWH(1, size.height - 1, size.width - 1, 1),
        paint,
      );
      canvas.drawRect(
        Rect.fromLTWH(size.width - 1, 1, 1, size.height - 2),
        paint,
      );
    }
  }

  @override
  bool shouldRepaint(_BevelPainter old) =>
      old.topLeft != topLeft || old.bottomRight != bottomRight;
}

Color _white(double opacity) =>
    const Color(0xFFFFFFFF).withValues(alpha: opacity);
Color _black(double opacity) =>
    const Color(0xFF000000).withValues(alpha: opacity);

/// A section label.
class Eyebrow extends StatelessWidget {
  const Eyebrow(this.text, {super.key});
  final String text;

  @override
  Widget build(BuildContext context) =>
      Text(text.toUpperCase(), style: context.type.eyebrow);
}

/// Loading is the word, not a spinner.
class LoadingText extends StatelessWidget {
  const LoadingText({super.key});

  @override
  Widget build(BuildContext context) =>
      Text('Loading…', style: context.type.muted);
}

/// `.panel`: a bordered surface whose children are separated by hairlines.
class Panel extends StatelessWidget {
  const Panel({super.key, required this.children, this.padding});

  final List<Widget> children;
  final EdgeInsetsGeometry? padding;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final items = <Widget>[];
    for (var i = 0; i < children.length; i++) {
      if (i > 0) {
        items.add(
          ColoredBox(color: c.gray200, child: const SizedBox(height: 1)),
        );
      }
      items.add(children[i]);
    }
    return DecoratedBox(
      decoration: BoxDecoration(
        color: c.paper,
        border: Border.all(color: c.gray200),
      ),
      child: CustomPaint(
        foregroundPainter: _BevelPainter(topLeft: _white(0.10)),
        child: Padding(
          padding: padding ?? EdgeInsets.zero,
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: items,
          ),
        ),
      ),
    );
  }
}

/// `.row`, with room to breathe on a phone: 16 x 18.
class PanelRow extends StatelessWidget {
  const PanelRow({super.key, required this.child});
  final Widget child;

  @override
  Widget build(BuildContext context) => Padding(
    padding: const EdgeInsets.symmetric(horizontal: 18, vertical: 16),
    child: child,
  );
}

enum ButtonKind { normal, primary, destructive }

/// `.btn`, `.btn-primary` and `.btn.tone-red`. One primary button per screen.
class DooraButton extends StatefulWidget {
  const DooraButton({
    super.key,
    required this.label,
    required this.onPressed,
    this.kind = ButtonKind.normal,
    this.semanticsLabel,
    this.expand = false,
    this.tall = false,
  });

  final String label;
  final VoidCallback? onPressed;
  final ButtonKind kind;
  final String? semanticsLabel;

  /// Fills the width it is given.
  final bool expand;

  /// 52 px instead of 44: the one action a screen exists for.
  final bool tall;

  @override
  State<DooraButton> createState() => _DooraButtonState();
}

class _DooraButtonState extends State<DooraButton> {
  bool _pressed = false;
  bool _hovered = false;
  bool _focused = false;

  bool get _enabled => widget.onPressed != null;

  void _activate() {
    if (!_enabled) return;
    HapticFeedback.selectionClick();
    widget.onPressed!();
  }

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final primary = widget.kind == ButtonKind.primary;
    final destructive = widget.kind == ButtonKind.destructive;
    final lit = _enabled && (_hovered || _pressed);

    final Color fill = primary
        ? (lit ? c.brandHover : c.brand)
        : (lit ? c.paper2.withValues(alpha: 1) : c.paper2);
    final Color border = primary ? (lit ? c.brandHover : c.brand) : c.gray200;
    final Color text = primary
        ? const Color(0xFFFFFFFF)
        : destructive
        ? c.red500
        : (lit ? c.ink : c.gray600);

    final sunk = _pressed && _enabled;
    final duration = Motion.of(context, Motion.fast);

    Widget button = AnimatedContainer(
      duration: duration,
      curve: Motion.curve,
      transformAlignment: Alignment.center,
      // Pressed: sinks 1 px into the panel and gives a little (0.97), on touch-down.
      transform: sunk && !Motion.reduced(context)
          ? (Matrix4.translationValues(0, 1, 0)
              ..scaleByDouble(0.97, 0.97, 1, 1))
          : Matrix4.identity(),
      constraints: BoxConstraints(
        minHeight: widget.tall ? 52 : minTarget,
        minWidth: widget.expand ? double.infinity : 0,
      ),
      decoration: BoxDecoration(
        color: fill,
        border: Border.all(color: border),
      ),
      child: CustomPaint(
        foregroundPainter: sunk
            ? _BevelPainter(topLeft: _black(0.4), bottomRight: _white(0.10))
            : _BevelPainter(topLeft: _white(0.14), bottomRight: _black(0.25)),
        child: Padding(
          padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
          child: Center(
            widthFactor: 1,
            child: AnimatedDefaultTextStyle(
              duration: duration,
              style: context.type.button.copyWith(color: text),
              textAlign: TextAlign.center,
              child: Text(widget.label.toUpperCase()),
            ),
          ),
        ),
      ),
    );

    button = Opacity(opacity: _enabled ? 1 : 0.45, child: button);

    return Semantics(
      button: true,
      enabled: _enabled,
      label: widget.semanticsLabel ?? widget.label,
      excludeSemantics: true,
      onTap: _enabled ? _activate : null,
      child: FocusableActionDetector(
        enabled: _enabled,
        onShowHoverHighlight: (v) => setState(() => _hovered = v),
        onShowFocusHighlight: (v) => setState(() => _focused = v),
        actions: {
          ActivateIntent: CallbackAction<ActivateIntent>(
            onInvoke: (_) {
              _activate();
              return null;
            },
          ),
        },
        child: GestureDetector(
          behavior: HitTestBehavior.opaque,
          onTapDown: (_) => setState(() => _pressed = true),
          onTapCancel: () => setState(() => _pressed = false),
          onTapUp: (_) => setState(() => _pressed = false),
          onTap: _activate,
          child: _FocusOutline(focused: _focused, child: button),
        ),
      ),
    );
  }
}

/// Focus: a 2 px `brandText` outline, 2 px offset.
class _FocusOutline extends StatelessWidget {
  const _FocusOutline({required this.focused, required this.child});
  final bool focused;
  final Widget child;

  @override
  Widget build(BuildContext context) => Stack(
    clipBehavior: Clip.none,
    children: [
      child,
      if (focused)
        Positioned(
          left: -4,
          right: -4,
          top: -4,
          bottom: -4,
          child: IgnorePointer(
            child: DecoratedBox(
              decoration: BoxDecoration(
                border: Border.all(color: context.colors.brandText, width: 2),
              ),
            ),
          ),
        ),
    ],
  );
}

/// A text-only action in `brandText`, underlined (the web's `text-brand-text underline`). On the
/// light sheet use [color] = ink, because `brandText` there is under 4.5:1.
class DooraTextButton extends StatefulWidget {
  const DooraTextButton({
    super.key,
    required this.label,
    required this.onPressed,
    this.color,
    this.semanticsLabel,
  });

  final String label;
  final VoidCallback? onPressed;
  final Color? color;
  final String? semanticsLabel;

  @override
  State<DooraTextButton> createState() => _DooraTextButtonState();
}

class _DooraTextButtonState extends State<DooraTextButton> {
  bool _focused = false;

  @override
  Widget build(BuildContext context) {
    final enabled = widget.onPressed != null;
    final color = widget.color ?? context.colors.brandText;
    return Semantics(
      button: true,
      enabled: enabled,
      label: widget.semanticsLabel ?? widget.label,
      excludeSemantics: true,
      onTap: widget.onPressed,
      child: FocusableActionDetector(
        enabled: enabled,
        onShowFocusHighlight: (v) => setState(() => _focused = v),
        actions: {
          ActivateIntent: CallbackAction<ActivateIntent>(
            onInvoke: (_) {
              widget.onPressed?.call();
              return null;
            },
          ),
        },
        child: GestureDetector(
          behavior: HitTestBehavior.opaque,
          onTap: widget.onPressed,
          child: _FocusOutline(
            focused: _focused,
            child: ConstrainedBox(
              constraints: const BoxConstraints(
                minHeight: minTarget,
                minWidth: minTarget,
              ),
              child: Center(
                widthFactor: 1,
                child: Opacity(
                  opacity: enabled ? 1 : 0.45,
                  child: Text(
                    widget.label,
                    style: context.type.body.copyWith(
                      color: color,
                      decoration: TextDecoration.underline,
                      decorationColor: color,
                    ),
                  ),
                ),
              ),
            ),
          ),
        ),
      ),
    );
  }
}

/// A labelled input: `backing` fill, hairline border, `brandText` border and a 2 px `brand` ring at
/// 55 % on focus, and one calm sentence under the field when [errorText] is set.
class DooraTextField extends StatefulWidget {
  const DooraTextField({
    super.key,
    required this.label,
    required this.controller,
    this.errorText,
    this.obscure = false,
    this.keyboardType,
    this.textInputAction,
    this.autofillHints,
    this.enabled = true,
    this.autofocus = false,
    this.onSubmitted,
    this.monospace = false,
    this.hint,
    this.maxLength,
  });

  final String label;
  final TextEditingController controller;
  final String? errorText;
  final bool obscure;
  final TextInputType? keyboardType;
  final TextInputAction? textInputAction;
  final Iterable<String>? autofillHints;
  final bool enabled;
  final bool autofocus;
  final ValueChanged<String>? onSubmitted;
  final bool monospace;
  final String? hint;
  final int? maxLength;

  @override
  State<DooraTextField> createState() => _DooraTextFieldState();
}

class _DooraTextFieldState extends State<DooraTextField> {
  final FocusNode _focus = FocusNode();
  bool _revealed = false;

  @override
  void initState() {
    super.initState();
    _focus.addListener(() => setState(() {}));
  }

  @override
  void dispose() {
    _focus.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final focused = _focus.hasFocus;
    final style = context.type.input.copyWith(
      fontFamily: widget.monospace ? mono : grotesk,
      letterSpacing: widget.monospace ? 2 : null,
    );

    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Padding(
          padding: const EdgeInsets.only(bottom: 6),
          child: ExcludeSemantics(child: Eyebrow(widget.label)),
        ),
        AnimatedContainer(
          duration: Motion.of(context, Motion.fast),
          curve: Motion.curve,
          constraints: const BoxConstraints(minHeight: minTarget),
          decoration: BoxDecoration(
            color: c.backing,
            border: Border.all(color: focused ? c.brandText : c.gray200),
            boxShadow: focused
                ? [
                    BoxShadow(
                      color: c.brand.withValues(alpha: 0.55),
                      spreadRadius: 2,
                    ),
                  ]
                : null,
          ),
          child: CustomPaint(
            foregroundPainter: _BevelPainter(
              topLeft: _black(0.4),
              bottomRight: _white(0.08),
            ),
            child: Row(
              children: [
                Expanded(
                  child: Semantics(
                    label: widget.label,
                    textField: true,
                    child: TextField(
                      controller: widget.controller,
                      focusNode: _focus,
                      enabled: widget.enabled,
                      autofocus: widget.autofocus,
                      obscureText: widget.obscure && !_revealed,
                      keyboardType: widget.keyboardType,
                      textInputAction: widget.textInputAction,
                      autofillHints: widget.autofillHints,
                      onSubmitted: widget.onSubmitted,
                      maxLength: widget.maxLength,
                      style: style,
                      cursorColor: c.ink,
                      autocorrect: false,
                      enableSuggestions: !widget.obscure,
                      decoration: InputDecoration(
                        isCollapsed: true,
                        border: InputBorder.none,
                        counterText: '',
                        hintText: widget.hint,
                        hintStyle: style.copyWith(color: c.gray400),
                        contentPadding: const EdgeInsets.symmetric(
                          horizontal: 14,
                          vertical: 13,
                        ),
                      ),
                    ),
                  ),
                ),
                if (widget.obscure) ...[
                  DooraTextButton(
                    label: _revealed ? 'Hide' : 'Show',
                    semanticsLabel: _revealed
                        ? 'Hide password'
                        : 'Show password',
                    color: c.gray400,
                    onPressed: () => setState(() => _revealed = !_revealed),
                  ),
                  const SizedBox(width: 14),
                ],
              ],
            ),
          ),
        ),
        if (widget.errorText != null)
          Padding(
            padding: const EdgeInsets.only(top: 6),
            child: Semantics(
              liveRegion: true,
              child: Text(
                widget.errorText!,
                style: context.type.body.copyWith(color: c.red400),
              ),
            ),
          ),
      ],
    );
  }
}

/// The lamp states of `KeyStatusBadge.svelte`: green lit, amber lit, blue waiting (breathes), red
/// dark.
enum LampState { green, amber, blueWaiting, redOff }

class Lamp extends StatefulWidget {
  const Lamp(this.state, {super.key});
  final LampState state;

  @override
  State<Lamp> createState() => _LampState();
}

class _LampState extends State<Lamp> with SingleTickerProviderStateMixin {
  late final AnimationController _breath = AnimationController(
    vsync: this,
    duration: const Duration(seconds: 2),
  );

  @override
  void didChangeDependencies() {
    super.didChangeDependencies();
    _sync();
  }

  @override
  void didUpdateWidget(Lamp old) {
    super.didUpdateWidget(old);
    _sync();
  }

  void _sync() {
    final shouldBreathe =
        widget.state == LampState.blueWaiting && !Motion.reduced(context);
    if (shouldBreathe && !_breath.isAnimating) {
      _breath.repeat(reverse: true);
    } else if (!shouldBreathe && _breath.isAnimating) {
      _breath.stop();
      _breath.value = 0;
    }
  }

  @override
  void dispose() {
    _breath.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    final color = switch (widget.state) {
      LampState.green => LampColors.green,
      LampState.amber => LampColors.amber,
      LampState.blueWaiting => LampColors.blue,
      LampState.redOff => LampColors.red,
    };
    final off = widget.state == LampState.redOff;
    // Dark lamp: the colour, unlit (58 % of it over the web's near-black).
    final fill = off ? Color.lerp(Palette.dark.backing, color, 0.58)! : color;

    return ExcludeSemantics(
      child: AnimatedBuilder(
        animation: _breath,
        builder: (context, _) {
          // `lamp-breathe`: at the midpoint the lamp drops to 35 % and loses its glow.
          final dim = widget.state == LampState.blueWaiting
              ? _breath.value
              : 0.0;
          return Opacity(
            opacity: 1 - 0.65 * dim,
            child: DecoratedBox(
              decoration: BoxDecoration(
                color: fill,
                borderRadius: BorderRadius.circular(1),
                boxShadow: off || dim > 0.5
                    ? null
                    : [
                        BoxShadow(
                          color: color.withValues(alpha: 0.65),
                          blurRadius: 6,
                          spreadRadius: 1,
                        ),
                      ],
              ),
              child: SizedBox(
                width: 8,
                height: 8,
                child: CustomPaint(
                  painter: _BevelPainter(
                    topLeft: off ? _black(0) : _white(0.5),
                  ),
                ),
              ),
            ),
          );
        },
      ),
    );
  }
}

/// The key's status as a recessed readout chip: a lamp and the label in mono caps.
class ReadoutChip extends StatelessWidget {
  const ReadoutChip({super.key, required this.lamp, required this.label});
  final LampState lamp;
  final String label;

  @override
  Widget build(BuildContext context) {
    return DecoratedBox(
      decoration: BoxDecoration(
        color: Palette.dark.backing,
        border: Border.all(color: Palette.dark.gray200),
      ),
      child: CustomPaint(
        foregroundPainter: _BevelPainter(topLeft: _black(0.7)),
        child: Padding(
          padding: const EdgeInsets.fromLTRB(8, 4, 10, 4),
          child: Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              Lamp(lamp),
              const SizedBox(width: 8),
              Flexible(
                child: Text(
                  label.toUpperCase(),
                  style: context.type.eyebrow.copyWith(color: Palette.dark.ink),
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}

/// The faint dot grid the web draws on `void`: one 1 px dot every 24 px.
class _DotGrid extends CustomPainter {
  const _DotGrid(this.color);
  final Color color;

  @override
  void paint(Canvas canvas, Size size) {
    final dot = Paint()..color = color;
    for (var y = 12.0; y < size.height; y += 24) {
      for (var x = 12.0; x < size.width; x += 24) {
        canvas.drawRect(Rect.fromLTWH(x, y, 1, 1), dot);
      }
    }
  }

  @override
  bool shouldRepaint(_DotGrid old) => old.color != color;
}

/// A labelled value, the way the console prints a reading: the label in tracked mono caps, the value
/// in mono under it.
class Readout extends StatelessWidget {
  const Readout({
    super.key,
    required this.label,
    required this.value,
    this.detail,
  });
  final String label;
  final String value;

  /// A second, quieter line under the value.
  final String? detail;

  @override
  Widget build(BuildContext context) {
    final type = context.type;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Eyebrow(label),
        const SizedBox(height: 4),
        Text(value, style: type.reading),
        if (detail != null)
          Text(
            detail!,
            style: type.reading.copyWith(color: type.palette.gray400),
          ),
      ],
    );
  }
}

/// How much of a span has passed: a recessed 4 px track with a lit fill, square ends.
class SpanMeter extends StatelessWidget {
  const SpanMeter({super.key, required this.fraction, required this.color});
  final double fraction;
  final Color color;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return ExcludeSemantics(
      child: DecoratedBox(
        decoration: BoxDecoration(
          color: c.backing,
          border: Border.all(color: c.gray200),
        ),
        child: SizedBox(
          height: 4,
          child: Align(
            alignment: Alignment.centerLeft,
            child: FractionallySizedBox(
              widthFactor: fraction.clamp(0.0, 1.0),
              heightFactor: 1,
              child: ColoredBox(color: color),
            ),
          ),
        ),
      ),
    );
  }
}

/// The page frame: the console bar with the brand, then the content in a single left-aligned column
/// on `void` and its dot grid. Scrolls, so 200 % text still fits.
class Screen extends StatelessWidget {
  const Screen({
    super.key,
    required this.children,
    this.onRefresh,
    this.barStatus,
  });

  final List<Widget> children;
  final Future<void> Function()? onRefresh;

  /// The right-hand cell of the bar, when the screen has something live to report.
  final Widget? barStatus;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final content = ListView(
      physics: const AlwaysScrollableScrollPhysics(),
      padding: EdgeInsets.fromLTRB(
        20,
        28,
        20,
        48 + MediaQuery.paddingOf(context).bottom,
      ),
      children: [
        Align(
          alignment: Alignment.topLeft,
          child: ConstrainedBox(
            constraints: const BoxConstraints(maxWidth: 480),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: children,
            ),
          ),
        ),
      ],
    );
    // Material supplies the ancestor text fields need and the default text style (no underlines).
    return Material(
      color: c.voidSurface,
      child: CustomPaint(
        painter: _DotGrid(_white(0.06)),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            BrandBar(status: barStatus),
            Expanded(
              child: onRefresh == null
                  ? content
                  : RefreshIndicator(
                      onRefresh: onRefresh!,
                      color: c.brandText,
                      backgroundColor: c.paper2,
                      child: content,
                    ),
            ),
          ],
        ),
      ),
    );
  }
}

/// The console bar: under the status bar, a row of cells divided by seams with a lit top edge. The
/// brand cell on the left; [status] in a cell on the right.
class BrandBar extends StatelessWidget {
  const BrandBar({super.key, this.status});
  final Widget? status;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final top = MediaQuery.paddingOf(context).top;
    Widget cell(Widget child, {required bool seamRight}) => DecoratedBox(
      decoration: BoxDecoration(
        border: seamRight
            ? Border(right: BorderSide(color: c.gray200))
            : Border(left: BorderSide(color: c.gray200)),
      ),
      child: CustomPaint(
        foregroundPainter: _BevelPainter(topLeft: _white(0.06)),
        child: Padding(
          padding: const EdgeInsets.symmetric(horizontal: 18),
          child: Center(widthFactor: 1, child: child),
        ),
      ),
    );

    return DecoratedBox(
      decoration: BoxDecoration(
        color: c.paper,
        border: Border(bottom: BorderSide(color: c.gray200, width: 2)),
      ),
      child: Padding(
        padding: EdgeInsets.only(top: top),
        child: CustomPaint(
          foregroundPainter: _BevelPainter(topLeft: _white(0.08)),
          child: SizedBox(
            height: 56,
            child: Row(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                Semantics(
                  header: true,
                  label: 'Doora',
                  excludeSemantics: true,
                  child: cell(
                    seamRight: true,
                    Row(
                      mainAxisSize: MainAxisSize.min,
                      children: [
                        Image.asset(
                          'assets/images/mark.png',
                          height: 26,
                          excludeFromSemantics: true,
                        ),
                        const SizedBox(width: 12),
                        Text(
                          'DOORA',
                          style: context.type.button.copyWith(
                            color: c.ink,
                            fontSize: 12,
                            fontWeight: FontWeight.w700,
                            letterSpacing: 2.4,
                          ),
                        ),
                      ],
                    ),
                  ),
                ),
                const Spacer(),
                if (status != null) cell(status!, seamRight: false),
              ],
            ),
          ),
        ),
      ),
    );
  }
}

/// A status cell's content: a lamp and a word in mono caps.
class BarStatus extends StatelessWidget {
  const BarStatus({super.key, required this.lamp, required this.label});
  final LampState lamp;
  final String label;

  @override
  Widget build(BuildContext context) => Semantics(
    label: label,
    excludeSemantics: true,
    child: Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        Lamp(lamp),
        const SizedBox(width: 10),
        Text(label.toUpperCase(), style: context.type.eyebrow),
      ],
    ),
  );
}

/// A status cell showing signal strength: a wifi glyph with the lamp's halo glow and the reading in
/// dBm. Above -60 is full, above -70 medium, anything weaker low.
class SignalStatus extends StatelessWidget {
  const SignalStatus({super.key, required this.rssi});
  final int rssi;

  @override
  Widget build(BuildContext context) {
    final level = rssi > -60
        ? 3
        : rssi > -70
        ? 2
        : 1;
    return Semantics(
      label: 'Door near, signal $rssi dBm',
      excludeSemantics: true,
      child: Row(
        mainAxisSize: MainAxisSize.min,
        children: [
          CustomPaint(
            size: const Size(18, 14),
            painter: _WifiPainter(level, LampColors.green),
          ),
          const SizedBox(width: 8),
          Text('$rssi DBM', style: context.type.eyebrow),
        ],
      ),
    );
  }
}

/// A dot and three arcs; `level` arcs are lit and glow, the rest are dark.
class _WifiPainter extends CustomPainter {
  _WifiPainter(this.level, this.color);
  final int level;
  final Color color;

  @override
  void paint(Canvas canvas, Size size) {
    final origin = Offset(size.width / 2, size.height - 1.5);
    const sweep = 1.2; // radians, centred straight up
    const start = -1.5707963 - sweep / 2;

    void draw(Paint paint, int lit) {
      for (var i = 0; i <= 3; i++) {
        final on = i <= lit;
        if (paint.maskFilter != null && !on) continue;
        final p = Paint()
          ..style = paint.style
          ..strokeCap = StrokeCap.round
          ..strokeWidth = 2
          ..maskFilter = paint.maskFilter
          ..color = on ? color : color.withValues(alpha: 0.25);
        if (i == 0) {
          canvas.drawCircle(origin, 1.25, p..style = PaintingStyle.fill);
        } else {
          final r = i * 4.2 + 1.5;
          canvas.drawArc(
            Rect.fromCircle(center: origin, radius: r),
            start,
            sweep,
            false,
            p..style = PaintingStyle.stroke,
          );
        }
      }
    }

    draw(
      Paint()..maskFilter = const MaskFilter.blur(BlurStyle.normal, 3),
      level,
    );
    draw(Paint(), level);
  }

  @override
  bool shouldRepaint(_WifiPainter old) =>
      old.level != level || old.color != color;
}
