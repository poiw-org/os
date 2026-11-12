#!/usr/bin/env python3
"""Generates docs/ble-test-vectors.json (docs/ble-protocol.md, section 7).

Everything is fixed or deterministic, so running this again reproduces the file byte for byte. The
door's host tests (doora/host_test) and the phone app's unit tests both read the JSON, so the two
implementations cannot drift. All keys below are throwaway test keys: they sign nothing real and the
RSA key here is not the backend's. Requires `cryptography` (pip install cryptography).

The JSON is flat with unique keys on purpose, so the door's tests can read it without a JSON library.
Binary values are lowercase hex; tokens are text.
"""
import base64, json, struct, sys
from pathlib import Path
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, padding, rsa, utils

DOOR_ID = "main"
OTHER_DOOR_ID = "other"
NONCE = bytes(range(0xA0, 0xB0))
OTHER_NONCE = bytes(range(0xC0, 0xD0))
NOW = 1_750_000_000
IAT, NBF, EXP = 1_749_999_000, 1_749_999_000, 1_850_000_000
SUB = "11111111-2222-3333-4444-555555555555"
JTI = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee"
TAG_SERIAL = "04a1b2c3d4e5f6"

# A throwaway 2048-bit RSA key (stands in for the backend's), generated once and fixed.
RSA_PEM = b"""-----BEGIN PRIVATE KEY-----
MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQDbxn4X9Fl4ru0V
AwR+HhjU1h2tJcZieDsoLXxS2dW/vKHgO+Z/of0ZtfD7pNbT+QNKz2QjPVh1e+mb
8gW03fh/fGCJbGPWjxTbBGTOmOSiY3mc5/qaS65ZaQin1Y+lEZimfHRavi/GC2sS
SgYjJqYH3yDmbtLjct2aev25BB24LBhaWUfat4AoKtlFaXd4Juf9E8k+my5aPIwY
udj2VN5Jv2sk7BRL5mlEUrcEw0BQqfcI4ZzSVYwFO/25oMqoROEwIRIoUOqXcTLs
riQ6jzSyY//MiSZCXtxuymN653Cyo9ByUTq6E+C88zK+XJ2JaEgMrl604u+7drFj
CdVsmkoxAgMBAAECggEAKrFdb1PVSZ9ZiNpkNgestTO+4hLTfe9zNnPXkdSlNDD1
lTStqx+Rpjv0yz2TSew2moK9JidVlsTydYvHVgJEsqc7rZZzsotq9rzcxFFpIK6I
QlXCFdUb0lyYBNp0pBFOARFuHO6TIzq2mv/sP0nTuPW8U2c16l828zpvLVDCYTM7
eAYYVJytXTmYe7GNryQOm88iwbydZ1ZeIkeKGqVQPu19NzQv1BDDg1hw+diUSXkt
2QjOoypJHFMTsEFt6obQ09UzKp03E/Bihh1eoDH9voDUQCvYOi8ebSZWiw+VE2Rz
fc1kQa/7VHcHYAEXpWwxKvCIU2W2Sh/UrYYn3v5VPQKBgQD04DkRGClWwYVEyZaX
i44mLAusLk0YGBRbY5hQqwd1ujiYlA+XAvtweH45R4upL6iAotE3MMLApur2Buju
o6VURAs6KOMQWwpDuQC6jzAdf1xQkQah9QRulZw0Pd2MoC8eu1wboyayu4zEwPBR
JgavSm27uvyEhDWEun6zqgACiwKBgQDlwl0s6ap+s+dw3auiuGut2gFTW+KQhLhr
Ft1G25JcwI5mh5VOFdnJo8uOe9BAKLfMieycU5g310jtFoiug34VPn0uSWFPoCuv
jHpsT/dkkQZ2fI7/AMYEUGhnjdTnDHPz2JnomB4+bM/cdv0vuS/ge5j80mgLOL19
EJFMFdjpswKBgEL0F8Zbw9nLueOP9tPivXAkdOCUHFItK0u9lITh7n06+ISHXyfc
EeGzoFeK/124ASuC2Q9tDqLcQLwaMZAmQGsk26kASu1K5yiTKebYhcBa9WO184lO
0/Jkh7wk+jGcDqYpdkq8NjWCYD/fvRWC/yrof85nlumouHVdBI2dOrl3AoGABtt6
NHD06bss2S2EheLDM/khuP8xD+cmKewQxqCfCa2lkmHRjvcP/PiqhqIjYQbWFYC4
UyIblO8T+h+GDM9X4AJxcgQ2p028gAzNSpZIZ/P8IWUuT2YSguZzd0PAavTwMQLx
9QM1Q9+zhHz4qYf5adXGhyUwfcU/wo/QBllQlAkCgYEAruKR/x0ILqmo7iRWM78X
ATplmYNQaO9GOZbqJdSIXdsWiRXPV/VTLDSqBb0P0KbiVx4JtAinUK9bU0evQR3g
bhDurTm3LCk5Ig1Tm4zI0Zcbk/Pwq2btuDiwxnlZiTcNq+ncCevrZUuX/CtSiepi
TbEPaG0nuYgDzqfo6vLqFtQ=
-----END PRIVATE KEY-----
"""

