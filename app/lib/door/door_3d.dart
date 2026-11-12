import 'dart:math' as math;

import 'package:flutter/rendering.dart';

/// A small 3D door in its frame, drawn without a 3D engine: boxes projected in perspective and lit
/// by one light. It turns with [yaw] and swings on its hinge with [open] (radians).
///
/// Each box is convex, so its own faces never overlap once back faces are culled. Between boxes
/// the order is decided per object, not per face: the frame and the door by depth, and inside the
/// door the far side's panels and handle, then the slab, then the near side's. Sorting single
/// faces instead lets a panel's edge sort in front of the slab it sits on.
class Door3dPainter extends CustomPainter {
  Door3dPainter({
    required this.yaw,
    required this.open,
    required this.dark,
    this.glow,
  });

  final double yaw;
  final double open;
  final bool dark;

  /// Lights the handle: blue while talking to the door, green once open.
  final Color? glow;

  static const double _w = 0.5; // half width of the leaf
  static const double _h = 1.0; // half height
  static const double _d = 0.06; // half thickness
  static const double _fw = 0.07; // frame width
  static const double _fd = 0.1; // frame half depth
  static const double _pitch = -0.18;
  static const double camera = 4.5;
  static final _light = _norm(const _V(-0.45, 0.6, 0.9));

  @override
  void paint(Canvas canvas, Size size) {
    final scale = size.height * 0.38;
    final origin = Offset(size.width / 2, size.height * 0.46);

    final shadowRect = Rect.fromCenter(
      center: origin + Offset(0, scale * 1.2),
      width: scale * (1.0 + 0.4 * math.cos(yaw).abs()),
      height: scale * 0.12,
    );
    canvas.drawOval(
      shadowRect,
      Paint()
        ..color = Color(dark ? 0x99000000 : 0x40000000)
        ..maskFilter = MaskFilter.blur(BlurStyle.normal, scale * 0.08),
    );

    final leaf = dark ? const Color(0xFF61656F) : const Color(0xFF30333A);
    final panel = dark ? const Color(0xFF6E727D) : const Color(0xFF3B3F47);
    final frame = dark ? const Color(0xFF8A8E98) : const Color(0xFF4B4F58);
    final metal = glow ?? const Color(0xFFD9DDE3);

    void draw(_Box b) {
      for (final face in b.faces()) {
        final path = Path()
          ..addPolygon([
            for (final p in face.points) origin + _project(p, scale),
          ], true);
        final lit = 0.3 + 0.7 * math.max(0, _dot(face.normal, _light));
        canvas
          ..drawPath(
            path,
            Paint()..color = Color.lerp(const Color(0xFF000000), b.color, lit)!,
          )
          ..drawPath(
            path,
            Paint()
              ..style = PaintingStyle.stroke
              ..strokeWidth = 0.5
              ..color = const Color(0x1AFFFFFF),
          );
      }
    }

    // The frame: two jambs and a head, fixed.
    final frameBoxes = [
      _Box(
        _place,
        const _V(-_w - _fw, -_fw / 2, 0),
        const _V(_fw, _h + _fw / 2, _fd),
        frame,
      ),
      _Box(
        _place,
        const _V(_w + _fw, -_fw / 2, 0),
        const _V(_fw, _h + _fw / 2, _fd),
        frame,
      ),
      _Box(
        _place,
        const _V(0, _h + _fw / 2, 0),
        const _V(_w + 2 * _fw, _fw / 2, _fd),
        frame,
      ),
    ];

    // The leaf with what is fixed to each side of it, in leaf space (hinge on the y axis).
    _Box onLeaf(_V c, _V e, Color color) => _Box(_leaf, c, e, color);
    final slab = onLeaf(
      const _V(_w, 0, 0),
      const _V(_w - 0.004, _h - 0.004, _d),
      leaf,
    );
    List<_Box> side(double z) => [
      onLeaf(
        _V(_w, 0.46, z * (_d + 0.008)),
        const _V(_w * 0.68, 0.38, 0.008),
        panel,
      ),
      onLeaf(
        _V(_w, -0.5, z * (_d + 0.008)),
        const _V(_w * 0.68, 0.34, 0.008),
        panel,
      ),
      // The lever: a rose on the face and a bar pointing at the hinge.
      onLeaf(
        _V(2 * _w - 0.13, -0.04, z * (_d + 0.02)),
        const _V(0.035, 0.035, 0.02),
        metal,
      ),
      onLeaf(
        _V(2 * _w - 0.21, -0.04, z * (_d + 0.052)),
        const _V(0.11, 0.018, 0.016),
        metal,
      ),
    ];
    // Which side of the leaf faces the camera.
    final frontNear =
        _leaf(const _V(_w, 0, 1)).z > _leaf(const _V(_w, 0, -1)).z;
    final door = [
      ...side(frontNear ? -1 : 1),
      slab,
      ...side(frontNear ? 1 : -1),
    ];

    // Far to near: the frame pieces and the door as a whole, by their centres.
    final items = <(double, List<_Box>)>[
      for (final b in frameBoxes) (b.centreDepth, [b]),
      (slab.centreDepth, door),
    ]..sort((a, b) => a.$1.compareTo(b.$1));
    for (final (_, boxes) in items) {
      boxes.forEach(draw);
    }

    if (glow != null) {
      final handle = _leaf(
        _V(2 * _w - 0.17, -0.04, (frontNear ? 1 : -1) * (_d + 0.05)),
      );
      final at = origin + _project(handle, scale);
      final r = scale * 0.32;
      canvas.drawCircle(
        at,
        r,
        Paint()
          ..shader = RadialGradient(
            colors: [glow!.withValues(alpha: 0.5), glow!.withValues(alpha: 0)],
          ).createShader(Rect.fromCircle(center: at, radius: r)),
      );
    }
  }

