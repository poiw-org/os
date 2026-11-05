#!/usr/bin/env python3
# Creates the OTA secrets in ~/.doora (once) and prints the two config.h lines that go with them:
#   ota_key.pem    RSA-2048 private key that signs firmware images (keep it; never commit it)
#   ota_password   the espota password
# The door gets only the public modulus and the MD5 of the password.
import hashlib
import os
import secrets
import subprocess

home = os.path.expanduser("~/.doora")
key = os.environ.get("DOORA_OTA_KEY", os.path.join(home, "ota_key.pem"))
password_file = os.path.join(home, "ota_password")
os.makedirs(home, mode=0o700, exist_ok=True)

if not os.path.exists(key):
    subprocess.run(["openssl", "genrsa", "-out", key, "2048"], check=True, stderr=subprocess.DEVNULL)
    os.chmod(key, 0o600)
    print("Created %s" % key)
if not os.path.exists(password_file):
    with open(os.open(password_file, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600), "w") as f:
        f.write(secrets.token_urlsafe(24) + "\n")
    print("Created %s" % password_file)

with open(password_file) as f:
    password = f.read().strip()
modulus = subprocess.run(["openssl", "rsa", "-in", key, "-noout", "-modulus"],
                         stdout=subprocess.PIPE, check=True, text=True).stdout.strip().split("=", 1)[1]
n = bytes.fromhex(modulus)
assert len(n) == 256, "the signing key must be RSA-2048"

print("\n// Add to config.h:")
print('#define OTA_PASSWORD_MD5 "%s"' % hashlib.md5(password.encode()).hexdigest())
print("#define OTA_SIGNING_KEY { \\")
for i in range(0, 256, 16):
    print("\t" + ", ".join("0x%02x" % b for b in n[i:i + 16]) + (", \\" if i < 240 else " \\"))
print("}")
