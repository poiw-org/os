import 'dart:async';
import 'dart:math' as math;

import 'package:flutter/foundation.dart';
import 'package:flutter/physics.dart';
import 'package:flutter/services.dart';
import 'package:material_ui/material_ui.dart';

import '../keys/format.dart';
import '../theme/theme.dart';
import 'door_3d.dart';
import 'door_unlocker.dart';

/// Shows the unlock card: a floating panel with the door turning in it, the way the system
/// presents a pair of earbuds that just came near. No app can put a card over the home screen;
/// inside the app this is the same gesture.
///
/// [unlock] runs the sequence and reports the door's id when its challenge arrives. Resolves when
/// the card has closed.
Future<void> showUnlockSheet(
  BuildContext context, {
  required String initialDoorName,
  required String? Function(String doorId) doorNameFor,
  required DateTime keyExpiresAt,
  required Future<UnlockOutcome> Function(void Function(String doorId) onDoorId)
  unlock,
  bool autoStart = false,
}) {
  unawaited(HapticFeedback.lightImpact());
  return showGeneralDialog<void>(
    context: context,
    // The card handles its own dismissal so it can refuse while an unlock is running.
    barrierDismissible: false,
    barrierColor: const Color(0x66000000),
    transitionDuration: Motion.of(context, const Duration(milliseconds: 520)),
    pageBuilder: (context, _, _) => UnlockSheet(
      initialDoorName: initialDoorName,
      doorNameFor: doorNameFor,
      keyExpiresAt: keyExpiresAt,
      unlock: unlock,
      autoStart: autoStart,
    ),
    transitionBuilder: (context, animation, _, child) {
      if (Motion.reduced(context)) {
        return FadeTransition(opacity: animation, child: child);
      }
      // Rises from the bottom edge with a slight overshoot, like the system card.
      final rise = CurvedAnimation(
        parent: animation,
        curve: Curves.easeOutBack,
        reverseCurve: Curves.easeInCubic,
      );
      return FadeTransition(
        opacity: CurvedAnimation(
          parent: animation,
          curve: const Interval(0, 0.4),
        ),
        child: SlideTransition(
          position: Tween(
            begin: const Offset(0, 0.6),
            end: Offset.zero,
          ).animate(rise),
          child: child,
        ),
      );
    },
  );
}

enum _Phase { ready, unlocking, opened, failed }

/// The card's colours: the system's grouped surfaces, light or dark with the phone.
class _Look {
  const _Look._({
    required this.card,
    required this.ink,
    required this.secondary,
    required this.chip,
    required this.button,
    required this.buttonInk,
  });

  factory _Look.of(BuildContext context) =>
      MediaQuery.platformBrightnessOf(context) == Brightness.dark
      ? const _Look._(
          card: Color(0xFF1C1C1E),
          ink: Color(0xFFFFFFFF),
          secondary: Color(0x99EBEBF5),
          chip: Color(0xFF2C2C2E),
          button: Color(0xFFFFFFFF),
          buttonInk: Color(0xFF000000),
        )
      : const _Look._(
          card: Color(0xFFFFFFFF),
          ink: Color(0xFF000000),
          secondary: Color(0x993C3C43),
          chip: Color(0xFFE9E9EB),
          button: Color(0xFF000000),
          buttonInk: Color(0xFFFFFFFF),
        );

  final Color card;
  final Color ink;
  final Color secondary;
  final Color chip;
  final Color button;
  final Color buttonInk;

  static const Color green = Color(0xFF30D158);
}

/// The system face on each platform rather than the console's.
String? get _systemFont =>
    defaultTargetPlatform == TargetPlatform.iOS ? 'CupertinoSystemText' : null;

class UnlockSheet extends StatefulWidget {
  const UnlockSheet({
    super.key,
    required this.initialDoorName,
    required this.doorNameFor,
    required this.keyExpiresAt,
    required this.unlock,
    this.autoStart = false,
    this.closeDelay = const Duration(milliseconds: 1600),
  });

