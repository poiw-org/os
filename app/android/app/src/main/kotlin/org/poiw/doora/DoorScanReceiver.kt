package org.poiw.doora

import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.bluetooth.BluetoothManager
import android.bluetooth.le.BluetoothLeScanner
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanSettings
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.os.Build
import android.os.ParcelUuid
import androidx.core.app.NotificationCompat

/**
 * Background discovery on Android: a PendingIntent BLE scan for the door's service UUID. The system
 * wakes this receiver when the door is seen, even with the app killed, and the receiver posts
 * "You're at the door" with an Unlock action. The action opens the app, which unlocks at once: the
 * tap is the consent. Nothing is ever unlocked from here.
 */
class DoorScanReceiver : BroadcastReceiver() {
  override fun onReceive(context: Context, intent: Intent) {
    if (!context.getSharedPreferences(PREFS, Context.MODE_PRIVATE).getBoolean(KEY_ON, false)) return
    notifyAtDoor(context)
  }

  companion object {
    private const val PREFS = "door_scan"
    private const val KEY_ON = "on"
    private const val CHANNEL = "door"
    private const val NOTIFICATION_ID = 1
    private const val COOLDOWN_MS = 60_000L
    private var lastShown = 0L

    private fun pending(context: Context): PendingIntent =
      PendingIntent.getBroadcast(
        context, 0, Intent(context, DoorScanReceiver::class.java),
        PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_MUTABLE,
      )

    private fun scanner(context: Context): BluetoothLeScanner? =
      (context.getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager)
        .adapter?.bluetoothLeScanner

    fun start(context: Context, serviceUuid: String) {
      context.getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit().putBoolean(KEY_ON, true).apply()
      val filter = ScanFilter.Builder().setServiceUuid(ParcelUuid.fromString(serviceUuid)).build()
      val settings = ScanSettings.Builder()
        .setScanMode(ScanSettings.SCAN_MODE_LOW_POWER)
        .setCallbackType(ScanSettings.CALLBACK_TYPE_FIRST_MATCH)
        .build()
      try {
        scanner(context)?.startScan(listOf(filter), settings, pending(context))
      } catch (e: SecurityException) {
        // Bluetooth permission not granted: nothing to scan with.
      }
    }

    fun stop(context: Context) {
      context.getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit().putBoolean(KEY_ON, false).apply()
      try {
        scanner(context)?.stopScan(pending(context))
      } catch (e: SecurityException) {
      }
    }

    private fun notifyAtDoor(context: Context) {
      val now = System.currentTimeMillis()
      if (now - lastShown < COOLDOWN_MS) return
      lastShown = now
      val manager = context.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
      if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
        manager.createNotificationChannel(
          NotificationChannel(CHANNEL, "At the door", NotificationManager.IMPORTANCE_HIGH),
        )
      }
      val open = Intent(context, MainActivity::class.java).apply {
        action = MainActivity.ACTION_UNLOCK
        flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_SINGLE_TOP
      }
      val unlock = PendingIntent.getActivity(
        context, 1, open, PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE,
      )
      val notification = NotificationCompat.Builder(context, CHANNEL)
        .setSmallIcon(R.mipmap.ic_launcher)
        .setContentTitle("You're at the door")
        .setContentText("Tap Unlock to open it.")
        .setCategory(NotificationCompat.CATEGORY_STATUS)
        .setPriority(NotificationCompat.PRIORITY_HIGH)
        .setAutoCancel(true)
        .setContentIntent(unlock)
        .addAction(0, "Unlock", unlock)
        .setTimeoutAfter(30_000)
        .build()
      try {
        manager.notify(NOTIFICATION_ID, notification)
      } catch (e: SecurityException) {
        // Notifications not allowed.
      }
    }
  }
}
