import 'dart:async';

import 'package:flutter/services.dart';
import 'package:material_ui/material_ui.dart';

import '../keys/format.dart';
import '../theme/theme.dart';
import '../theme/widgets.dart';
import 'door_unlocker.dart';

/// Shows the unlock prompt: the web's inverted light surface rising from the bottom. No app can put
/// a system card over the home screen; this is the closest honest version.
///
/// [unlock] runs the sequence and reports the door's id when its challenge arrives. Resolves when
/// the sheet has closed.
Future<void> showUnlockSheet(
  BuildContext context, {
  required String initialDoorName,
  required String? Function(String doorId) doorNameFor,
  required DateTime keyExpiresAt,
  required Future<UnlockOutcome> Function(void Function(String doorId) onDoorId)
  unlock,
  bool autoStart = false,
}) {
  return showModalBottomSheet<void>(
    context: context,
    useSafeArea: true,
    isScrollControlled: true,
    // The sheet handles its own dismissal so it can refuse while an unlock is running.
    isDismissible: false,
    enableDrag: false,
    backgroundColor: const Color(0x00000000),
    barrierColor: const Color(0x99000000),
    builder: (context) => UnlockSheet(
      initialDoorName: initialDoorName,
      doorNameFor: doorNameFor,
      keyExpiresAt: keyExpiresAt,
      unlock: unlock,
      autoStart: autoStart,
    ),
  );
}

enum _Phase { ready, unlocking, opened, failed }

class UnlockSheet extends StatefulWidget {
  const UnlockSheet({
    super.key,
    required this.initialDoorName,
    required this.doorNameFor,
    required this.keyExpiresAt,
    required this.unlock,
    this.autoStart = false,
    this.closeDelay = const Duration(milliseconds: 1500),
  });

  final String initialDoorName;
  final String? Function(String doorId) doorNameFor;
  final DateTime keyExpiresAt;
  final Future<UnlockOutcome> Function(void Function(String doorId) onDoorId)
  unlock;
  final bool autoStart;

  /// How long "Door open" stays before the sheet closes.
  final Duration closeDelay;

  @override
  State<UnlockSheet> createState() => _UnlockSheetState();
}

