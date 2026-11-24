"""Archive system --- save and load metro networks to/from files.

Supported formats
-----------------
- JSON (plain)            — human-readable, ``.json``
- Compressed (zlib)       — JSON + deflate, ``.json.z``
- Binary (pickle+zlib)    — compact, ``.bin``
- Encrypted (HMAC-SHA256) — password-protected, ``.enc``
"""

import hashlib
import hmac as _hmac
import json
import os
import pickle
import struct
import zlib
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from main import MetroNetwork


# Current archive format version.
ARCHIVE_VERSION = 1

# 4-byte magic + 2-byte version = 6-byte header for binary format.
_BIN_MAGIC = b"UCBN"
_BIN_VERSION: int = 1

# Encryption parameters.
_SALT_BYTES: int = 16
_ITERATIONS: int = 600_000
_KEY_BYTES: int = 32
_NONCE_BYTES: int = 16
_TAG_BYTES: int = 32


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _build_archive(network: MetroNetwork,
                   meta: dict[str, Any] | None = None) -> dict:
    """Wrap *network.to_dict()* inside the standard archive envelope."""
    return {
        "archive_version": ARCHIVE_VERSION,
        "exported_at": datetime.now(timezone.utc).isoformat(),
        "meta": meta or {},
        "network": network.to_dict(),
    }


def _unwrap_archive(data: dict) -> MetroNetwork:
    """Extract and rebuild a MetroNetwork from an archive dict."""
    version = data.get("archive_version", 1)
    if version != ARCHIVE_VERSION:
        raise ValueError(
            f"Unsupported archive version {version}"
            f" (expected {ARCHIVE_VERSION})"
        )
    return MetroNetwork.from_dict(data["network"])


# ---------------------------------------------------------------------------
# JSON format
# ---------------------------------------------------------------------------

def export_json(network: MetroNetwork, filepath: str | Path,
                meta: dict[str, Any] | None = None) -> Path:
    """Save a network as a human-readable JSON archive."""
    filepath = Path(filepath)
    if filepath.suffix.lower() != ".json":
        filepath = filepath.with_suffix(".json")
    archive = _build_archive(network, meta)
    filepath.write_text(json.dumps(archive, ensure_ascii=False, indent=2),
                        encoding="utf-8")
    return filepath


def import_json(filepath: str | Path) -> MetroNetwork:
    """Load a network from a JSON archive."""
    filepath = Path(filepath)
    data = json.loads(filepath.read_text(encoding="utf-8"))
    return _unwrap_archive(data)


# ---------------------------------------------------------------------------
# Compressed format (JSON + zlib deflate)
# ---------------------------------------------------------------------------

def export_compressed(network: MetroNetwork, filepath: str | Path,
                      meta: dict[str, Any] | None = None,
                      level: int = 6) -> Path:
    """Save a network as a zlib-compressed JSON archive (``.json.z``)."""
    filepath = Path(filepath)
    if filepath.suffix.lower() != ".z":
        filepath = filepath.with_suffix(filepath.suffix + ".z")
    archive = _build_archive(network, meta)
    raw = json.dumps(archive, ensure_ascii=False).encode("utf-8")
    compressed = zlib.compress(raw, level)
    filepath.write_bytes(compressed)
    return filepath


def import_compressed(filepath: str | Path) -> MetroNetwork:
    """Load a network from a compressed archive (``.json.z``)."""
    filepath = Path(filepath)
    compressed = filepath.read_bytes()
    raw = zlib.decompress(compressed)
    data = json.loads(raw.decode("utf-8"))
    return _unwrap_archive(data)


# ---------------------------------------------------------------------------
# Binary format (pickle + zlib, with magic header)
# ---------------------------------------------------------------------------

def export_binary(network: MetroNetwork, filepath: str | Path,
                  meta: dict[str, Any] | None = None) -> Path:
    """Save a network as a compact binary archive (``.bin``)."""
    filepath = Path(filepath)
    if filepath.suffix.lower() != ".bin":
        filepath = filepath.with_suffix(".bin")
    archive = _build_archive(network, meta)
    pickled = pickle.dumps(archive, protocol=5)
    compressed = zlib.compress(pickled)
    header = struct.pack("<4sH", _BIN_MAGIC, _BIN_VERSION)
    filepath.write_bytes(header + compressed)
    return filepath


