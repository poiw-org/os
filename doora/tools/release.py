#!/usr/bin/env python3
"""Publishes the signed firmware.bin for approval in the portal.

Needs DOORA_SUPABASE_URL (https://<project>.supabase.co) and DOORA_SUPABASE_KEY (the publishable key).

    python3 tools/release.py [firmware.bin]
"""
import json
import os
import subprocess
import sys
import urllib.error
import urllib.request


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else ".pio/build/esp32dev/firmware.bin"
    url, key = os.environ.get("DOORA_SUPABASE_URL"), os.environ.get("DOORA_SUPABASE_KEY")
    if not url or not key:
        sys.exit(__doc__)
    with open(path, "rb") as f:
        image = f.read()
    if not image.endswith(b"DOORASIG"):
        sys.exit("%s is not signed. Run tools/ota_keygen.py and rebuild." % path)
    version = subprocess.run(["git", "describe", "--always", "--dirty"], stdout=subprocess.PIPE,
                             check=True, text=True).stdout.strip()

    request = urllib.request.Request(url.rstrip("/") + "/functions/v1/firmware-upload", data=image, method="POST", headers={
        "apikey": key,
        "Content-Type": "application/octet-stream",
        "x-firmware-version": version,
    })
    try:
        with urllib.request.urlopen(request, timeout=120) as response:
            result = json.load(response)
    except urllib.error.HTTPError as e:
        sys.exit("Upload refused (HTTP %d): %s" % (e.code, e.read().decode(errors="replace")))
    print("Uploaded %s (%d bytes, sha256 %s)." % (version, result["size"], result["sha256"]))
    print("Approve it in the portal, under Firmware.")


if __name__ == "__main__":
    main()
