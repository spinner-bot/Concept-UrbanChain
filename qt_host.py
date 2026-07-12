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
        self._map_actions: list = []  # dynamic per-map QActions
        self.setWindowTitle(title)
        self.resize(1280, 900)

        # Central widget — the pygfx canvas.
        self.setCentralWidget(
            QtWidgets.QWidget.createWindowContainer(renderer._canvas)
        )

        self._setup_menus()
        self._setup_status_bar()

        # Refresh Qt menus after every map change
        renderer.on_map_changed = self._on_map_changed

    # -- menus ---------------------------------------------------------------

    def _setup_menus(self):
        from lang import t, toggle_language as _tl
        menu_bar = self.menuBar()

        # --- File ---
        file_menu = menu_bar.addMenu(t("menu_file"))
        act = file_menu.addAction(t("open_map_list"))
        act.triggered.connect(lambda: self._renderer._toggle_map_menu())

        file_menu.addSeparator()

        act = file_menu.addAction(t("save_map"))
        act.triggered.connect(
            lambda: self._renderer._save_current_map()
            if self._renderer._edit_mode else None
        )

        act = file_menu.addAction(t("export_json"))
        act.triggered.connect(self._export_json_dialog)

        act = file_menu.addAction(t("import_json"))
        act.triggered.connect(self._import_json_dialog)

        file_menu.addSeparator()
        act = file_menu.addAction(t("exit_app"))
        act.triggered.connect(self.close)

        # --- View ---
        view_menu = menu_bar.addMenu(t("menu_view"))
        act = view_menu.addAction(t("toggle_dark_mode"))
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
        act = view_menu.addAction(t("toggle_language"))
        act.triggered.connect(
            lambda: _tl() or self._renderer._rebuild_map()
            or self._renderer._rebuild_ui()
            or self._renderer._canvas.request_draw(
                self._renderer._render_frame
            )
        )

        # --- Edit ---
        edit_menu = menu_bar.addMenu(t("menu_edit"))
        act = edit_menu.addAction(t("toggle_edit_mode"))
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
        self._maps_menu = menu_bar.addMenu(t("menu_maps"))
        act = self._maps_menu.addAction(t("map_list_item"))
        act.triggered.connect(lambda: self._renderer._toggle_map_menu())
        act = self._maps_menu.addAction(t("new_map_item"))
        act.triggered.connect(lambda: self._renderer._create_new_map())

        self._maps_menu.addSeparator()
        self._rebuild_maps_menu()

        # --- UI Mode ---
        mode_menu = menu_bar.addMenu(t("menu_mode"))
        act = mode_menu.addAction(t("switch_to_full"))
        act.triggered.connect(lambda: self._toggle_ui_mode("full"))

    # -- status bar ----------------------------------------------------------

    def _setup_status_bar(self):
        self._status = self.statusBar()
        self._update_status_bar()

    def _update_status_bar(self):
        """Refresh the status bar with the current map display name."""
        from lang import t
        from maps import get_map
        map_key = self._renderer._map_key
        entry = get_map(map_key) if map_key else None
        display = entry[0] if entry else (map_key or "(none)")
        self._status.showMessage(t("status_map_prefix") + display)

    # -- dynamic map menu ----------------------------------------------------

    def _rebuild_maps_menu(self):
        """Rebuild the per-map switch actions in the Maps menu."""
        from maps import list_maps
        # Remove old dynamic actions
        for act in self._map_actions:
            self._maps_menu.removeAction(act)
        self._map_actions.clear()

        current_key = self._renderer._map_key
        for key, display_name in list_maps():
            label = f"{display_name}  [{key}]"
            act = self._maps_menu.addAction(label)
            act.setCheckable(True)
            act.setChecked(key == current_key)
            act.triggered.connect(
                lambda checked, k=key: self._switch_to_map(k)
            )
            self._map_actions.append(act)

        # If no maps registered at all, show a placeholder
        if not self._map_actions:
            from lang import t
            act = self._maps_menu.addAction(t("no_maps"))
            act.setEnabled(False)
            self._map_actions.append(act)

    def _switch_to_map(self, map_key: str):
        """Switch to *map_key* and update menus + status bar."""
        self._renderer._switch_to_map(map_key)
        self._rebuild_maps_menu()
        self._update_status_bar()

    def _on_map_changed(self):
        """Called by the renderer after any map change."""
        self._rebuild_maps_menu()
        self._update_status_bar()

    # -- dialogs -------------------------------------------------------------

    def _export_json_dialog(self):
        from lang import t
        path, _ = QtWidgets.QFileDialog.getSaveFileName(
            self, t("export_dialog_title"), "", "JSON (*.json);;All (*)",
        )
        if path and self._renderer._network:
            from archive import export_json
            export_json(
                self._renderer._network, Path(path),
                meta={"map_key": self._renderer._map_key},
            )
            self._status.showMessage(t("exported_msg") + str(path))

    def _import_json_dialog(self):
        from lang import t
        path, _ = QtWidgets.QFileDialog.getOpenFileName(
            self, t("import_dialog_title"), "", "JSON (*.json);;All (*)",
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
            self._status.showMessage(t("imported_msg") + key)

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
