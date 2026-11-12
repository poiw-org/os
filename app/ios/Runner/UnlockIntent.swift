import AppIntents

/// "Unlock Door" for Shortcuts, Siri, the Action button and automations (e.g. an NFC automation).
/// It opens the app and asks it to unlock, exactly like the Live Activity's Unlock link: the app
/// finds the closest door over Bluetooth and unlocks it, asking for biometrics as usual.
@available(iOS 16.0, *)
struct UnlockDoorIntent: AppIntent {
  static var title: LocalizedStringResource = "Unlock Door"
  static var description = IntentDescription("Unlocks the closest door you have a key for.")
  static var openAppWhenRun = true

  @MainActor
  func perform() async throws -> some IntentResult {
    DoorLaunch.requestUnlock()
    return .result()
  }
}

@available(iOS 16.0, *)
struct DooraShortcuts: AppShortcutsProvider {
  static var appShortcuts: [AppShortcut] {
    AppShortcut(
      intent: UnlockDoorIntent(),
      phrases: ["Unlock the door with \(.applicationName)", "Open the door with \(.applicationName)"],
      shortTitle: "Unlock Door",
      systemImageName: "lock.open")
  }
}
