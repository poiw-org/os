# PlatformIO pre-build script: fetches the PN532 NFC libraries (not in the PlatformIO registry).
# elechouse/PN532 ships three libraries in one repository (PN532, PN532_I2C, NDEF); they are copied
# into .pio/vendor/ (listed in lib_extra_dirs). Not lib/: new files there change the project hash
# mid-build, and PlatformIO then wipes the build directory while it is compiling.
Import("env")  # noqa: F821  (provided by PlatformIO)
import os
import shutil
import subprocess
import tempfile

REPO = "https://github.com/elechouse/PN532.git"
COMMIT = "8b05896a6616396392f0eac5af88a97c9733ebce"
LIBS = ["PN532", "PN532_I2C", "NDEF"]

lib_dir = os.path.join(env.subst("$PROJECT_DIR"), ".pio", "vendor")  # noqa: F821

if not all(os.path.isdir(os.path.join(lib_dir, name)) for name in LIBS):
    print("Fetching PN532 libraries (elechouse/PN532 @ %s)..." % COMMIT[:8])
    os.makedirs(lib_dir, exist_ok=True)
    with tempfile.TemporaryDirectory() as tmp:
        subprocess.check_call(["git", "clone", "--quiet", REPO, tmp])
        subprocess.check_call(["git", "-C", tmp, "checkout", "--quiet", COMMIT])
        for name in LIBS:
            target = os.path.join(lib_dir, name)
            if os.path.isdir(target):
                shutil.rmtree(target)
            shutil.copytree(os.path.join(tmp, name), target)

# Patches to the vendored library (applied every build, idempotent):
# - NfcAdapter::begin() spins in `while (1)` when the PN532 does not answer, which would freeze the
#   door (no console, no Wi-Fi). Return instead; Reader reports the failure and retries.
# - PN532_I2C::readResponse() ignores a timeout from getResponseLength() (-1 becomes length 255) and
#   then requests 263 bytes into a smaller Wire buffer, corrupting the heap. Return the error.
PATCHES = [
    ("NDEF/NfcAdapter.cpp", "while (1); // halt", "return;"),
    # - NfcAdapter keeps the polled UID private; Reader needs it without polling the tag twice (an
    #   already selected MIFARE Classic does not answer a second poll).
    ("NDEF/NfcAdapter.h",
     "    private:\n",
     "        const byte* uidBytes() const { return uid; }\n        uint8_t uidSize() const { return (uint8_t)uidLength; }\n    private:\n"),
    ("PN532_I2C/PN532_I2C.cpp",
     "    length = getResponseLength(buf, len, timeout);\n",
     "    int16_t responseLength = getResponseLength(buf, len, timeout);\n"
     "    if (responseLength < 0) {\n        return responseLength;\n    }\n"
     "    length = responseLength;\n"),
]
for rel, old, new in PATCHES:
    path = os.path.join(lib_dir, *rel.split("/"))
    with open(path) as f:
        src = f.read()
    if new not in src and old in src:
        with open(path, "w") as f:
            f.write(src.replace(old, new))
