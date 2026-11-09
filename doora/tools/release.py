#!/usr/bin/env python3
"""Publishes firmware.bin to the door and follows the update until the door reports how it went.

An admin's build is approved by the upload itself; anyone else's waits for an admin in the portal.

The first run opens the portal to ask for permission to upload on your behalf. The answer is kept in
~/.doora/release_token for 30 days.

    python3 tools/release.py [firmware.bin]
"""
import base64
import hashlib
import http.server
import json
import os
import secrets
import ssl
import subprocess
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
import webbrowser

import certifi

PORTAL = os.environ.get("DOORA_PORTAL_URL", "https://poiw.org").rstrip("/")
SUPABASE = os.environ.get("DOORA_SUPABASE_URL", "https://yshwchmbsuafribrttlt.supabase.co").rstrip("/")
KEY = os.environ.get("DOORA_SUPABASE_KEY", "sb_publishable_Yqe-dl6usU3GEGPUbAIPTA_olGKWuMG")
TOKEN_FILE = os.path.expanduser("~/.doora/release_token")
CONSENT_WAIT_S = 300
POLL_PENDING_S = 5
POLL_DEPLOYING_S = 2
# As long as the server accepts an approval, and a little past how long a door may fetch an approved build.
PENDING_WAIT_S = 24 * 3600
DEPLOY_WAIT_S = 40 * 60
PROGRESS = {
    "pending": "Waiting for an admin to approve it.",
    "approved": "Approved. Waiting for the door to fetch it.",
    "downloading": "The door is downloading it.",
    "rebooting": "The door verified it and will restart when idle.",
}
# PlatformIO's Python has no system certificates, but it does ship certifi.
TLS = ssl.create_default_context(cafile=certifi.where())


def post(path, data, headers):
    request = urllib.request.Request(SUPABASE + path, data=data, method="POST", headers={"apikey": KEY, **headers})
    with urllib.request.urlopen(request, timeout=120, context=TLS) as response:
        return json.load(response)


def wait_for_consent(state):
    """Serves one callback on a loopback port; returns the port's server, already listening."""
    answer = {}

    class Callback(http.server.BaseHTTPRequestHandler):
        def do_GET(self):
            url = urllib.parse.urlsplit(self.path)
            query = dict(urllib.parse.parse_qsl(url.query))
            good = url.path == "/callback" and secrets.compare_digest(query.get("state", ""), state)
            if good:
                answer.update(query)
            self.send_response(200 if good else 400)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.end_headers()
            self.wfile.write(b"You can close this tab and return to the terminal.\n" if good else b"Bad request.\n")

        def log_message(self, *args):
            pass

    server = http.server.HTTPServer(("127.0.0.1", 0), Callback)
    server.timeout = 1
    return server, answer


def sign_in():
    verifier = secrets.token_urlsafe(48)
    challenge = base64.urlsafe_b64encode(hashlib.sha256(verifier.encode()).digest()).rstrip(b"=").decode()
    state = secrets.token_urlsafe(24)
    server, answer = wait_for_consent(state)
    query = urllib.parse.urlencode({"port": server.server_address[1], "state": state, "challenge": challenge})
    link = "%s/os/firmware/authorize?%s" % (PORTAL, query)
    print("Allow uploads in your browser. If it did not open:\n%s" % link)
    webbrowser.open(link)
    deadline = time.monotonic() + CONSENT_WAIT_S
    while not answer and time.monotonic() < deadline:
        server.handle_request()
    server.server_close()
    if "code" not in answer:
        sys.exit("Permission was not granted.")
    result = post("/functions/v1/firmware-cli-auth", json.dumps({"action": "token", "code": answer["code"], "verifier": verifier}).encode(),
                  {"Content-Type": "application/json"})
    os.makedirs(os.path.dirname(TOKEN_FILE), mode=0o700, exist_ok=True)
    with open(os.open(TOKEN_FILE, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600), "w") as f:
        f.write(result["token"])
    return result["token"]


def saved_token():
    if not os.path.exists(TOKEN_FILE):
        return None
    with open(TOKEN_FILE) as f:
        return f.read().strip() or None


def upload(image, version, token):
    return post("/functions/v1/firmware-upload", image, {
        "Authorization": "Bearer %s" % token,
        "Content-Type": "application/octet-stream",
        "x-firmware-version": version,
        "x-firmware-sha256": hashlib.sha256(image).hexdigest(),
    })


def follow(release, token):
    """Prints each change of state until the update ends. Exits non-zero if it did not install."""
    body = json.dumps({"id": release}).encode()
    headers = {"Authorization": "Bearer %s" % token, "Content-Type": "application/json"}
    shown = None
    deadline = time.monotonic() + PENDING_WAIT_S
    while True:
        try:
            row = post("/functions/v1/firmware-status", body, headers)
        except urllib.error.HTTPError as e:
            if e.code < 500:
                sys.exit("Could not read the update's state (HTTP %d): %s" % (e.code, e.read().decode(errors="replace")))
            row = None
        except OSError:  # the network dropping out does not stop the door
            row = None
        state = row and (row["door_state"] or row["status"])
        if state and state != shown:
            if shown in (None, "pending") and state != "pending":
                deadline = time.monotonic() + DEPLOY_WAIT_S
            shown = state
            if state == "installed":
                print("Installed. The door is running this build.")
                return
            if state == "rejected":
                sys.exit("Rejected; the door will not install it.")
            if state in ("failed", "rolled_back"):
                sys.exit("The door did not install it (%s): %s" % (state, row["door_error"] or "no details"))
            print(PROGRESS[state], flush=True)
        if time.monotonic() > deadline:
            sys.exit("Gave up waiting while the update was: %s. Check the portal." % (shown or "unknown"))
        time.sleep(POLL_PENDING_S if shown in (None, "pending") else POLL_DEPLOYING_S)


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else ".pio/build/esp32dev/firmware.bin"
    with open(path, "rb") as f:
        image = f.read()
    version = subprocess.run(["git", "describe", "--always", "--dirty"], stdout=subprocess.PIPE,
                             check=True, text=True).stdout.strip()

    token = saved_token() or sign_in()
    try:
        try:
            result = upload(image, version, token)
        except urllib.error.HTTPError as e:
            if e.code != 401:
                raise
            token = sign_in()  # expired: ask again
            result = upload(image, version, token)
    except urllib.error.HTTPError as e:
        sys.exit("Upload refused (HTTP %d): %s" % (e.code, e.read().decode(errors="replace")))

    sha256 = hashlib.sha256(image).hexdigest()
    if result["sha256"] != sha256:
        sys.exit("The server stored a different image (%s). Do not approve it; reject it if it is approved." % result["sha256"])
    # The address is fixed here, never taken from the response, and the hash is the one computed locally.
    link = "%s/os/firmware/%s?sha256=%s" % (PORTAL, urllib.parse.quote(result["id"], safe=""), sha256)
    print("Uploaded %s (%d bytes)." % (version, result["size"]))
    print("sha256: %s" % sha256)
    if result.get("approved"):
        print("Approved automatically because you are an admin.")
    else:
        print("Approve it, checking that the portal shows this hash:\n%s" % link)
        webbrowser.open(link)
    try:
        follow(result["id"], token)
    except KeyboardInterrupt:
        print("\nStopped watching. The update carries on; the portal shows how it goes.")


if __name__ == "__main__":
    main()