b64u = lambda b: base64.urlsafe_b64encode(b).rstrip(b"=").decode()
H = lambda b: b.hex()


def point(key):
    return key.public_key().public_bytes(serialization.Encoding.X962, serialization.PublicFormat.UncompressedPoint)


def jwt(rsa_key, claims):
    header = b64u(json.dumps({"alg": "RS256", "typ": "JWT"}, separators=(",", ":")).encode())
    payload = b64u(json.dumps(claims, separators=(",", ":")).encode())
    signed = f"{header}.{payload}"
    sig = rsa_key.sign(signed.encode(), padding.PKCS1v15(), hashes.SHA256())
    return f"{signed}.{b64u(sig)}"


def proof_message(door_id, nonce):
    d = door_id.encode()
    return b"doora-ble-1" + bytes([len(d)]) + d + nonce


def sign(ec_key, message):
    """Returns (DER, raw r||s). RFC 6979 deterministic ECDSA over SHA-256."""
    der = ec_key.sign(message, ec.ECDSA(hashes.SHA256(), deterministic_signing=True))
    r, s = utils.decode_dss_signature(der)
    return der, r.to_bytes(32, "big") + s.to_bytes(32, "big")


def chunks(body, mtu):
    """Each write is header | data; the ATT payload is mtu-3 bytes, so a chunk carries mtu-4 data bytes."""
    size = mtu - 4
    parts = [body[i:i + size] for i in range(0, len(body), size)]
    return [bytes([i | (0x80 if i == len(parts) - 1 else 0)]) + p for i, p in enumerate(parts)]


def cobs(data):
    out, code_at, code = bytearray([0]), 0, 1
    for b in data:
        if b == 0:
            out[code_at] = code
            code_at = len(out)
            out.append(0)
            code = 1
        else:
            out.append(b)
            code += 1
            if code == 0xFF:
                out[code_at] = code
                code_at = len(out)
                out.append(0)
                code = 1
    out[code_at] = code
    return bytes(out)