  final String initialDoorName;
  final String? Function(String doorId) doorNameFor;
  final DateTime keyExpiresAt;
  final Future<UnlockOutcome> Function(void Function(String doorId) onDoorId)
  unlock;
  final bool autoStart;

  /// How long "Open" stays before the card closes.
  final Duration closeDelay;

  @override
  State<UnlockSheet> createState() => _UnlockSheetState();
}

class _UnlockSheetState extends State<UnlockSheet>
    with TickerProviderStateMixin {
  _Phase _phase = _Phase.ready;
  UnlockOutcome? _outcome;
  late String _doorName = widget.initialDoorName;
  Timer? _closeTimer;
  Timer? _beacon;

  // The door turns slowly while the card is up, and swings on its hinge when it opens.
  late final AnimationController _turn = AnimationController(
    vsync: this,
    duration: const Duration(seconds: 9),
  );
  // Unbounded: the spring overshoots 1 and settles back, like a door caught by its closer.
  late final AnimationController _swing = AnimationController.unbounded(
    vsync: this,
  );

  // Drag state: the card follows the finger down, resists upward, and settles back or leaves.
  late final AnimationController _settle = AnimationController(vsync: this);
  double _drag = 0;

  @override
  void initState() {
    super.initState();
    if (widget.autoStart) {
      WidgetsBinding.instance.addPostFrameCallback((_) => _start());
    }
  }

  @override
  void didChangeDependencies() {
    super.didChangeDependencies();
    // A still door for reduced motion; otherwise it keeps turning.
    if (Motion.reduced(context)) {
      _turn.stop();
    } else if (!_turn.isAnimating) {
      unawaited(_turn.repeat());
    }
  }

  @override
  void dispose() {
    _closeTimer?.cancel();
    _beacon?.cancel();
    _turn.dispose();
    _swing.dispose();
    _settle.dispose();
    super.dispose();
  }

  bool get _locked => _phase == _Phase.unlocking;

  void _close() {
    if (mounted && Navigator.of(context).canPop()) Navigator.of(context).pop();
  }

  void _dismiss() {
    if (!_locked) _close();
  }

  Future<void> _start() async {
    if (_locked) return;
    unawaited(HapticFeedback.selectionClick());
    _startBeacon();
    setState(() {
      _phase = _Phase.unlocking;
      _outcome = null;
    });
    final outcome = await widget.unlock((doorId) {
      if (!mounted) return;
      // The door answered: one firmer pulse, then the beacon carries on.
      unawaited(HapticFeedback.mediumImpact());
      setState(() => _doorName = widget.doorNameFor(doorId) ?? doorId);
    });
    _beacon?.cancel();
    if (!mounted) return;
    setState(() {
      _outcome = outcome;
      _phase = outcome is Opened ? _Phase.opened : _Phase.failed;
    });
    if (outcome is Opened) {
      unawaited(_latchClick());
      unawaited(
        _swing.animateWith(
          SpringSimulation(
            // Response 0.5 s, damping 0.75: a little bounce, earned by the push.
            SpringDescription.withDampingRatio(
              mass: 1,
              stiffness: math.pow(2 * math.pi / 0.5, 2).toDouble(),
              ratio: 0.75,
            ),
            0,
            1,
            0,
          ),
        ),
      );
      _closeTimer = Timer(widget.closeDelay, _close);
    } else {
      unawaited(HapticFeedback.vibrate());
    }
  }

  /// While the phone talks to the door: a soft double pulse every 0.9 s, like a beacon.
  void _startBeacon() {
    _beacon?.cancel();
    void pulse() {
      unawaited(HapticFeedback.lightImpact());
      Timer(const Duration(milliseconds: 110), () {
        if (_locked) unawaited(HapticFeedback.selectionClick());
      });
    }

    _beacon = Timer.periodic(const Duration(milliseconds: 900), (_) {
      if (_locked) pulse();
    });
  }

  /// The latch letting go: a hard click, then the lighter knock of the door leaving the frame.
  Future<void> _latchClick() async {
    await HapticFeedback.heavyImpact();
    await Future<void>.delayed(const Duration(milliseconds: 70));
    await HapticFeedback.lightImpact();
  }

  // ---------------------------------------------------------------- drag

  void _dragUpdate(DragUpdateDetails details) {
    if (_locked) return;
    _settle.stop();
    setState(() {
      final next = _drag + details.delta.dy;
      _drag = next >= 0 ? next : _drag + details.delta.dy * 0.2;
      if (_drag < -20) _drag = -20;
    });
  }

  void _dragEnd(DragEndDetails details) {
    if (_locked) return;
    final velocity = details.velocity.pixelsPerSecond.dy;
    if (_drag > 120 || velocity > 700) {
      _close();
      return;
    }
    final from = _drag;
    _settle
      ..duration = Motion.of(context, Motion.page)
      ..reset();
    final curve = CurvedAnimation(parent: _settle, curve: Curves.easeOutBack);
    void tick() => setState(() => _drag = from * (1 - curve.value));
    _settle.addListener(tick);
    _settle.forward().whenComplete(() {
      _settle.removeListener(tick);
      if (mounted) setState(() => _drag = 0);
    });
  }

  // ---------------------------------------------------------------- build

  @override
  Widget build(BuildContext context) {
    final look = _Look.of(context);
    final bottom = MediaQuery.paddingOf(context).bottom;
    return Semantics(
      scopesRoute: true,
      namesRoute: true,
      explicitChildNodes: true,
      label: 'Unlock $_doorName',
      child: Stack(
        children: [
          // Outside the card: a tap closes unless an unlock is running.
          Positioned.fill(
            child: Semantics(
              button: true,
              label: 'Dismiss',
              excludeSemantics: true,
              onTap: _locked ? null : _dismiss,
              child: GestureDetector(
                behavior: HitTestBehavior.opaque,
                onTap: _dismiss,
              ),
            ),
          ),
          Align(
            alignment: Alignment.bottomCenter,
            child: Padding(
              padding: EdgeInsets.fromLTRB(
                8,
                0,
                8,
                bottom > 0 ? bottom - 18 + 8 : 8,
              ),
              child: GestureDetector(
                onVerticalDragUpdate: _dragUpdate,
                onVerticalDragEnd: _dragEnd,
                child: Transform.translate(
                  offset: Offset(0, _drag),
                  child: ConstrainedBox(
                    constraints: const BoxConstraints(maxWidth: 440),
                    child: _card(context, look),
                  ),
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }

  Widget _card(BuildContext context, _Look look) {
    return DecoratedBox(
      decoration: ShapeDecoration(
        color: look.card,
        shape: RoundedSuperellipseBorder(
          borderRadius: BorderRadius.circular(44),
        ),
        shadows: const [
          BoxShadow(
            color: Color(0x33000000),
            blurRadius: 40,
            offset: Offset(0, 12),
          ),
        ],
      ),
      child: DefaultTextStyle(
        style: TextStyle(
          fontFamily: _systemFont,
          color: look.ink,
          decoration: TextDecoration.none,
          letterSpacing: -0.2,
        ),
        child: Padding(
          padding: const EdgeInsets.fromLTRB(24, 14, 24, 24),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              Row(
                children: [
                  const SizedBox(width: 30),
                  Expanded(
                    child: Semantics(
                      header: true,
                      child: Text(
                        _doorName,
                        textAlign: TextAlign.center,
                        maxLines: 1,
                        overflow: TextOverflow.ellipsis,
                        style: const TextStyle(
                          fontSize: 22,
                          height: 1.2,
                          fontWeight: FontWeight.w700,
                          letterSpacing: -0.4,
                        ),
                      ),
                    ),
                  ),
                  _CloseButton(
                    look: look,
                    onPressed: _locked ? null : _dismiss,
                  ),
                ],
              ),
              const SizedBox(height: 6),
              _door(look),
              const SizedBox(height: 6),
              _status(look),
              const SizedBox(height: 20),
              _action(look),
            ],
          ),
        ),
      ),
    );
  }

  Widget _door(_Look look) {
    return ExcludeSemantics(
      child: SizedBox(
        height: 210,
        child: AnimatedBuilder(
          animation: Listenable.merge([_turn, _swing]),
          builder: (context, _) {
            final settled = _swing.value.clamp(0.0, 1.0);
            return CustomPaint(
              painter: Door3dPainter(
                // Sways around a three-quarter view, never edge-on; settles front-on to swing.
                yaw: lerpAngle(
                  0.5 + 0.55 * math.sin(_turn.value * 2 * math.pi),
                  0.35,
                  settled,
                ),
                open: _swing.value * 1.1,
                dark:
                    MediaQuery.platformBrightnessOf(context) == Brightness.dark,
                glow: _phase == _Phase.opened
                    ? _Look.green
                    : _phase == _Phase.unlocking
                    ? const Color(0xFF0A84FF)
                    : null,
              ),
            );
          },
        ),
      ),
    );
  }

  Widget _status(_Look look) {
    final expires = widget.keyExpiresAt;
    final failed = _outcome;
    final text = switch (_phase) {
      _Phase.ready => 'Key valid until ${spokenDate(expires)}',
      _Phase.unlocking => 'Talking to the door…',
      _Phase.opened => 'Door open',
      _Phase.failed => switch (failed) {
        Refused(:final sentence) => sentence,
        Failed(:final sentence) => sentence,
        _ => '',
      },
    };
    return Semantics(
      liveRegion: true,
      child: AnimatedSwitcher(
        duration: Motion.of(context, Motion.base),
        child: Text(
          text,
          key: ValueKey(text),
          textAlign: TextAlign.center,
          style: TextStyle(
            fontSize: 15,
            height: 1.35,
            color: _phase == _Phase.opened ? _Look.green : look.secondary,
            fontWeight: _phase == _Phase.opened
                ? FontWeight.w600
                : FontWeight.w400,
          ),
        ),
      ),
    );
  }

  Widget _action(_Look look) {
    final retry = switch (_outcome) {
      Refused(:final canRetry) => canRetry,
      Failed(:final canRetry) => canRetry,
      _ => false,
    };
    return switch (_phase) {
      _Phase.ready => _PillButton(
        label: 'Unlock',
        look: look,
        onPressed: _start,
      ),
      _Phase.unlocking => _PillButton(
        label: 'Unlocking…',
        look: look,
        busy: true,
      ),
      _Phase.opened => _PillButton(
        label: 'Open',
        look: look,
        color: _Look.green,
        ink: const Color(0xFFFFFFFF),
        done: true,
      ),
      _Phase.failed => _PillButton(
        label: retry ? 'Try again' : 'Close',
        look: look,
        onPressed: retry ? _start : _close,
      ),
    };
  }
}

/// Rotates from [from] toward [to] (radians) by [t], the short way round.
double lerpAngle(double from, double to, double t) {
  var delta = (to - from) % (2 * math.pi);
  if (delta > math.pi) delta -= 2 * math.pi;
  if (delta < -math.pi) delta += 2 * math.pi;
  return from + delta * Curves.easeOutCubic.transform(t);
}

/// The round gray × in the corner of a system card.
class _CloseButton extends StatelessWidget {
  const _CloseButton({required this.look, required this.onPressed});
  final _Look look;
  final VoidCallback? onPressed;

  @override
  Widget build(BuildContext context) {
    return Semantics(
      button: true,
      label: 'Close',
      excludeSemantics: true,
      child: GestureDetector(
        onTap: onPressed,
        child: AnimatedOpacity(
          opacity: onPressed == null ? 0.35 : 1,
          duration: Motion.of(context, Motion.base),
          child: SizedBox.square(
            dimension: 30,
            child: DecoratedBox(
              decoration: BoxDecoration(
                color: look.chip,
                shape: BoxShape.circle,
              ),
              child: CustomPaint(painter: _CrossPainter(look.secondary)),
            ),
          ),
        ),
      ),
    );
  }
}

class _CheckPainter extends CustomPainter {
  const _CheckPainter(this.color);
  final Color color;

  @override
  void paint(Canvas canvas, Size size) {
    canvas.drawPath(
      Path()
        ..moveTo(size.width * 0.16, size.height * 0.52)
        ..lineTo(size.width * 0.4, size.height * 0.76)
        ..lineTo(size.width * 0.86, size.height * 0.26),
      Paint()
        ..color = color
        ..style = PaintingStyle.stroke
        ..strokeWidth = 2.6
        ..strokeCap = StrokeCap.round
        ..strokeJoin = StrokeJoin.round,
    );
  }

  @override
  bool shouldRepaint(_CheckPainter old) => old.color != color;
}

class _CrossPainter extends CustomPainter {
  const _CrossPainter(this.color);
  final Color color;

  @override
  void paint(Canvas canvas, Size size) {
    final paint = Paint()
      ..color = color
      ..strokeWidth = 2.2
      ..strokeCap = StrokeCap.round;
    final c = size.center(Offset.zero);
    const r = 4.5;
    canvas
      ..drawLine(c + const Offset(-r, -r), c + const Offset(r, r), paint)
      ..drawLine(c + const Offset(r, -r), c + const Offset(-r, r), paint);
  }

  @override
  bool shouldRepaint(_CrossPainter old) => old.color != color;
}

/// The wide pill a system card ends with. Presses down on touch, not on release.
class _PillButton extends StatefulWidget {
  const _PillButton({
    required this.label,
    required this.look,
    this.onPressed,
    this.busy = false,
    this.done = false,
    this.color,
    this.ink,
  });

  final String label;
  final _Look look;
  final VoidCallback? onPressed;
  final bool busy;
  final bool done;
  final Color? color;
  final Color? ink;

  @override
  State<_PillButton> createState() => _PillButtonState();
}

class _PillButtonState extends State<_PillButton> {
  bool _down = false;

  @override
  Widget build(BuildContext context) {
    final look = widget.look;
    final color = widget.color ?? look.button;
    final ink = widget.ink ?? look.buttonInk;
    final enabled = widget.onPressed != null;
    return Semantics(
      button: true,
      enabled: enabled,
      label: widget.label,
      excludeSemantics: true,
      child: GestureDetector(
        onTapDown: enabled ? (_) => setState(() => _down = true) : null,
        onTapCancel: () => setState(() => _down = false),
        onTapUp: (_) => setState(() => _down = false),
        onTap: widget.onPressed,
        child: AnimatedScale(
          scale: _down ? 0.97 : 1,
          duration: const Duration(milliseconds: 100),
          child: AnimatedContainer(
            duration: Motion.of(context, Motion.base),
            curve: Motion.curve,
            height: 54,
            decoration: ShapeDecoration(
              color: widget.busy ? color.withValues(alpha: 0.75) : color,
              shape: RoundedSuperellipseBorder(
                borderRadius: BorderRadius.circular(18),
              ),
            ),
            alignment: Alignment.center,
            child: Row(
              mainAxisSize: MainAxisSize.min,
              children: [
                if (widget.busy) ...[
                  SizedBox.square(
                    dimension: 16,
                    child: CircularProgressIndicator(
                      strokeWidth: 2,
                      color: ink,
                    ),
                  ),
                  const SizedBox(width: 10),
                ],
                if (widget.done) ...[
                  SizedBox.square(
                    dimension: 18,
                    child: CustomPaint(painter: _CheckPainter(ink)),
                  ),
                  const SizedBox(width: 6),
                ],
                Text(
                  widget.label,
                  style: TextStyle(
                    fontFamily: _systemFont,
                    fontSize: 17,
                    fontWeight: FontWeight.w600,
                    color: ink,
                    letterSpacing: -0.3,
                    decoration: TextDecoration.none,
                  ),
                ),
              ],
            ),
          ),
        ),
      ),
    );
  }
}
