#!/usr/bin/env python3
# Creates the firmware signing key in ~/.doora (once) and prints the config.h line that goes with it:
#   ota_key.pem    RSA-2048 private key that signs firmware images (keep it; never commit it)
# The door gets only the public modulus.
import os
import subprocess

home = os.path.expanduser("~/.doora")
key = os.environ.get("DOORA_OTA_KEY", os.path.join(home, "ota_key.pem"))
os.makedirs(home, mode=0o700, exist_ok=True)

if not os.path.exists(key):
    subprocess.run(["openssl", "genrsa", "-out", key, "2048"], check=True, stderr=subprocess.DEVNULL)
    os.chmod(key, 0o600)
    print("Created %s" % key)

modulus = subprocess.run(["openssl", "rsa", "-in", key, "-noout", "-modulus"],
                         stdout=subprocess.PIPE, check=True, text=True).stdout.strip().split("=", 1)[1]
n = bytes.fromhex(modulus)
assert len(n) == 256, "the signing key must be RSA-2048"

print("\n// Add to config.h:")
print("#define OTA_SIGNING_KEY { \\")
for i in range(0, 256, 16):
    print("\t" + ", ".join("0x%02x" % b for b in n[i:i + 16]) + (", \\" if i < 240 else " \\"))
print("}")