def crc16(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def frame(type_, conn, payload):
    raw = bytes([type_, conn]) + payload
    decoded = raw + struct.pack(">H", crc16(raw))
    return decoded, cobs(decoded) + b"\x00"


def main():
    out = {}
    # P-256 phone key: a fixed private scalar.
    phone = ec.derive_private_key(0x1F2E3D4C5B6A79880123456789ABCDEF0FEDCBA9876543210FEDCBA987654321 % (2**255), ec.SECP256R1())
    other = ec.derive_private_key(0x0A0B0C0D0E0F10111213141516171819202122232425262728292A2B2C2D2E2F, ec.SECP256R1())
    pub = point(phone)
    out["phone_private_scalar"] = H(phone.private_numbers().private_value.to_bytes(32, "big"))
    out["phone_public_key"] = H(pub)
    out["phone_public_key_b64u"] = b64u(pub)
    out["door_id"] = DOOR_ID
    out["nonce"] = H(NONCE)
    out["now"] = NOW

    msg = proof_message(DOOR_ID, NONCE)
    der, raw = sign(phone, msg)
    out["signed_message"] = H(msg)
    out["signed_message_sha256"] = H(__import__("hashlib").sha256(msg).digest())
    out["signature_der"] = H(der)
    out["signature_raw"] = H(raw)
    # Proofs that must fail for the door above.
    out["signature_raw_other_door"] = H(sign(phone, proof_message(OTHER_DOOR_ID, NONCE))[1])
    out["signature_raw_other_nonce"] = H(sign(phone, proof_message(DOOR_ID, OTHER_NONCE))[1])
    out["signature_raw_other_key"] = H(sign(other, msg)[1])
    out["nonce_other"] = H(OTHER_NONCE)
    out["door_id_other"] = OTHER_DOOR_ID

    rsa_key = serialization.load_pem_private_key(RSA_PEM, None)
    out["rsa_public_modulus_b64u"] = b64u(rsa_key.public_key().public_numbers().n.to_bytes(256, "big"))
    claims = {"sub": SUB, "jti": JTI, "aud": "ble", "cnf": b64u(pub), "iat": IAT, "nbf": NBF, "exp": EXP}
    credential = jwt(rsa_key, claims)
    out["credential_jwt"] = credential
    out["credential_jti"] = JTI
    out["credential_iat"], out["credential_nbf"], out["credential_exp"] = IAT, NBF, EXP
    out["tag_jwt"] = jwt(rsa_key, {"sub": SUB, "jti": JTI, "aud": TAG_SERIAL, "iat": IAT, "nbf": NBF, "exp": EXP})
    out["tag_serial"] = TAG_SERIAL
    # A credential whose cnf is a point that is not on the curve (Y flipped), correctly signed.
    bad = bytearray(pub); bad[64] ^= 0x01
    out["credential_jwt_off_curve"] = jwt(rsa_key, {**claims, "cnf": b64u(bytes(bad))})
    out["credential_jwt_wrong_cnf_length"] = jwt(rsa_key, {**claims, "cnf": b64u(pub[:64])})
    # The same credential with a flipped signature byte.
    t = credential.rsplit(".", 1)
    sig = bytearray(base64.urlsafe_b64decode(t[1] + "=="))
    sig[10] ^= 0x01
    out["credential_jwt_bad_rsa_signature"] = f"{t[0]}.{b64u(bytes(sig))}"
    out["credential_length"] = len(credential)
    assert len(credential) <= 860, len(credential)

    body = bytes([0x02, 0x01]) + raw + credential.encode()
    out["request_body"] = H(body)
    for mtu in (23, 185, 247):
        out[f"request_writes_mtu{mtu}"] = [H(c) for c in chunks(body, mtu)]

    challenge = bytes([0x01, 0x01]) + NONCE + bytes([len(DOOR_ID)]) + DOOR_ID.encode()
    out["challenge_message"] = H(challenge)
    out["result_opened_message"] = H(bytes([0x03, 0x00]))

    frames = {
        "ping": (0x11, 0, b""),
        "pong": (0x12, 0, b""),
        "hello": (0x10, 0, bytes([1]) + b"1.0.0"),
        "connected": (0x20, 7, b""),
        "challenge": (0x21, 7, challenge),
        "request": (0x22, 7, body),
        "result": (0x23, 7, bytes([0x03, 0x00])),
        "gone": (0x24, 7, b""),
        "zeros": (0x22, 1, bytes(40)),                       # payload of zeros: many COBS code bytes
        "run254": (0x22, 2, bytes(range(1, 255))[:253] + b"\x00" + bytes(range(1, 255))),  # long non-zero runs
        "max_payload": (0x22, 255, bytes((i * 7 + 1) % 251 + 1 for i in range(1100))),
    }
    for name, (t, c, p) in frames.items():
        decoded, encoded = frame(t, c, p)
        out[f"frame_{name}_decoded"] = H(decoded)
        out[f"frame_{name}_encoded"] = H(encoded)
    out["crc16_check_123456789"] = f"{crc16(b'123456789'):04x}"  # CRC-16/CCITT-FALSE check value 29b1

    path = Path(__file__).resolve().parent.parent / "ble-test-vectors.json"
    path.write_text(json.dumps(out, indent=1) + "\n")
    print(f"wrote {path}", file=sys.stderr)


if __name__ == "__main__":
    main()
