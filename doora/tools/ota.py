# PlatformIO extra script (see platformio.ini and README.md, "Updates over Wi-Fi").
#
# - Signs firmware.bin after every build when the signing key exists: appends the RSASSA-PKCS1-v1_5
#   SHA-256 signature of the image and the "DOORASIG" marker (src/OtaImage.h). The boot loader
#   ignores the extra bytes, so the signed file still flashes over USB.
# - For espota uploads, refuses an unsigned image and passes the OTA password to espota.
#
# Secrets stay outside the repository: the private key in $DOORA_OTA_KEY (default
# ~/.doora/ota_key.pem) and the password in $DOORA_OTA_PASSWORD (default ~/.doora/ota_password).
# tools/ota_keygen.py creates both.
import os
import subprocess

Import("env")  # noqa: F821 (provided by PlatformIO)

MAGIC = b"DOORASIG"
SIG_BYTES = 256
HOME = os.path.expanduser("~/.doora")
KEY = os.environ.get("DOORA_OTA_KEY", os.path.join(HOME, "ota_key.pem"))
PASSWORD_FILE = os.path.join(HOME, "ota_password")


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


def password():
    if os.environ.get("DOORA_OTA_PASSWORD"):
        return os.environ["DOORA_OTA_PASSWORD"]
    if os.path.exists(PASSWORD_FILE):
        with open(PASSWORD_FILE) as f:
            return f.read().strip()
    raise SystemExit("OTA: set DOORA_OTA_PASSWORD or create %s (tools/ota_keygen.py)" % PASSWORD_FILE)


def check_upload(source, target, env):
    if not signed(str(source[0])):
        raise SystemExit("OTA: refusing to upload an unsigned image; the door would discard it. "
                         "Create the key with tools/ota_keygen.py and rebuild.")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", sign)

if env.GetProjectOption("upload_protocol", "") == "espota":
    env.Append(UPLOADERFLAGS=["--auth=" + password()])
    env.AddPreAction("upload", check_upload)
