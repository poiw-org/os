import Flutter
import ActivityKit
import Security
import UIKit

/// The phone's key pairs: P-256, in the Secure Enclave, one per virtual key. The private key never
/// leaves it. Channel `org.poiw.doora/device_key`. Nothing here logs keys, signatures or tokens.
final class DeviceKey {
  private static func tag(_ keyId: String) -> Data {
    Data("org.poiw.doora.\(keyId)".utf8)
  }

  private static func lookup(_ keyId: String) -> SecKey? {
    let query: [String: Any] = [
      kSecClass as String: kSecClassKey,
      kSecAttrApplicationTag as String: tag(keyId),
      kSecAttrKeyType as String: kSecAttrKeyTypeECSECPrimeRandom,
      kSecReturnRef as String: true,
    ]
    var item: CFTypeRef?
    guard SecItemCopyMatching(query as CFDictionary, &item) == errSecSuccess else { return nil }
    return (item as! SecKey)
  }

  private static func create(_ keyId: String) throws -> SecKey {
    var error: Unmanaged<CFError>?
    // No biometry flag: signing must work after a notification tap on a locked phone.
    #if targetEnvironment(simulator)
      // The simulator has no Secure Enclave: a software key, in simulator builds only.
      let flags: SecAccessControlCreateFlags = []
    #else
      let flags: SecAccessControlCreateFlags = .privateKeyUsage
    #endif
    guard let access = SecAccessControlCreateWithFlags(
      nil, kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly, flags, &error)
    else { throw error!.takeRetainedValue() }

    var privateAttributes: [String: Any] = [
      kSecAttrIsPermanent as String: true,
      kSecAttrApplicationTag as String: tag(keyId),
      kSecAttrAccessControl as String: access,
    ]
    var attributes: [String: Any] = [
      kSecAttrKeyType as String: kSecAttrKeyTypeECSECPrimeRandom,
      kSecAttrKeySizeInBits as String: 256,
    ]
    #if !targetEnvironment(simulator)
      attributes[kSecAttrTokenID as String] = kSecAttrTokenIDSecureEnclave
    #endif
    attributes[kSecPrivateKeyAttrs as String] = privateAttributes
    privateAttributes = [:]
    guard let key = SecKeyCreateRandomKey(attributes as CFDictionary, &error) else {
      throw error!.takeRetainedValue()
    }
    return key
  }

  static func publicKey(_ keyId: String) throws -> Data {
    let key = try lookup(keyId) ?? create(keyId)
    guard let publicKey = SecKeyCopyPublicKey(key) else { throw DeviceKeyError.noPublicKey }
    var error: Unmanaged<CFError>?
    guard let data = SecKeyCopyExternalRepresentation(publicKey, &error) as Data? else {
      throw error!.takeRetainedValue()
    }
    return data  // 04 || X || Y
  }

  static func sign(_ keyId: String, message: Data) throws -> Data {
    guard let key = lookup(keyId) else { throw DeviceKeyError.noKey }
    var error: Unmanaged<CFError>?
    guard let signature = SecKeyCreateSignature(
      key, .ecdsaSignatureMessageX962SHA256, message as CFData, &error) as Data?
    else { throw error!.takeRetainedValue() }
    return signature  // DER
  }

  static func delete(_ keyId: String) {
    let query: [String: Any] = [
      kSecClass as String: kSecClassKey,
      kSecAttrApplicationTag as String: tag(keyId),
      kSecAttrKeyType as String: kSecAttrKeyTypeECSECPrimeRandom,
    ]
    SecItemDelete(query as CFDictionary)
  }

  static func register(messenger: FlutterBinaryMessenger) {
    FlutterMethodChannel(name: "org.poiw.doora/device_key", binaryMessenger: messenger)
      .setMethodCallHandler { call, result in
        let args = call.arguments as? [String: Any]
        let keyId = args?["keyId"] as? String
        do {
          switch call.method {
          case "publicKey":
            guard let keyId else { throw DeviceKeyError.badArguments }
            result(FlutterStandardTypedData(bytes: try publicKey(keyId)))
          case "sign":
            guard let keyId, let message = args?["message"] as? FlutterStandardTypedData
            else { throw DeviceKeyError.badArguments }
            result(FlutterStandardTypedData(bytes: try sign(keyId, message: message.data)))
          case "delete":
            guard let keyId else { throw DeviceKeyError.badArguments }
            delete(keyId)
            result(nil)
          case "deviceName":
            result(UIDevice.current.name)
          default:
            result(FlutterMethodNotImplemented)
          }
        } catch {
          result(FlutterError(code: "device_key", message: "The key operation failed", details: nil))
        }
      }

    FlutterMethodChannel(name: "org.poiw.doora/system", binaryMessenger: messenger)
      .setMethodCallHandler { call, result in
        switch call.method {
        case "openSettings":
          if let url = URL(string: UIApplication.openSettingsURLString) {
            UIApplication.shared.open(url)
          }
          result(nil)
        case "startDoorActivity":
          let name = (call.arguments as? [String: Any])?["doorName"] as? String ?? "Door"
          DoorActivity.show(doorName: name)
          result(nil)
        case "endDoorActivity":
          DoorActivity.hide()
          result(nil)
        case "takeUnlockLaunch":
          result(DoorLaunch.takeUnlock())
        default:
          // Background scanning on iOS is Core Bluetooth state restoration, not a channel call.
          result(nil)
        }
      }
  }
}

enum DeviceKeyError: Error {
  case badArguments, noKey, noPublicKey
}

/// Set when the Live Activity's Unlock link (`doora://unlock`) opens the app; read once by Dart.
enum DoorLaunch {
  private static var unlockPending = false

  static func noteURL(_ url: URL) {
    if url.scheme == "doora" && url.host == "unlock" { unlockPending = true }
  }

  static func takeUnlock() -> Bool {
    defer { unlockPending = false }
    return unlockPending
  }
}

/// The "door at hand" Live Activity: shown while the door is near, gone when it is not. ActivityKit
/// only starts one from the foreground; from a background wake the notification is the fallback.
enum DoorActivity {
  static func show(doorName: String) {
    guard #available(iOS 16.2, *), ActivityAuthorizationInfo().areActivitiesEnabled else { return }
    let state = DoorActivityAttributes.ContentState(doorName: doorName)
    if let current = Activity<DoorActivityAttributes>.activities.first {
      Task { await current.update(ActivityContent(state: state, staleDate: nil)) }
      return
    }
    _ = try? Activity.request(
      attributes: DoorActivityAttributes(),
      content: ActivityContent(state: state, staleDate: nil),
      pushType: nil)
  }

  static func hide() {
    guard #available(iOS 16.2, *) else { return }
    for activity in Activity<DoorActivityAttributes>.activities {
      Task { await activity.end(nil, dismissalPolicy: .immediate) }
    }
  }
}
