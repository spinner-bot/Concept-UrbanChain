"""Interactive metro map renderer — GPU-accelerated via pygfx / wgpu."""

import math
from collections import defaultdict

import numpy as np
import pygfx as gfx
from rendercanvas.auto import RenderCanvas, loop

from spline import catmull_rom_spline, build_key_points

# ---------------------------------------------------------------------------
# Style (screen-space px — task 2 will add dynamic scaling)
# ---------------------------------------------------------------------------
LINE_THICKNESS = 8.0
STATION_SIZE = 14.0          # regular centre circle diameter
TRANSFER_SIZE = 23.0          # transfer centre circle diameter  (≈1.65×)


# ---------------------------------------------------------------------------
# Colour helpers
# ---------------------------------------------------------------------------
def _rgba(rgb: tuple, a: float = 1.0) -> tuple:
    r, g, b = rgb[:3]
    return (r / 255, g / 255, b / 255, a)


def _luminance(rgb: tuple) -> float:
    return 0.299 * rgb[0] + 0.587 * rgb[1] + 0.114 * rgb[2]


# ===================================================================
# MetroMapRenderer
# ===================================================================
class MetroMapRenderer:
    """GPU metro map with pan, zoom, and dark/light toggle."""

    def __init__(self, lines: list) -> None:
        self._lines = lines

        # Station → lines index (transfer detection)
        self._station_lines: dict[int, list] = defaultdict(list)
        for ln in lines:
            seen: set[int] = set()
            for st in ln.route:
                if st.id not in seen:
                    self._station_lines[st.id].append(ln)
                    seen.add(st.id)

        # Precompute splines as N×3 numpy arrays
        self._spline_data: dict[int, np.ndarray] = {}
        for ln in lines:
            keys = build_key_points(ln.route, ln.fine_trajectory)
            pts = catmull_rom_spline(keys, 30, ln.smooth_tension)
            self._spline_data[ln.id] = np.array(
                [(x, y, 0.0) for x, y in pts], dtype=np.float32,
            )

        # State
        self._dark_mode = False

        # ---- Canvas, renderer, scene, camera ----
        self._canvas = RenderCanvas(size=(1280, 900), title="Concept UrbanChain")
        self._renderer = gfx.renderers.WgpuRenderer(self._canvas)
        self._scene = gfx.Scene()
        self._camera = gfx.OrthographicCamera()
        self._camera.maintain_aspect = True

        # UI overlay scene (screen-space pixels)
        self._ui_scene = gfx.Scene()
        self._ui_camera = gfx.OrthographicCamera()
        self._ui_camera.maintain_aspect = False
        # Position at (w/2, h/2) so that pixel (0,0) is bottom-left
        self._ui_camera.width = 1280
        self._ui_camera.height = 900
        self._ui_camera.local.position = (640, 450, 0)

        # ---- Controller (pan + zoom) ----
        self._controller = gfx.PanZoomController(self._camera)
        self._controller.add_default_event_handlers(
            self._canvas, self._camera,
        )

        # ---- Keyboard ----
        self._canvas.add_event_handler(self._on_key, "key_down")

        # Defer build until first frame (canvas size is known)
        self._built = False

    # ------------------------------------------------------------------
    def show(self) -> None:
        @self._renderer.add_event_handler("before_render")
        def _render(_event):
            if not self._built:
                self._auto_fit()
                self._build_scene()
                self._build_ui()
                self._built = True
            self._renderer.render(self._scene, self._camera)
            self._renderer.render(self._ui_scene, self._ui_camera,
                                  clear_color=False, clear_depth=True)

        loop.run()

    # ------------------------------------------------------------------
    # Camera
    # ------------------------------------------------------------------
    def _auto_fit(self) -> None:
        xs, ys = [], []
        for ln in self._lines:
            for st in ln.route:
                xs.append(st.position[0])
                ys.append(st.position[1])
        if not xs:
            return
        ex = max(xs) - min(xs) or 1
        ey = max(ys) - min(ys) or 1
        pad = max(ex, ey) * 0.18
        self._camera.width = ex + pad * 2
        self._camera.height = ey + pad * 2
        self._camera.local.position = (
            (min(xs) + max(xs)) / 2,
            (min(ys) + max(ys)) / 2,
            0,
        )

    # ------------------------------------------------------------------
    # Scene construction
    # ------------------------------------------------------------------
    def _build_scene(self) -> None:
        self._scene.clear()

        # Background
        bg = (0.06, 0.06, 0.06, 1) if self._dark_mode else (0.94, 0.94, 0.94, 1)
        self._scene.add(
            gfx.Background(None, gfx.BackgroundMaterial(bg, bg, bg, bg)),
        )

        # Lines (bottom layer)
        for ln in self._lines:
            self._add_line(ln)

        # Stations (top layer — deduplicated)
        drawn: set[int] = set()
        for ln in self._lines:
            for st in ln.route:
                if st.id in drawn:
                    continue
                drawn.add(st.id)
                self._add_station(st, self._station_lines[st.id])

        # Terminal labels
        for ln in self._lines:
            self._add_terminal_label(ln)

    # ------------------------------------------------------------------
    # Line
    # ------------------------------------------------------------------
    def _add_line(self, ln) -> None:
        pts = self._spline_data[ln.id]
        if len(pts) < 2:
            return
        self._scene.add(gfx.Line(
            gfx.Geometry(positions=pts),
            gfx.LineMaterial(
                thickness=LINE_THICKNESS,
                thickness_space="screen",
                color=_rgba(ln.color),
            ),
        ))

    # ------------------------------------------------------------------
    # Station
    # ------------------------------------------------------------------
    def _add_station(self, st, slines: list) -> None:
        x, y = st.position[0], st.position[1]
        is_transfer = len(slines) >= 2
        size = TRANSFER_SIZE if is_transfer else STATION_SIZE

        # Outer ring (fg colour = black or white depending on mode)
        fg = (1, 1, 1, 1) if self._dark_mode else (0, 0, 0, 1)
        ring_size = size + 5
        ring = gfx.Points(
            gfx.Geometry(positions=np.float32([(x, y, 0.002)])),
            gfx.PointsMaterial(size=ring_size, size_space="screen", color=fg),
        )
        self._scene.add(ring)

        # Centre — for transfer stations use blended colour
        if is_transfer:
            colours = [_rgba(l.color) for l in slines]
            blended = tuple(sum(c[i] for c in colours) / len(colours)
                            for i in range(4))
        else:
            blended = _rgba(slines[0].color)

        centre = gfx.Points(
            gfx.Geometry(positions=np.float32([(x, y, 0.003)])),
            gfx.PointsMaterial(size=size, size_space="screen", color=blended),
        )
        self._scene.add(centre)

    # ------------------------------------------------------------------
    # UI overlay
    # ------------------------------------------------------------------
    def _build_ui(self) -> None:
        self._ui_scene.clear()
        self._draw_legend()

    def _draw_legend(self) -> None:
        fg = "#ddd" if self._dark_mode else "#222"
        y_base = self._ui_camera.height - 40
        for i, ln in enumerate(self._lines):
            y = y_base - i * 32
            # Swatch
            geo = gfx.Geometry(
                positions=np.float32([(30, y, 0), (55, y, 0),
                                      (55, y + 16, 0), (30, y + 16, 0)]),
                indices=np.int32([[0, 1, 2], [0, 2, 3]]),
            )
            mat = gfx.MeshBasicMaterial(color=_rgba(ln.color))
            self._ui_scene.add(gfx.Mesh(geo, mat))

            # Label
            label = ln.name or f"地铁{ln.id}号线"
            text = gfx.Text(
                text=label,
                font_size=14,
                screen_space=True,
                anchor="middle-left",
                material=gfx.TextMaterial(color=fg),
            )
            text.local.position = (62, y + 8, 0)
            self._ui_scene.add(text)

    # ------------------------------------------------------------------
    # Terminal labels
    # ------------------------------------------------------------------
    def _add_terminal_label(self, ln) -> None:
        pts = self._spline_data[ln.id]
        if len(pts) < 2:
            return
        text_str = ln.name or f"地铁{ln.id}号线"
        font_size = self._camera.width / 75  # world units, scales with zoom

        for st, is_start in [(ln.route[0], True), (ln.route[-1], False)]:
            sx, sy = st.position[0], st.position[1]

            # Tangent direction at terminal
            best_i = int(np.argmin(np.sum((pts[:, :2] - (sx, sy)) ** 2, axis=1)))
            if best_i <= 0:
                dx, dy = float(pts[1, 0] - pts[0, 0]), float(pts[1, 1] - pts[0, 1])
            else:
                dx, dy = float(pts[-1, 0] - pts[-2, 0]), float(pts[-1, 1] - pts[-2, 1])

            mag = math.sqrt(dx * dx + dy * dy) or 1e-10
            nx, ny = -dy / mag, dx / mag
            sign = -1 if is_start else 1
            offset = font_size * 1.8 * sign
            lx, ly = sx + nx * offset, sy + ny * offset

            fg = "#fff" if self._dark_mode else "#111"
            text = gfx.Text(
                text=text_str,
                font_size=font_size,
                screen_space=False,
                anchor="middle-center",
                material=gfx.TextMaterial(color=fg),
            )
            text.local.position = (lx, ly, 0.005)
            self._scene.add(text)

    # ------------------------------------------------------------------
    # Keyboard
    # ------------------------------------------------------------------
    def _on_key(self, event) -> None:
        if event.get("key", "") in ("b", "B"):
            self._dark_mode = not self._dark_mode
            self._build_scene()
            self._canvas.request_draw()
