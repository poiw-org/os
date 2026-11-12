import Flutter
import UIKit

class SceneDelegate: FlutterSceneDelegate {
  // The Live Activity's Unlock link: `doora://unlock`, cold or warm.
  override func scene(
    _ scene: UIScene, willConnectTo session: UISceneSession,
    options connectionOptions: UIScene.ConnectionOptions
  ) {
    connectionOptions.urlContexts.forEach { DoorLaunch.noteURL($0.url) }
    super.scene(scene, willConnectTo: session, options: connectionOptions)
  }

  override func scene(_ scene: UIScene, openURLContexts URLContexts: Set<UIOpenURLContext>) {
    URLContexts.forEach { DoorLaunch.noteURL($0.url) }
    super.scene(scene, openURLContexts: URLContexts)
  }
}
