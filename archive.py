"""Archive system — save and load metro networks to/from files.

Supported formats are built up incrementally across sub-tasks:

- JSON (plain)            — human-readable, ``.json``
- Binary (MessagePack)    — compact, ``.msgpack``          (task 5.2)
- Compressed (zlib)       — JSON + deflate, ``.json.z``    (task 5.2)
- Encrypted (AES-256-GCM) — password-protected, ``.enc``   (task 5.3)
"""

import json
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from main import MetroNetwork


# Current archive format version.  Bump when the file structure changes
# in a breaking way so importers can adapt.
ARCHIVE_VERSION = 1


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
