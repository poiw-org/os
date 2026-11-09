#!/usr/bin/env python3
"""Publishes the signed firmware.bin for approval in the portal.

Needs DOORA_SUPABASE_URL (https://<project>.supabase.co) and DOORA_SUPABASE_KEY (the publishable key).

    python3 tools/release.py [firmware.bin]
"""
import hashlib
import json
import os
import ssl
import subprocess
import sys
import urllib.error
import urllib.parse
import urllib.request
import webbrowser

import certifi

PORTAL = os.environ.get("DOORA_PORTAL_URL", "https://poiw.org").rstrip("/")


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else ".pio/build/esp32dev/firmware.bin"
    url, key = os.environ.get("DOORA_SUPABASE_URL", "https://yshwchmbsuafribrttlt.supabase.co"), os.environ.get("DOORA_SUPABASE_KEY", "sb_publishable_Yqe-dl6usU3GEGPUbAIPTA_olGKWuMG")
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
        # PlatformIO's Python has no system certificates, but it does ship certifi.
        context = ssl.create_default_context(cafile=certifi.where())
        with urllib.request.urlopen(request, timeout=120, context=context) as response:
            result = json.load(response)
    except urllib.error.HTTPError as e:
        sys.exit("Upload refused (HTTP %d): %s" % (e.code, e.read().decode(errors="replace")))
    sha256 = hashlib.sha256(image).hexdigest()
    if result["sha256"] != sha256:
        sys.exit("The server stored a different image (%s). Do not approve it." % result["sha256"])
    # The address is fixed here, never taken from the response, and the hash is the one computed locally.
    link = "%s/os/firmware/%s?sha256=%s" % (PORTAL, urllib.parse.quote(result["id"], safe=""), sha256)
    print("Uploaded %s (%d bytes)." % (version, result["size"]))
    print("sha256: %s" % sha256)
    print("Approve it, checking that the portal shows this hash:\n%s" % link)
    webbrowser.open(link)


if __name__ == "__main__":
    main()