class _UnlockSheetState extends State<UnlockSheet>
    with SingleTickerProviderStateMixin {
  _Phase _phase = _Phase.ready;
  UnlockOutcome? _outcome;
  late String _doorName = widget.initialDoorName;
  Timer? _closeTimer;

  // Drag state: the sheet follows the finger down, resists upward, and settles back or leaves.
  late final AnimationController _settle = AnimationController(vsync: this);
  double _drag = 0;
  double _height = 0;

  @override
  void initState() {
    super.initState();
    if (widget.autoStart) {
      WidgetsBinding.instance.addPostFrameCallback((_) => _start());
    }
  }

  @override
  void dispose() {
    _closeTimer?.cancel();
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
    setState(() {
      _phase = _Phase.unlocking;
      _outcome = null;
    });
    final outcome = await widget.unlock((doorId) {
      if (!mounted) return;
      setState(() => _doorName = widget.doorNameFor(doorId) ?? doorId);
    });
    if (!mounted) return;
    setState(() {
      _outcome = outcome;
      _phase = outcome is Opened ? _Phase.opened : _Phase.failed;
    });
    if (outcome is Opened) {
      unawaited(HapticFeedback.heavyImpact());
      _closeTimer = Timer(widget.closeDelay, _close);
    }
  }

  // ---------------------------------------------------------------- drag

  void _dragUpdate(DragUpdateDetails details) {
    if (_locked) return;
    _settle.stop();
    setState(() {
      final next = _drag + details.delta.dy;
      // Down follows the finger; up resists, like the system sheets.
      _drag = next >= 0 ? next : _drag + details.delta.dy * 0.25;
      if (_drag < -24) _drag = -24;
    });
  }

  void _dragEnd(DragEndDetails details) {
    if (_locked) return;
    final velocity = details.velocity.pixelsPerSecond.dy;
    if (_drag > _height * 0.3 || velocity > 700) {
      _close();
      return;
    }
    final from = _drag;
    _settle
      ..duration = Motion.of(context, Motion.base)
      ..reset();
    final curve = CurvedAnimation(parent: _settle, curve: Motion.curve);
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
    final palette = Palette.sheet;
    return DooraTheme(
      palette: palette,
      child: Semantics(
        scopesRoute: true,
        namesRoute: true,
        explicitChildNodes: true,
        label: 'Unlock $_doorName',
        child: Stack(
          children: [
            // The scrim: a tap closes unless an unlock is running.
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
              child: GestureDetector(
                onVerticalDragUpdate: _dragUpdate,
                onVerticalDragEnd: _dragEnd,
                child: Transform.translate(
                  offset: Offset(0, _drag),
                  child: LayoutBuilder(
                    builder: (context, constraints) {
                      return _SheetBody(
                        onLayout: (height) => _height = height,
                        child: _content(context, palette),
                      );
                    },
                  ),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _content(BuildContext context, Palette c) {
    final type = context.type;
    final failed = _outcome;
    final retry = switch (failed) {
      Refused(:final canRetry) => canRetry,
      Failed(:final canRetry) => canRetry,
      _ => false,
    };
    final sentence = switch (failed) {
      Refused(:final sentence) => sentence,
      Failed(:final sentence) => sentence,
      _ => '',
    };
    final expires = widget.keyExpiresAt;

    return Column(
      mainAxisSize: MainAxisSize.min,
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        // The drag handle.
        Padding(
          padding: const EdgeInsets.only(top: 10, bottom: 14),
          child: Center(
            child: ExcludeSemantics(
              child: SizedBox(
                width: 36,
                height: 4,
                child: ColoredBox(color: c.gray300),
              ),
            ),
          ),
        ),
        Padding(
          padding: const EdgeInsets.symmetric(horizontal: 20),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.stretch,
            children: [
              Semantics(
                header: true,
                child: Text(_doorName, style: type.heading),
              ),
              const SizedBox(height: 6),
              Semantics(
                label: 'Key valid until ${spokenDate(expires)}',
                excludeSemantics: true,
                child: Text(
                  'Key valid until ${formatDateTime(expires)}',
                  style: type.muted,
                ),
              ),
              const SizedBox(height: 20),
              _status(context, c, sentence),
              const SizedBox(height: 20),
              ..._actions(context, retry),
            ],
          ),
        ),
        const SizedBox(height: 16),
      ],
    );
  }

  Widget _status(BuildContext context, Palette c, String sentence) {
    final type = context.type;
    Widget line(LampState? lamp, String text) => Semantics(
      liveRegion: true,
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.center,
        children: [
          if (lamp != null) ...[Lamp(lamp), const SizedBox(width: 10)],
          Expanded(child: Text(text, style: type.body)),
        ],
      ),
    );
    return AnimatedSize(
      duration: Motion.of(context, Motion.base),
      curve: Motion.curve,
      alignment: Alignment.topCenter,
      child: switch (_phase) {
        _Phase.ready => const SizedBox(width: double.infinity),
        _Phase.unlocking => line(LampState.blueWaiting, 'Talking to the door…'),
        _Phase.opened => line(LampState.green, 'Door open'),
        // The failure is a sentence and a lit lamp: no tinted box, and the sentence stays in ink
        // because the sheet's red is under 4.5:1.
        _Phase.failed => line(LampState.amber, sentence),
      },
    );
  }

  List<Widget> _actions(BuildContext context, bool retry) {
    final c = context.colors;
    switch (_phase) {
      case _Phase.ready:
        return [
          DooraButton(
            label: 'Unlock',
            kind: ButtonKind.primary,
            onPressed: _start,
          ),
          Center(
            child: DooraTextButton(
              label: 'Not now',
              color: c.gray600,
              onPressed: _dismiss,
            ),
          ),
        ];
      case _Phase.unlocking:
        return [
          const DooraButton(
            label: 'Unlocking…',
            kind: ButtonKind.primary,
            onPressed: null,
          ),
        ];
      case _Phase.opened:
        return const [];
      case _Phase.failed:
        return retry
            ? [
                DooraButton(
                  label: 'Try again',
                  kind: ButtonKind.primary,
                  onPressed: _start,
                ),
                Center(
                  child: DooraTextButton(
                    label: 'Close',
                    color: c.gray600,
                    onPressed: _close,
                  ),
                ),
              ]
            : [
                DooraButton(
                  label: 'Close',
                  kind: ButtonKind.primary,
                  onPressed: _close,
                ),
              ];
    }
  }
}

/// The sheet's surface: `void` of the inverted palette, a hairline on top, square corners. It
/// reports its height so a drag knows what 30 % is.
class _SheetBody extends StatelessWidget {
  const _SheetBody({required this.child, required this.onLayout});

  final Widget child;
  final ValueChanged<double> onLayout;

  @override
  Widget build(BuildContext context) {
    final c = context.colors;
    return DecoratedBox(
      decoration: BoxDecoration(
        color: c.voidSurface,
        border: Border(top: BorderSide(color: c.gray200)),
      ),
      child: _Measure(
        onLayout: onLayout,
        child: SafeArea(top: false, child: child),
      ),
    );
  }
}

class _Measure extends StatelessWidget {
  const _Measure({required this.child, required this.onLayout});
  final Widget child;
  final ValueChanged<double> onLayout;

  @override
  Widget build(BuildContext context) {
    return NotificationListener<SizeChangedLayoutNotification>(
      onNotification: (_) => true,
      child: SizeChangedLayoutNotifier(
        child: LayoutBuilder(
          builder: (context, constraints) {
            WidgetsBinding.instance.addPostFrameCallback((_) {
              final box = context.findRenderObject();
              if (box is RenderBox && box.hasSize) onLayout(box.size.height);
            });
            return child;
          },
        ),
      ),
    );
  }
}
