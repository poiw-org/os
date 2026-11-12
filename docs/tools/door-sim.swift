// A fake door for testing the app without hardware: this Mac advertises as the door bridge
// (docs/ble-protocol.md), sends a challenge to the phone that subscribes, takes the request and
// answers "open". It does not check the signature or the credential.
//
// After each unlock it goes quiet, then advertises again 10 s later, so the phone sees the door
// arrive again. (The app itself waits 60 s before prompting again for the same door unless you
// tap Unlock on the row.)
//
//   docs/tools/door-sim.sh [door-id] [result-code]
//
// door-id defaults to "sim-door"; result-code defaults to 0 (open). See protocol section 3 for the
// other codes. The terminal running it needs Bluetooth access (System Settings > Privacy).
import CoreBluetooth
import Foundation

let serviceUUID = CBUUID(string: "7d0a0001-6f2b-4c1e-9a53-d00a00000001")
let charUUID = CBUUID(string: "7d0a0002-6f2b-4c1e-9a53-d00a00000001")
let args = CommandLine.arguments
let doorId = args.count > 1 ? args[1] : "sim-door"
let resultCode = UInt8(args.count > 2 ? Int(args[2]) ?? 0 : 0)
let cycle: TimeInterval = 10

func log(_ s: String) {
  let t = DateFormatter.localizedString(from: Date(), dateStyle: .none, timeStyle: .medium)
  print("[\(t)] \(s)")
}

final class Door: NSObject, CBPeripheralManagerDelegate {
  var manager: CBPeripheralManager!
  let characteristic = CBMutableCharacteristic(
    type: charUUID, properties: [.write, .notify], value: nil, permissions: [.writeable])
  var nonce = Data()
  var buffer = Data()
  var nextIndex = 0
  var pending: [Data] = []
  var unlocks = 0

  override init() {
    super.init()
    manager = CBPeripheralManager(delegate: self, queue: nil)
  }

  func peripheralManagerDidUpdateState(_ p: CBPeripheralManager) {
    guard p.state == .poweredOn else { log("Bluetooth state \(p.state.rawValue); waiting"); return }
    let service = CBMutableService(type: serviceUUID, primary: true)
    service.characteristics = [characteristic]
    p.add(service)
  }

  func peripheralManager(_ p: CBPeripheralManager, didAdd service: CBService, error: Error?) {
    if let error { log("Adding the service failed: \(error)"); exit(1) }
    advertise()
  }

  func advertise() {
    manager.startAdvertising([
      CBAdvertisementDataServiceUUIDsKey: [serviceUUID],
      CBAdvertisementDataLocalNameKey: "doora",
    ])
    log("Advertising as door \"\(doorId)\"")
  }

  func peripheralManager(_ p: CBPeripheralManager, central: CBCentral, didSubscribeTo c: CBCharacteristic) {
    log("Phone connected (max write \(central.maximumUpdateValueLength) bytes); sending challenge")
    var n = Data(count: 16)
    _ = n.withUnsafeMutableBytes { SecRandomCopyBytes(kSecRandomDefault, 16, $0.baseAddress!) }
    nonce = n
    buffer = Data()
    nextIndex = 0
    let id = Data(doorId.utf8)
    send(Data([0x01, 0x01]) + nonce + Data([UInt8(id.count)]) + id)
  }

  func peripheralManager(_ p: CBPeripheralManager, didReceiveWrite requests: [CBATTRequest]) {
    for r in requests {
      guard let v = r.value, let header = v.first else { continue }
      let index = Int(header & 0x7f)
      if index != nextIndex { log("Chunk \(index) out of order (expected \(nextIndex))"); reply(1); break }
      buffer.append(v.dropFirst())
      nextIndex += 1
      if header & 0x80 != 0 { handleRequest() }
    }
    p.respond(to: requests[0], withResult: .success)
  }

  func handleRequest() {
    let ok = buffer.count > 66 && buffer[buffer.startIndex] == 0x02 && buffer[buffer.startIndex + 1] == 0x01
    if !ok { log("Malformed request (\(buffer.count) bytes)"); reply(1); return }
    let jwt = String(decoding: buffer.dropFirst(66), as: UTF8.self)
    log("Request: 64-byte signature, credential \(jwt.prefix(24))… (\(jwt.count) chars)")
    reply(resultCode)
  }

  func reply(_ code: UInt8) {
    send(Data([0x03, code]))
    if code == 0 {
      unlocks += 1
      log("🔓 Door open (unlock #\(unlocks)). Going quiet for \(Int(cycle)) s")
    } else {
      log("Answered code \(code)")
    }
    manager.stopAdvertising()
    DispatchQueue.main.asyncAfter(deadline: .now() + cycle) { self.advertise() }
  }

  func send(_ d: Data) {
    if !manager.updateValue(d, for: characteristic, onSubscribedCentrals: nil) { pending.append(d) }
  }

  func peripheralManagerIsReady(toUpdateSubscribers p: CBPeripheralManager) {
    while let d = pending.first, p.updateValue(d, for: characteristic, onSubscribedCentrals: nil) {
      pending.removeFirst()
    }
  }

  func peripheralManager(_ p: CBPeripheralManager, central: CBCentral, didUnsubscribeFrom c: CBCharacteristic) {
    log("Phone disconnected")
  }
}

let door = Door()
RunLoop.main.run()