def import_binary(filepath: str | Path) -> MetroNetwork:
    """Load a network from a binary archive (``.bin``)."""
    filepath = Path(filepath)
    raw = filepath.read_bytes()
    if len(raw) < 6:
        raise ValueError("File too short to be a valid binary archive.")
    magic, version = struct.unpack("<4sH", raw[:6])
    if magic != _BIN_MAGIC:
        raise ValueError(
            f"Bad magic bytes: expected {_BIN_MAGIC!r}, got {magic!r}"
        )
    if version != _BIN_VERSION:
        raise ValueError(
            f"Unsupported binary version {version} (expected {_BIN_VERSION})"
        )
    pickled = zlib.decompress(raw[6:])
    data = pickle.loads(pickled)
    return _unwrap_archive(data)


# ---------------------------------------------------------------------------
# Encrypted format (HMAC-SHA256 stream cipher, stdlib only, zero deps)
# ---------------------------------------------------------------------------

def _derive_key(password: str, salt: bytes) -> bytes:
    """Derive a 256-bit key via PBKDF2-HMAC-SHA256."""
    return hashlib.pbkdf2_hmac(
        "sha256", password.encode("utf-8"), salt, _ITERATIONS,
        dklen=_KEY_BYTES,
    )


def _stream_xor(key: bytes, nonce: bytes, data: bytes) -> bytes:
    """XOR *data* with a keystream from HMAC-SHA256(key, nonce || ctr)."""
    result = bytearray(len(data))
    counter = 0
    offset = 0
    while offset < len(data):
        ctr_bytes = counter.to_bytes(8, "big")
        keystream = _hmac.digest(key, nonce + ctr_bytes, "sha256")
        chunk = min(len(keystream), len(data) - offset)
        for i in range(chunk):
            result[offset + i] = data[offset + i] ^ keystream[i]
        offset += chunk
        counter += 1
    return bytes(result)


def export_encrypted(network: MetroNetwork, filepath: str | Path,
                     password: str,
                     meta: dict[str, Any] | None = None) -> Path:
    """Save a network as a password-protected encrypted archive (``.enc``).

    File layout: salt (16) | nonce (16) | ciphertext | tag (32)

    The tag authenticates the entire payload; any tampering or wrong
    password is detected on import.
    """
    filepath = Path(filepath)
    if filepath.suffix.lower() != ".enc":
        filepath = filepath.with_suffix(".enc")

    archive = _build_archive(network, meta)
    plaintext = json.dumps(archive, ensure_ascii=False).encode("utf-8")

    salt = os.urandom(_SALT_BYTES)
    key = _derive_key(password, salt)
    nonce = os.urandom(_NONCE_BYTES)

    ciphertext = _stream_xor(key, nonce, plaintext)
    tag = _hmac.digest(key, salt + nonce + ciphertext, "sha256")

    filepath.write_bytes(salt + nonce + ciphertext + tag)
    return filepath


def import_encrypted(filepath: str | Path, password: str) -> MetroNetwork:
    """Load a network from an encrypted archive (``.enc``).

    Raises:
        ValueError: wrong password or corrupted / tampered file.
    """
    filepath = Path(filepath)
    raw = filepath.read_bytes()
    min_len = _SALT_BYTES + _NONCE_BYTES + 1 + _TAG_BYTES
    if len(raw) < min_len:
        raise ValueError("File too short to be a valid encrypted archive.")

    tag = raw[-_TAG_BYTES:]
    salt = raw[:_SALT_BYTES]
    nonce = raw[_SALT_BYTES:_SALT_BYTES + _NONCE_BYTES]
    ciphertext = raw[_SALT_BYTES + _NONCE_BYTES:-_TAG_BYTES]

    key = _derive_key(password, salt)

    expected_tag = _hmac.digest(key, salt + nonce + ciphertext, "sha256")
    if not _hmac.compare_digest(tag, expected_tag):
        raise ValueError(
            "Wrong password or corrupted file (authentication failed)."
        )

    plaintext = _stream_xor(key, nonce, ciphertext)
    data = json.loads(plaintext.decode("utf-8"))
    return _unwrap_archive(data)


# ---------------------------------------------------------------------------
# Convenience — save / load registered maps
# ---------------------------------------------------------------------------

def save_registered_map(network: MetroNetwork,
                        map_key: str,
                        display_name: str,
                        directory: str | Path = ".") -> Path:
    """Save *network* as a JSON file named after *map_key* inside
    *directory*, returning the written path."""
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=True)
    return export_json(
        network,
        directory / f"{map_key}.json",
        meta={"map_key": map_key, "display_name": display_name},
    )
