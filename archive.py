"""Archive system — save and load metro networks to/from files.

Supported formats
-----------------
- JSON (plain)            — human-readable, ``.json``
- Compressed (zlib)       — JSON + deflate, ``.json.z``
- Binary (pickle+zlib)    — compact, ``.bin``
- Encrypted (AES-256-GCM) — password-protected, ``.enc``   (task 5.3)
"""

import json
import pickle
import struct
import zlib
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from main import MetroNetwork


# Current archive format version.  Bump when the file structure changes
# in a breaking way so importers can adapt.
ARCHIVE_VERSION = 1

# 4-byte magic + 2-byte version = 6-byte header for binary formats.
_BIN_MAGIC = b"UCBN"       # UrbanChain Binary
_BIN_VERSION: int = 1


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
    """Extract and rebuild a MetroNetwork from an archive dict.

    Raises:
        ValueError: if *archive_version* is unsupported.
    """
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
    """Save a network as a human-readable JSON archive.

    Args:
        network: The metro network to persist.
        filepath: Destination path (``.json`` extension is appended if
                  missing).
        meta: Optional user-supplied metadata stored under ``"meta"``
              (e.g. ``{"name": "My Map", "author": "me"}``).

    Returns:
        The resolved write path (may have been adjusted for extension).
    """
    filepath = Path(filepath)
    if filepath.suffix.lower() != ".json":
        filepath = filepath.with_suffix(".json")

    archive = _build_archive(network, meta)
    filepath.write_text(json.dumps(archive, ensure_ascii=False, indent=2),
                        encoding="utf-8")
    return filepath


def import_json(filepath: str | Path) -> MetroNetwork:
    """Load a network from a JSON archive created by :func:`export_json`.

    Args:
        filepath: Path to the ``.json`` file.

    Returns:
        A reconstructed :class:`~main.MetroNetwork`.
    """
    filepath = Path(filepath)
    data = json.loads(filepath.read_text(encoding="utf-8"))
    return _unwrap_archive(data)


# ---------------------------------------------------------------------------
# Compressed format (JSON + zlib deflate)
# ---------------------------------------------------------------------------

def export_compressed(network: MetroNetwork, filepath: str | Path,
                      meta: dict[str, Any] | None = None,
                      level: int = 6) -> Path:
    """Save a network as a zlib-compressed JSON archive (``.json.z``).

    Args:
        network: The metro network to persist.
        filepath: Destination path.
        meta:     Optional metadata dict.
        level:    zlib compression level 1-9 (default 6).

    Returns:
        The resolved write path.
    """
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
    """Save a network as a compact binary archive (``.bin``).

    Uses pickle protocol 5 + zlib compression with a 6-byte magic header
    for format identification.

    Args:
        network: The metro network to persist.
        filepath: Destination path.
        meta:     Optional metadata dict.

    Returns:
        The resolved write path.
    """
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
    """Load a network from a binary archive (``.bin``).

    Raises:
        ValueError: if the file does not start with the expected magic
                    bytes or uses an unsupported version.
    """
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
            f"Unsupported binary version {version}"
            f" (expected {_BIN_VERSION})"
        )
    pickled = zlib.decompress(raw[6:])
    data = pickle.loads(pickled)
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
