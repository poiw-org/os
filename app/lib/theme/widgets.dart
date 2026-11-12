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

/// `.row`: 10 x 16 padding.
class PanelRow extends StatelessWidget {
  const PanelRow({super.key, required this.child});
  final Widget child;

  @override
  Widget build(BuildContext context) => Padding(
    padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 10),
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
  });

  final String label;
  final VoidCallback? onPressed;
  final ButtonKind kind;
  final String? semanticsLabel;

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
      transform: Matrix4.translationValues(
        0,
        sunk && !Motion.reduced(context) ? 1 : 0,
        0,
      ),
      constraints: const BoxConstraints(minHeight: minTarget),
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
                if (widget.obscure)
                  DooraTextButton(
                    label: _revealed ? 'Hide' : 'Show',
                    semanticsLabel: _revealed
                        ? 'Hide password'
                        : 'Show password',
                    color: c.gray400,
                    onPressed: () => setState(() => _revealed = !_revealed),
                  ),
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

/// `.tag.tag-signal`: the `VIRTUAL` tag.
class VirtualTag extends StatelessWidget {
  const VirtualTag({super.key});

  @override
  Widget build(BuildContext context) {
    final c = Palette.dark;
    return ExcludeSemantics(
      child: DecoratedBox(
        decoration: BoxDecoration(
          color: c.brand.withValues(alpha: 0.18),
          border: Border.all(color: c.brand),
        ),
        child: Padding(
          padding: const EdgeInsets.symmetric(horizontal: 6, vertical: 2),
          child: Text(
            'VIRTUAL',
            style: context.type.eyebrow.copyWith(color: LampColors.tagText),
          ),
        ),
      ),
    );
  }
}

/// The page frame: a slim top bar with the brand, then the content in a single left-aligned column
/// on `void`. Scrolls, so 200 % text still fits.
class Screen extends StatelessWidget {
  const Screen({super.key, required this.children, this.onRefresh});

  final List<Widget> children;
  final Future<void> Function()? onRefresh;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    final content = ListView(
      physics: const AlwaysScrollableScrollPhysics(),
      padding: const EdgeInsets.fromLTRB(16, 16, 16, 32),
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
      child: SafeArea(
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            const BrandBar(),
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

/// The brand at the top, in the console bar's style: a cell with the mark and the name in mono caps.
class BrandBar extends StatelessWidget {
  const BrandBar({super.key});

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return Semantics(
      header: true,
      label: 'Doora',
      excludeSemantics: true,
      child: DecoratedBox(
        decoration: BoxDecoration(
          color: c.paper,
          border: Border(bottom: BorderSide(color: c.gray200, width: 2)),
        ),
        child: Row(
          children: [
            DecoratedBox(
              decoration: BoxDecoration(
                border: Border(right: BorderSide(color: c.gray200)),
              ),
              child: Padding(
                padding: const EdgeInsets.symmetric(
                  horizontal: 18,
                  vertical: 10,
                ),
                child: Row(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    Image.asset(
                      'assets/images/poiw.png',
                      width: 24,
                      height: 24,
                      excludeFromSemantics: true,
                    ),
                    const SizedBox(width: 10),
                    Text(
                      'DOORA',
                      style: context.type.button.copyWith(
                        color: c.ink,
                        fontSize: 12,
                      ),
                    ),
                  ],
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }
}
