package org.poiw.doora

import android.os.Build
import android.security.keystore.KeyGenParameterSpec
import android.security.keystore.KeyProperties
import android.security.keystore.StrongBoxUnavailableException
import io.flutter.plugin.common.MethodChannel
import java.security.KeyPairGenerator
import java.security.KeyStore
import java.security.PrivateKey
import java.security.Signature
import java.security.interfaces.ECPublicKey
import java.security.spec.ECGenParameterSpec

/**
 * The phone's key pairs: P-256 in the Android Keystore (StrongBox when there is one), one per
 * virtual key, no user-authentication requirement so an unlock works from a notification tap. The
 * private key never leaves the Keystore. Nothing here logs keys, signatures or tokens.
 */
object DeviceKey {
  private fun alias(keyId: String) = "org.poiw.doora.$keyId"

  private val store: KeyStore get() = KeyStore.getInstance("AndroidKeyStore").apply { load(null) }

  private fun create(keyId: String) {
    fun spec(strongBox: Boolean) =
      KeyGenParameterSpec.Builder(alias(keyId), KeyProperties.PURPOSE_SIGN)
        .setAlgorithmParameterSpec(ECGenParameterSpec("secp256r1"))
        .setDigests(KeyProperties.DIGEST_SHA256)
        .setUserAuthenticationRequired(false)
        .apply { if (strongBox && Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) setIsStrongBoxBacked(true) }
        .build()

    fun generate(strongBox: Boolean) {
      val generator = KeyPairGenerator.getInstance(KeyProperties.KEY_ALGORITHM_EC, "AndroidKeyStore")
      generator.initialize(spec(strongBox))
      generator.generateKeyPair()
    }
    try {
      generate(strongBox = true)
    } catch (e: StrongBoxUnavailableException) {
      generate(strongBox = false)
    } catch (e: java.security.ProviderException) {
      generate(strongBox = false)
    }
  }

  /** 65 bytes: 04 || X || Y, each coordinate left-padded to 32 bytes. */
  fun publicKey(keyId: String): ByteArray {
    if (!store.containsAlias(alias(keyId))) create(keyId)
    val key = store.getCertificate(alias(keyId)).publicKey as ECPublicKey
    fun coordinate(value: java.math.BigInteger): ByteArray {
      val raw = value.toByteArray().let { if (it.size > 32 && it[0] == 0.toByte()) it.copyOfRange(1, it.size) else it }
      return ByteArray(32 - raw.size) + raw
    }
    return byteArrayOf(0x04) + coordinate(key.w.affineX) + coordinate(key.w.affineY)
  }

  /** The DER ECDSA signature of [message] with SHA-256. */
  fun sign(keyId: String, message: ByteArray): ByteArray {
    val key = store.getKey(alias(keyId), null) as PrivateKey
    return Signature.getInstance("SHA256withECDSA").run {
      initSign(key)
      update(message)
      sign()
    }
  }

  fun delete(keyId: String) {
    if (store.containsAlias(alias(keyId))) store.deleteEntry(alias(keyId))
  }

  fun deviceName(context: android.content.Context): String =
    android.provider.Settings.Global.getString(context.contentResolver, "device_name")
      ?: Build.MODEL

  fun register(channel: MethodChannel, context: android.content.Context) {
    channel.setMethodCallHandler { call, result ->
      try {
        val keyId = call.argument<String>("keyId")
        when (call.method) {
          "publicKey" -> result.success(publicKey(keyId!!))
          "sign" -> result.success(sign(keyId!!, call.argument<ByteArray>("message")!!))
          "delete" -> { delete(keyId!!); result.success(null) }
          "deviceName" -> result.success(deviceName(context))
          else -> result.notImplemented()
        }
      } catch (e: Exception) {
        result.error("device_key", "The key operation failed", null)
      }
    }
  }
}
