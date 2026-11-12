import ActivityKit

/// The "door at hand" Live Activity. Shared by the app (which starts and ends it) and the widget
/// extension (which draws it).
struct DoorActivityAttributes: ActivityAttributes {
  struct ContentState: Codable, Hashable {
    var doorName: String
  }
}
