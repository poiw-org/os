package org.poiw.doora

import android.content.Intent
import android.net.Uri
import android.provider.Settings
import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.MethodChannel

class MainActivity : FlutterActivity() {
  companion object {
    const val ACTION_UNLOCK = "org.poiw.doora.UNLOCK"
  }

  /** Set when the notification's Unlock action opened the app; Dart reads it once. */
  private var unlockPending = false

  override fun onCreate(savedInstanceState: android.os.Bundle?) {
    super.onCreate(savedInstanceState)
    noteIntent(intent)
  }

  override fun onNewIntent(intent: Intent) {
    super.onNewIntent(intent)
    noteIntent(intent)
  }

  private fun noteIntent(intent: Intent?) {
    if (intent?.action == ACTION_UNLOCK) unlockPending = true
  }

  override fun configureFlutterEngine(flutterEngine: FlutterEngine) {
    super.configureFlutterEngine(flutterEngine)
    val messenger = flutterEngine.dartExecutor.binaryMessenger
    DeviceKey.register(MethodChannel(messenger, "org.poiw.doora/device_key"), applicationContext)

    MethodChannel(messenger, "org.poiw.doora/system").setMethodCallHandler { call, result ->
      when (call.method) {
        "openSettings" -> {
          startActivity(
            Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS, Uri.parse("package:$packageName"))
              .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK),
          )
          result.success(null)
        }
        "startBackgroundScan" -> {
          DoorScanReceiver.start(applicationContext, call.argument<String>("serviceUuid")!!)
          result.success(null)
        }
        "stopBackgroundScan" -> {
          DoorScanReceiver.stop(applicationContext)
          result.success(null)
        }
        "takeUnlockLaunch" -> {
          result.success(unlockPending)
          unlockPending = false
        }
        else -> result.success(null)
      }
    }
  }
}
