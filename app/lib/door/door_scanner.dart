import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import 'ble_protocol.dart';
import 'proximity.dart';

/// A door that is near enough to offer an unlock.
class DoorSighting {
  const DoorSighting({required this.deviceId, required this.rssi});
  final String deviceId;
  final int rssi;
}

/// Why the app cannot look for the door right now.
enum BluetoothProblem { off, permissionDenied, unsupported }

/// Scans for the door's service UUID and reports the door once it is near ([ProximityTracker]).
/// Touching it for the first time on iOS shows the system's Bluetooth dialog, so the keys screen
/// explains first and starts it only afterwards.
class DoorScanner extends ChangeNotifier {
  DoorScanner({
    ProximityTracker Function()? tracker,
    DateTime Function()? clock,
  }) : _newTracker = tracker ?? ProximityTracker.new,
       _clock = clock ?? DateTime.now {
    _tracker = _newTracker();
  }

  final ProximityTracker Function() _newTracker;
  final DateTime Function() _clock;
  late ProximityTracker _tracker;

  StreamSubscription<List<ScanResult>>? _results;
  StreamSubscription<BluetoothAdapterState>? _adapterSub;
  Timer? _staleTimer;
  BluetoothAdapterState _adapter = BluetoothAdapterState.unknown;
  bool _wanted = false;
  bool _scanning = false;
  bool _background = false;
  DoorSighting? _near;

  /// The door that is near now.
  DoorSighting? get near => _near;
  bool get scanning => _scanning;

  BluetoothProblem? get problem => switch (_adapter) {
    BluetoothAdapterState.off ||
    BluetoothAdapterState.turningOff => BluetoothProblem.off,
    BluetoothAdapterState.unauthorized => BluetoothProblem.permissionDenied,
    BluetoothAdapterState.unavailable => BluetoothProblem.unsupported,
    _ => null,
  };

  /// In the background the system reports a discovery once, so one result is enough.
  set background(bool value) {
    if (_background == value) return;
    _background = value;
    _tracker = ProximityTracker(requiredResults: value ? 1 : 2);
    // iOS reports each device once per scan in the background, and a door already seen in the
    // foreground counts as seen: without a fresh scan the lock screen never hears it arrive.
    if (_scanning) unawaited(_restart());
  }

  Future<void> _restart() async {
    _near = null;
    notifyListeners();
    _scanning = false;
    try {
      await FlutterBluePlus.stopScan();
    } on Object {
      // Not scanning.
    }
    if (_wanted && _adapter == BluetoothAdapterState.on) await _scan();
  }

  /// Starts looking for the door, and keeps looking while Bluetooth allows.
  Future<void> start() async {
    _wanted = true;
    _adapterSub ??= FlutterBluePlus.adapterState.listen((state) {
      _adapter = state;
      notifyListeners();
      if (_wanted) {
        if (state == BluetoothAdapterState.on) {
          unawaited(_scan());
        } else {
          unawaited(_halt());
        }
      }
    });
    if (_adapter == BluetoothAdapterState.on) await _scan();
  }

  Future<void> _scan() async {
    if (_scanning) return;
    _scanning = true;
    _results ??= FlutterBluePlus.onScanResults.listen(
      _onResults,
      onError: (_) {},
    );
    _staleTimer ??= Timer.periodic(
      const Duration(seconds: 1),
      (_) => _refreshNear(),
    );
    try {
      await FlutterBluePlus.startScan(
        withServices: [Guid(doorServiceUuid)],
        continuousUpdates: true,
        androidScanMode: AndroidScanMode.lowLatency,
      );
    } on Object {
      _scanning = false;
    }
    notifyListeners();
  }

  void _onResults(List<ScanResult> results) {
    final now = _clock();
    for (final result in results) {
      _tracker.add(result.device.remoteId.str, result.rssi, now);
    }
    _refreshNear();
  }

  void _refreshNear() {
    final id = _tracker.nearAt(_clock());
    final next = id == null
        ? null
        : DoorSighting(deviceId: id, rssi: _tracker.lastRssi);
    if (next?.deviceId != _near?.deviceId || next?.rssi != _near?.rssi) {
      _near = next;
      notifyListeners();
    }
  }

  Future<void> _halt() async {
    _scanning = false;
    _tracker.reset();
    _near = null;
    try {
      await FlutterBluePlus.stopScan();
    } on Object {
      // Not scanning.
    }
    notifyListeners();
  }

  /// Stops looking. Scanning only runs while there is a usable credential.
  Future<void> stop() async {
    _wanted = false;
    _staleTimer?.cancel();
    _staleTimer = null;
    await _results?.cancel();
    _results = null;
    await _halt();
  }

  @override
  void dispose() {
    _staleTimer?.cancel();
    _results?.cancel();
    _adapterSub?.cancel();
    super.dispose();
  }
}
