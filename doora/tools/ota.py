# PlatformIO extra script (see platformio.ini and README.md, "Updates over Bluetooth").
#
# Signs firmware.bin after every build when the signing key exists: appends the RSASSA-PKCS1-v1_5
# SHA-256 signature of the image and the "DOORASIG" marker (src/OtaImage.h). The boot loader
# ignores the extra bytes, so the signed file still flashes over USB.
#
# The private key stays outside the repository, in $DOORA_OTA_KEY (default ~/.doora/ota_key.pem).
# tools/ota_keygen.py creates it.
import os
import subprocess

Import("env")  # noqa: F821 (provided by PlatformIO)

MAGIC = b"DOORASIG"
SIG_BYTES = 256
KEY = os.environ.get("DOORA_OTA_KEY", os.path.expanduser("~/.doora/ota_key.pem"))


def signed(path):
    with open(path, "rb") as f:
        f.seek(0, os.SEEK_END)
        if f.tell() < len(MAGIC) + SIG_BYTES:
            return False
        f.seek(-len(MAGIC), os.SEEK_END)
        return f.read() == MAGIC


def sign(target, source, env):
    path = str(target[0])
    if signed(path):
        return
    if not os.path.exists(KEY):
        print("OTA: no signing key at %s; firmware.bin is unsigned (USB only). Run tools/ota_keygen.py." % KEY)
        return
    with open(path, "rb") as f:
        image = f.read()
    sig = subprocess.run(["openssl", "dgst", "-sha256", "-sign", KEY], input=image,
                         stdout=subprocess.PIPE, check=True).stdout
    if len(sig) != SIG_BYTES:
        raise SystemExit("OTA: the signing key must be RSA-2048")
    with open(path, "ab") as f:
        f.write(sig + MAGIC)
    print("OTA: signed %s" % path)


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", sign)
