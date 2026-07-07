"""Qt host window for Concept UrbanChain (full mode).

Wraps the pygfx :class:`~renderer.MetroMapRenderer` inside a QMainWindow
with native menu bar, status bar, and dockable panels.

If PySide6 is not installed the import succeeds but :func:`run_qt_app`
raises :class:`RuntimeError` with a helpful message.
"""

from __future__ import annotations

import sys
from pathlib import Path
from typing import TYPE_CHECKING

from ui_config import config

if TYPE_CHECKING:
    from renderer import MetroMapRenderer


# ---------------------------------------------------------------------------
# Optional Qt import
# ---------------------------------------------------------------------------

_QT_AVAILABLE = False
try:
    from PySide6 import QtWidgets, QtCore, QtGui  # noqa: F401
    _QT_AVAILABLE = True
except ImportError:
    pass


def _require_qt():
    if not _QT_AVAILABLE:
        raise RuntimeError(
            "Full UI mode requires PySide6.  Install it with:\n"
            "    pip install PySide6"
        )


# ---------------------------------------------------------------------------
# Qt main window
# ---------------------------------------------------------------------------

class MainWindow(QtWidgets.QMainWindow if _QT_AVAILABLE else object):
    """QMainWindow hosting the pygfx renderer with native menus."""

    def __init__(self, renderer: "MetroMapRenderer",
                 title: str = "Concept UrbanChain"):
        _require_qt()
        super().__init__()
        self._renderer = renderer
        self.setWindowTitle(title)
        self.resize(1280, 900)

        # Central widget — the pygfx canvas.
        self.setCentralWidget(
            QtWidgets.QWidget.createWindowContainer(renderer._canvas)
        )

        self._setup_menus()
        self._setup_status_bar()

    # -- menus ---------------------------------------------------------------

    def _setup_menus(self):
        menu_bar = self.menuBar()

        # --- File ---
        file_menu = menu_bar.addMenu("&File")
        act = file_menu.addAction("&Open Map List\tM")
        act.triggered.connect(lambda: self._renderer._toggle_map_menu())

        file_menu.addSeparator()

        act = file_menu.addAction("&Save Map\tCtrl+S")
        act.triggered.connect(
            lambda: self._renderer._save_current_map()
            if self._renderer._edit_mode else None
        )

        act = file_menu.addAction("E&xport JSON...\tCtrl+E")
        act.triggered.connect(self._export_json_dialog)

        act = file_menu.addAction("&Import JSON...\tCtrl+I")
        act.triggered.connect(self._import_json_dialog)

        file_menu.addSeparator()
        act = file_menu.addAction("E&xit\tAlt+F4")
        act.triggered.connect(self.close)

        # --- View ---
        view_menu = menu_bar.addMenu("&View")
        act = view_menu.addAction("Toggle &Dark Mode\tB")
        act.triggered.connect(
            lambda: setattr(
                self._renderer, '_dark_mode',
                not self._renderer._dark_mode
            ) or self._renderer._rebuild_map()
            or self._renderer._rebuild_ui()
            or self._renderer._canvas.request_draw(
                self._renderer._render_frame
            )
        )
        act = view_menu.addAction("Toggle &Language\tL")
        from lang import toggle_language as _tl
        act.triggered.connect(
            lambda: _tl() or self._renderer._rebuild_map()
            or self._renderer._rebuild_ui()
            or self._renderer._canvas.request_draw(
                self._renderer._render_frame
            )
        )

        # --- Edit ---
        edit_menu = menu_bar.addMenu("&Edit")
        act = edit_menu.addAction("Toggle Edit &Mode\tE")
        act.triggered.connect(
            lambda: setattr(
                self._renderer, '_edit_mode',
                not self._renderer._edit_mode
            ) or self._renderer._rebuild_map()
            or self._renderer._rebuild_ui()
            or self._renderer._canvas.request_draw(
                self._renderer._render_frame
            )
        )

        # --- Maps ---
        maps_menu = menu_bar.addMenu("&Maps")
        act = maps_menu.addAction("&Map List...\tM")
        act.triggered.connect(lambda: self._renderer._toggle_map_menu())
        act = maps_menu.addAction("&New Map\tCtrl+N")
        act.triggered.connect(lambda: self._renderer._create_new_map())

        # --- UI Mode ---
        mode_menu = menu_bar.addMenu("&Mode")
        act = mode_menu.addAction("Switch to &Full\tCtrl+Shift+F")
        act.triggered.connect(lambda: self._toggle_ui_mode("full"))

    # -- status bar ----------------------------------------------------------

    def _setup_status_bar(self):
        self._status = self.statusBar()
        self._status.showMessage(
            f"Map: {self._renderer._map_key or '(none)'}"
        )

    # -- dialogs -------------------------------------------------------------

    def _export_json_dialog(self):
        path, _ = QtWidgets.QFileDialog.getSaveFileName(
            self, "Export Map", "", "JSON (*.json);;All (*)",
        )
        if path and self._renderer._network:
            from archive import export_json
            export_json(
                self._renderer._network, Path(path),
                meta={"map_key": self._renderer._map_key},
            )
            self._status.showMessage(f"Exported: {path}")

    def _import_json_dialog(self):
        path, _ = QtWidgets.QFileDialog.getOpenFileName(
            self, "Import Map", "", "JSON (*.json);;All (*)",
        )
        if path:
            from archive import import_json
            from maps import register_map, next_user_key
            net = import_json(Path(path))
            key = next_user_key(Path(path).stem)
            register_map(key, Path(path).stem, lambda n=net: n)
            self._renderer.reload_network(
                net.lines, map_key=key, network=net,
            )
            self._status.showMessage(f"Imported: {key}")

    # -- ui mode switch ------------------------------------------------------

    def _toggle_ui_mode(self, target: str):
        config.mode = target
        self._renderer._rebuild_ui()
        self._renderer._canvas.request_draw(
            self._renderer._render_frame
        )


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def run_qt_app(renderer: "MetroMapRenderer") -> None:
    """Launch the Qt host window (full mode)."""
    _require_qt()
    app = QtWidgets.QApplication(sys.argv)
    window = MainWindow(renderer)
    window.show()
    app.exec()