  /// Centred door space to view space: turn, then tilt.
  _V _place(_V p) => _rotX(_rotY(p, yaw), _pitch);

  /// Leaf space (hinge on the y axis at the frame's left edge) to view space.
  _V _leaf(_V p) {
    final swung = _rotY(p, -open);
    return _place(_V(swung.x - _w, swung.y, swung.z));
  }

  Offset _project(_V p, double scale) {
    final f = camera / (camera - p.z);
    return Offset(p.x * f * scale, -p.y * f * scale);
  }

  @override
  bool shouldRepaint(Door3dPainter old) =>
      old.yaw != yaw ||
      old.open != open ||
      old.dark != dark ||
      old.glow != glow;
}

class _Box {
  _Box(this.toView, this.c, this.e, this.color);

  final _V Function(_V) toView;
  final _V c;
  final _V e;
  final Color color;

  double get centreDepth => toView(c).z;

  /// The faces that look at the camera, in view space.
  List<_Face> faces() {
    _V k(double sx, double sy, double sz) =>
        toView(_V(c.x + sx * e.x, c.y + sy * e.y, c.z + sz * e.z));
    final quads = [
      [k(-1, -1, 1), k(1, -1, 1), k(1, 1, 1), k(-1, 1, 1)],
      [k(1, -1, -1), k(-1, -1, -1), k(-1, 1, -1), k(1, 1, -1)],
      [k(1, -1, 1), k(1, -1, -1), k(1, 1, -1), k(1, 1, 1)],
      [k(-1, -1, -1), k(-1, -1, 1), k(-1, 1, 1), k(-1, 1, -1)],
      [k(-1, 1, 1), k(1, 1, 1), k(1, 1, -1), k(-1, 1, -1)],
      [k(-1, -1, -1), k(1, -1, -1), k(1, -1, 1), k(-1, -1, 1)],
    ];
    final out = <_Face>[];
    for (final q in quads) {
      final n = _norm(_cross(_sub(q[1], q[0]), _sub(q[2], q[0])));
      final mid = _scaleV(q.fold(_V.zero, _add), 0.25);
      if (_dot(n, _sub(const _V(0, 0, Door3dPainter.camera), mid)) <= 0) {
        continue;
      }
      out.add(_Face(q, n));
    }
    return out;
  }
}

class _Face {
  const _Face(this.points, this.normal);
  final List<_V> points;
  final _V normal;
}

class _V {
  const _V(this.x, this.y, this.z);
  static const zero = _V(0, 0, 0);
  final double x, y, z;
}

_V _add(_V a, _V b) => _V(a.x + b.x, a.y + b.y, a.z + b.z);
_V _sub(_V a, _V b) => _V(a.x - b.x, a.y - b.y, a.z - b.z);
_V _scaleV(_V a, double s) => _V(a.x * s, a.y * s, a.z * s);
double _dot(_V a, _V b) => a.x * b.x + a.y * b.y + a.z * b.z;
_V _cross(_V a, _V b) =>
    _V(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
_V _norm(_V a) {
  final l = math.sqrt(_dot(a, a));
  return _V(a.x / l, a.y / l, a.z / l);
}

_V _rotY(_V p, double a) {
  final c = math.cos(a), s = math.sin(a);
  return _V(p.x * c + p.z * s, p.y, -p.x * s + p.z * c);
}

_V _rotX(_V p, double a) {
  final c = math.cos(a), s = math.sin(a);
  return _V(p.x, p.y * c - p.z * s, p.y * s + p.z * c);
}
