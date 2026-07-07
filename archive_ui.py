"""Archive UI helpers — cross-platform file dialogs and import/export wizards.

Tries Qt, then tkinter, then falls back to plain-text path prompts.
"""

from __future__ import annotations

from pathlib import Path
from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from main import MetroNetwork


# ---------------------------------------------------------------------------
# File dialog — best-effort native dialog
# ---------------------------------------------------------------------------

def _get_qt_dialogs():
    """Return QFileDialog helpers if PySide6 is available."""
    try:
        from PySide6 import QtWidgets
        return QtWidgets.QFileDialog
    except ImportError:
        return None


def _get_tk_dialogs():
    """Return tkinter filedialog helpers if available."""
    try:
        from tkinter import Tk, filedialog
        root = Tk()
        root.withdraw()  # hide the root window
        return filedialog, root
    except ImportError:
        return None


def save_file_dialog(title: str = "Save",
                     default_name: str = "map.json",
                     filters: str = "JSON (*.json);;All (*)") -> str | None:
    """Open a native save-file dialog and return the chosen path.

    Returns *None* if the user cancelled.
    """
    # 1. Qt
    qfd = _get_qt_dialogs()
    if qfd is not None:
        path, _ = qfd.getSaveFileName(None, title, default_name, filters)
        return path or None

    # 2. tkinter
    tk = _get_tk_dialogs()
    if tk is not None:
        fd, root = tk
        path = fd.asksaveasfilename(
            title=title, initialfile=default_name,
            filetypes=[("JSON", "*.json"), ("All", "*")],
        )
        root.destroy()
        return path or None

    # 3. Fallback — console prompt.
    try:
        return input(f"{title} [{default_name}]: ") or default_name
    except (EOFError, KeyboardInterrupt):
        return None


def open_file_dialog(title: str = "Open",
                     filters: str = "JSON (*.json);;All (*)") -> str | None:
    """Open a native open-file dialog and return the chosen path.

    Returns *None* if the user cancelled.
    """
    # 1. Qt
    qfd = _get_qt_dialogs()
    if qfd is not None:
        path, _ = qfd.getOpenFileName(None, title, "", filters)
        return path or None

    # 2. tkinter
    tk = _get_tk_dialogs()
    if tk is not None:
        fd, root = tk
        path = fd.askopenfilename(
            title=title,
            filetypes=[("JSON", "*.json"), ("All", "*")],
        )
        root.destroy()
        return path or None

    # 3. Fallback
    try:
        return input(f"{title}: ") or None
    except (EOFError, KeyboardInterrupt):
        return None


# ---------------------------------------------------------------------------
# High-level export / import flows
# ---------------------------------------------------------------------------

def export_via_dialog(network: "MetroNetwork",
                      map_key: str = "") -> Path | None:
    """Full export flow: ask for path, format, optional password, then save.

    Returns the written path, or *None* if cancelled.
    """
    path_str = save_file_dialog("Export Map", f"{map_key or 'map'}.json")
    if not path_str:
        return None
    path = Path(path_str)

    fmt = path.suffix.lower()
    from archive import (
        export_json, export_compressed, export_binary, export_encrypted,
    )

    meta = {"map_key": map_key} if map_key else None

    if fmt == ".z":
        return export_compressed(network, path, meta=meta)
    elif fmt == ".bin":
        return export_binary(network, path, meta=meta)
    elif fmt == ".enc":
        try:
            import getpass
            pwd = getpass.getpass("Password: ")
        except (ImportError, EOFError):
            pwd = input("Password: ")
        return export_encrypted(network, path, pwd, meta=meta)
    else:
        return export_json(network, path, meta=meta)


def import_via_dialog() -> "MetroNetwork | None":
    """Full import flow: ask for file, detect format, reconstruct network.

    Returns the imported network or *None* if cancelled / failed.
    """
    path_str = open_file_dialog("Import Map")
    if not path_str:
        return None
    path = Path(path_str)

    from archive import (
        import_json, import_compressed, import_binary, import_encrypted,
    )

    fmt = path.suffix.lower()
    try:
        if fmt == ".z":
            return import_compressed(path)
        elif fmt == ".bin":
            return import_binary(path)
        elif fmt == ".enc":
            try:
                import getpass
                pwd = getpass.getpass("Password: ")
            except (ImportError, EOFError):
                pwd = input("Password: ")
            return import_encrypted(path, pwd)
        else:
            return import_json(path)
    except Exception as e:
        print(f"[archive] Import failed: {e}")
        return None
