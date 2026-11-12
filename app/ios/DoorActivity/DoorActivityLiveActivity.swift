import ActivityKit
import SwiftUI
import WidgetKit

// The web app's look: square corners, bevelled plates, Space Mono caps, one purple that means "act".

private let mono = "SpaceMono-Regular"
private let monoBold = "SpaceMono-Bold"

/// A door in line art: a frame, a leaf, a handle and a lit lamp. Square, no gradients.
struct DoorGraphic: View {
  var size: CGFloat = 44

  var body: some View {
    let line = max(1.5, size / 22)
    ZStack {
      Rectangle().stroke(Tokens.gray400, lineWidth: line)
      Rectangle()
        .fill(Tokens.paper2)
        .overlay(Rectangle().stroke(Tokens.gray300, lineWidth: line))
        .padding(size * 0.14)
      Rectangle()
        .fill(Tokens.ink)
        .frame(width: size * 0.07, height: size * 0.07)
        .offset(x: size * 0.15, y: size * 0.02)
      Rectangle()
        .fill(Tokens.lampGreen)
        .frame(width: size * 0.12, height: size * 0.12)
        .shadow(color: Tokens.lampGreen.opacity(0.65), radius: size * 0.08)
        .offset(x: size * 0.15, y: -size * 0.28)
    }
    .frame(width: size, height: size * 1.25)
    .accessibilityHidden(true)
  }
}

/// `.btn.btn-primary`: brand fill, a light edge top-left and a dark one bottom-right. Opens the app
/// on `doora://unlock`, which unlocks at once: the tap is the consent.
struct UnlockButton: View {
  var body: some View {
    Link(destination: URL(string: "doora://unlock")!) {
      Text("UNLOCK")
        .font(.custom(mono, size: 12))
        .tracking(1.6)
        .foregroundStyle(.white)
        .frame(maxWidth: .infinity, minHeight: 44)
        .background(Tokens.brand)
        .overlay(Rectangle().stroke(Tokens.brand, lineWidth: 1))
        .overlay(alignment: .top) { Rectangle().fill(.white.opacity(0.14)).frame(height: 1) }
        .overlay(alignment: .bottom) { Rectangle().fill(.black.opacity(0.25)).frame(height: 1) }
    }
    .accessibilityLabel("Unlock the door")
  }
}

struct DoorLockScreenView: View {
  let state: DoorActivityAttributes.ContentState

  var body: some View {
    HStack(spacing: 16) {
      DoorGraphic()
      VStack(alignment: .leading, spacing: 6) {
        Text("DOOR NEAR")
          .font(.custom(mono, size: 11))
          .tracking(1.5)
          .foregroundStyle(Tokens.gray400)
        Text(state.doorName)
          .font(.custom(monoBold, size: 17))
          .foregroundStyle(Tokens.ink)
          .lineLimit(1)
        UnlockButton()
      }
    }
    .padding(16)
    .background(Tokens.paper)
    .overlay(Rectangle().stroke(Tokens.gray200, lineWidth: 1))
    .accessibilityElement(children: .contain)
  }
}

struct DoorLiveActivity: Widget {
  var body: some WidgetConfiguration {
    ActivityConfiguration(for: DoorActivityAttributes.self) { context in
      DoorLockScreenView(state: context.state)
        .activityBackgroundTint(Tokens.paper)
        .activitySystemActionForegroundColor(Tokens.ink)
    } dynamicIsland: { context in
      DynamicIsland {
        DynamicIslandExpandedRegion(.leading) { DoorGraphic(size: 32) }
        DynamicIslandExpandedRegion(.center) {
          VStack(spacing: 2) {
            Text("DOOR NEAR").font(.custom(mono, size: 10)).tracking(1.4).foregroundStyle(Tokens.gray400)
            Text(context.state.doorName).font(.custom(monoBold, size: 15)).foregroundStyle(Tokens.ink)
          }
        }
        DynamicIslandExpandedRegion(.bottom) { UnlockButton() }
      } compactLeading: {
        DoorGraphic(size: 14)
      } compactTrailing: {
        Rectangle().fill(Tokens.lampGreen).frame(width: 8, height: 8)
      } minimal: {
        Rectangle().fill(Tokens.lampGreen).frame(width: 8, height: 8)
      }
      .keylineTint(Tokens.brand)
    }
  }
}

@main
struct DoorActivityBundle: WidgetBundle {
  var body: some Widget { DoorLiveActivity() }
}
