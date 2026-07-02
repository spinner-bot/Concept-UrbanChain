"""Interactive metro map — GPU-accelerated via pygfx / wgpu.

Single-scene architecture: map objects in world space, UI via
screen_space=True text.  Pan/zoom via PanZoomController.
"""

import math
from collections import defaultdict

import numpy as np
import pygfx as gfx
from rendercanvas.auto import RenderCanvas, loop

from spline import catmull_rom_spline, build_key_points, line_identifier
from calibrator import Calibrator, add_reticle_to_scene

# ---------------------------------------------------------------------------
# Style
# ---------------------------------------------------------------------------
LINE_THICKNESS = 8.0
STATION_SIZE = 14.0
TRANSFER_SIZE = 23.0


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
def _rgba(rgb, a=1.0):
    r, g, b = rgb[:3]
    return (r / 255, g / 255, b / 255, a)


def _hex(rgb):
    r, g, b = rgb[:3]
    return f"#{r:02x}{g:02x}{b:02x}"


def _luminance(rgb):
    return 0.299 * rgb[0] + 0.587 * rgb[1] + 0.114 * rgb[2]


def _fg_for(rgb):
    return "#000" if _luminance(rgb) > 128 else "#fff"


def _blend_colours(deg, angles, colours):
    if len(colours) == 1:
        return colours[0]
    weights = []
    for a in angles:
        diff = abs(deg - a)
        if diff > 180:
            diff = 360 - diff
        weights.append(math.exp(-0.5 * (diff / 25) ** 2))
    t = sum(weights) or 1e-10
    r = sum(colours[i][0] * weights[i] / t for i in range(len(colours)))
    g = sum(colours[i][1] * weights[i] / t for i in range(len(colours)))
    b = sum(colours[i][2] * weights[i] / t for i in range(len(colours)))
    return (int(r), int(g), int(b))


def _point_seg_dist(px, py, x1, y1, x2, y2):
    dx, dy = x2 - x1, y2 - y1
    if dx == 0 and dy == 0:
        return math.hypot(px - x1, py - y1)
    t = max(0, min(1, ((px - x1) * dx + (py - y1) * dy) / (dx * dx + dy * dy)))
    return math.hypot(px - (x1 + t * dx), py - (y1 + t * dy))


# ---------------------------------------------------------------------------
# MetroMapRenderer
# ---------------------------------------------------------------------------
class MetroMapRenderer:
    def __init__(self, lines):
        self._lines = lines

        # index
        self._station_lines = defaultdict(list)
        for ln in lines:
            seen = set()
            for st in ln.route:
                if st.id not in seen:
                    self._station_lines[st.id].append(ln)
                    seen.add(st.id)

        # splines
        self._spline_data = {}
        for ln in lines:
            keys = build_key_points(ln.route, ln.fine_trajectory)
            pts = catmull_rom_spline(keys, 30, ln.smooth_tension)
            self._spline_data[ln.id] = np.float32(
                [(x, y, 0.01) for x, y in pts])

        # state
        self._dark_mode = False
        self._scale_factor = 1.0
        self._hidden_lines = set()
        self._legend_mode = "partial"
        self._hovered = None
        self._mouse_screen = (0, 0)
        self._pointer_down_pos = None

        # ---- Canvas & renderer ----
        self._canvas = RenderCanvas(size=(1280, 900),
                                     title="Concept UrbanChain")
        self._renderer = gfx.renderers.WgpuRenderer(self._canvas)

        # ---- Scene & camera ----
        # UI overlay (pixel-space)
        self._ui_scene = gfx.Scene()
        self._ui_camera = gfx.OrthographicCamera(1280, 900)
        self._ui_camera.maintain_aspect = False
        self._ui_camera.local.position = (640, 450, 10)

        self._scene = gfx.Scene()
        self._camera = gfx.OrthographicCamera(20, 15)
        self._camera.maintain_aspect = False
        self._camera.local.position = (0, 0, 10)

        # ---- Controller ----
        self._controller = gfx.PanZoomController(
            self._camera, register_events=self._renderer)

        # ---- Input ----
        self._canvas.add_event_handler(self._on_key, "key_down")
        self._canvas.add_event_handler(self._on_ptr_move, "pointer_move")
        self._canvas.add_event_handler(self._on_ptr_down, "pointer_down")
        self._canvas.add_event_handler(self._on_ptr_up, "pointer_up")

        self._built = False
        self._calibrator = Calibrator()
        self._calibration_targets = []  # world coords of 3x3 grid

    # ------------------------------------------------------------------
    def show(self):
        self._auto_fit()
        self._rebuild_map()
        self._rebuild_ui()

        def draw():
            # Update UI camera to match canvas size
            lw, lh = self._canvas.get_logical_size()
            if lw and lh:
                self._ui_camera.width = lw
                self._ui_camera.height = lh
                self._ui_camera.local.position = (lw / 2, lh / 2, 10)
            self._renderer.render(self._scene, self._camera, flush=False)
            self._renderer.render(self._ui_scene, self._ui_camera, flush=True)

        self._canvas.request_draw(draw)
        loop.run()

    # ------------------------------------------------------------------
    def _auto_fit(self):
        xs, ys = [], []
        for ln in self._lines:
            for st in ln.route:
                xs.append(st.position[0])
                ys.append(st.position[1])
        if not xs:
            return
        ex, ey = max(xs) - min(xs) or 1, max(ys) - min(ys) or 1
        pad = max(ex, ey) * 0.18
        # Match viewport aspect ratio
        lw, lh = self._canvas.get_logical_size()
        lw, lh = lw or 1280, lh or 900
        view_aspect = lw / lh
        world_w = ex + pad * 2
        world_h = ey + pad * 2
        world_aspect = world_w / world_h
        if world_aspect > view_aspect:
            # World is wider — expand height
            world_h = world_w / view_aspect
        else:
            world_w = world_h * view_aspect
        self._camera.width = world_w
        self._camera.height = world_h
        self._camera.local.position = (
            (min(xs) + max(xs)) / 2,
            (min(ys) + max(ys)) / 2,
            10,
        )

    # ------------------------------------------------------------------
    def _rebuild_map(self):
        self._scene.clear()
        if self._calibrator.active:
            bg = (0, 0, 0, 1)
        else:
            bg = (0.06, 0.06, 0.06, 1) if self._dark_mode else (0.94, 0.94, 0.94, 1)
        self._scene.add(gfx.Background(
            None, gfx.BackgroundMaterial(bg, bg, bg, bg)))

        if not self._calibrator.active:
            for ln in self._lines:
                if ln.id not in self._hidden_lines:
                    self._add_line(ln)

            drawn = set()
            for ln in self._lines:
                for st in ln.route:
                    if st.id in drawn:
                        continue
                    drawn.add(st.id)
                    vis = [l for l in self._station_lines[st.id]
                           if l.id not in self._hidden_lines]
                    if vis:
                        self._add_station(st, vis)

            self._draw_labels()
        else:
            self._draw_calibration_reticles()
        self._built = True

    def _draw_calibration_reticles(self):
        """Draw calibration targets on the map."""
        base_r = self._camera.width / 30
        for i, (wx, wy) in enumerate(self._calibration_targets):
            highlight = (i == self._calibrator.step())
            add_reticle_to_scene(self._scene, wx, wy, base_r, highlight)

    def _rebuild_ui(self):
        self._ui_scene.clear()
        self._draw_legend()

    # ------------------------------------------------------------------
    def _add_line(self, ln):
        pts = self._spline_data[ln.id]
        if len(pts) < 2:
            return
        hovered = (self._hovered and self._hovered.get("type") == "line"
                   and self._hovered.get("line") is ln)
        t = self._line_thickness() * 1.5 if hovered else self._line_thickness()
        self._scene.add(gfx.Line(
            gfx.Geometry(positions=pts),
            gfx.LineMaterial(thickness=t, thickness_space="screen",
                             color=_rgba(ln.color)),
        ))

    def _line_thickness(self):
        return LINE_THICKNESS * self._scale_factor

    # ------------------------------------------------------------------
    def _add_station(self, st, slines):
        x, y = st.position[0], st.position[1]
        is_xfer = len(slines) >= 2
        size = (TRANSFER_SIZE if is_xfer else STATION_SIZE) * self._scale_factor
        ring_sz = size + 5 * self._scale_factor
        fg = (1, 1, 1, 1) if self._dark_mode else (0, 0, 0, 1)

        hovered = (self._hovered and self._hovered.get("type") == "station"
                   and self._hovered.get("station") is st)
        if hovered:
            ring_sz += 3 * self._scale_factor

        # ring (z above lines which are at 0.01)
        self._scene.add(gfx.Points(
            gfx.Geometry(positions=np.float32([(x, y, 0.02)])),
            gfx.PointsMaterial(size=ring_sz, size_space="screen", color=fg),
        ))
        # centre (z above ring)
        self._scene.add(gfx.Points(
            gfx.Geometry(positions=np.float32([(x, y, 0.03)])),
            gfx.PointsMaterial(size=size, size_space="screen",
                               color=_rgba(slines[0].color)),
        ))

    # ------------------------------------------------------------------
    def _draw_labels(self):
        font_sz = 11  # screen pixels — consistent size
        for ln in self._lines:
            if ln.hide_terminal_label or ln.id in self._hidden_lines:
                continue
            pts = self._spline_data[ln.id]
            if len(pts) < 2:
                continue
            label = line_identifier(ln.id, ln.name)
            for st, is_first in [(ln.route[0], True), (ln.route[-1], False)]:
                sx, sy = st.position[0], st.position[1]
                # Project world → screen
                spx, spy = self._world_to_screen(sx, sy)
                # Tangent in screen space for offset direction
                bi = int(np.argmin(np.sum((pts[:, :2] - (sx, sy))**2, axis=1)))
                if bi <= 0:
                    dx = float(pts[min(1, len(pts)-1), 0] - pts[0, 0])
                    dy = float(pts[min(1, len(pts)-1), 1] - pts[0, 1])
                else:
                    dx = float(pts[-1, 0] - pts[-2, 0])
                    dy = float(pts[-1, 1] - pts[-2, 1])
                mag = math.sqrt(dx*dx + dy*dy) or 1e-10
                nx, ny = -dy/mag, dx/mag
                sign = -1 if is_first else 1
                offset = (STATION_SIZE * self._scale_factor + font_sz) * sign
                spx += nx * offset
                spy += ny * offset
                fg_c = "#fff" if self._dark_mode else "#111"
                t = gfx.Text(text=label, font_size=font_sz,
                              screen_space=True, anchor="middle-center",
                              material=gfx.TextMaterial(color=fg_c))
                t.local.position = (spx, spy, 0)
                self._scene.add(t)

    def _world_to_screen_raw(self, wx, wy):
        """World to screen WITHOUT calibration correction."""
        cw, ch = self._camera.width, self._camera.height
        cx, cy, _ = self._camera.local.position
        lw, lh = self._canvas.get_logical_size()
        lw, lh = lw or 1280, lh or 900
        return ((wx - cx) / cw * lw + lw / 2,
                (wy - cy) / ch * lh + lh / 2)

    def _world_to_screen(self, wx, wy):
        """Convert world coords to screen pixel coords."""
        cw, ch = self._camera.width, self._camera.height
        cx, cy, _ = self._camera.local.position
        lw, lh = self._canvas.get_logical_size()
        lw, lh = lw or 1280, lh or 900
        sx = (wx - cx) / cw * lw + lw / 2
        sy = (wy - cy) / ch * lh + lh / 2
        return sx, sy

    # ------------------------------------------------------------------
    def _draw_legend(self):
        fg = "#ddd" if self._dark_mode else "#222"
        lw = self._ui_camera.width or 1280
        lh = self._ui_camera.height or 900
        y0 = lh - 40
        for i, ln in enumerate(self._lines):
            y = y0 - i * 32
            hidden = ln.id in self._hidden_lines
            c = _rgba(ln.color)
            if hidden:
                c = (c[0]*0.3, c[1]*0.3, c[2]*0.3, 0.5)
            label = line_identifier(ln.id, ln.name)
            lc = "#555" if hidden else fg
            # swatch rectangle
            geo = gfx.Geometry(
                positions=np.float32([(30, y, 0), (50, y, 0),
                                      (50, y + 14, 0), (30, y + 14, 0)]),
                indices=np.int32([[0, 1, 2], [0, 2, 3]]),
            )
            self._ui_scene.add(gfx.Mesh(geo, gfx.MeshBasicMaterial(color=c)))
            # text
            t = gfx.Text(text=label, font_size=14, screen_space=True,
                          anchor="middle-left",
                          material=gfx.TextMaterial(color=lc))
            t.local.position = (58, y + 7, 0)
            self._ui_scene.add(t)

    # ------------------------------------------------------------------
    # Input
    # ------------------------------------------------------------------
    def _start_calibration(self):
        """Set up a 3x3 calibration grid and enter calibration mode."""
        cw, ch = self._camera.width, self._camera.height
        cx, cy, _ = self._camera.local.position
        # 3x3 grid: 20%, 50%, 80% of visible area
        fracs = [0.2, 0.5, 0.8]
        targets = []
        for fy in fracs:
            for fx in fracs:
                wx = cx + (fx - 0.5) * cw
                wy = cy + (fy - 0.5) * ch
                targets.append((wx, wy))
        self._calibration_targets = targets
        self._calibrator.start(targets)
        self._rebuild_map()
        self._canvas.request_draw(self._render_frame)

    def _on_key(self, event):
        k = event.get("key", "")
        if k in ("b", "B"):
            self._dark_mode = not self._dark_mode
            self._rebuild_map()
            self._rebuild_ui()
            self._canvas.request_draw(self._render_frame)
        elif k == "[":
            self._scale_factor = max(0.3, self._scale_factor - 0.1)
            self._rebuild_map()
            self._rebuild_ui()
            self._canvas.request_draw(self._render_frame)
        elif k == "]":
            self._scale_factor = min(3.0, self._scale_factor + 0.1)
            self._rebuild_map()
            self._rebuild_ui()
            self._canvas.request_draw(self._render_frame)
        elif k in ("f", "F"):
            if self._calibrator.active:
                self._calibrator.cancel()
                self._rebuild_map()
                self._canvas.request_draw(self._render_frame)
            else:
                self._start_calibration()

    def _render_frame(self):
        self._renderer.render(self._scene, self._camera)

    def _screen_to_world(self, sx, sy):
        # Apply calibration correction first
        csx, csy = self._calibrator.apply(sx, sy)
        cw, ch = self._camera.width, self._camera.height
        cx, cy, _ = self._camera.local.position
        lw, lh = self._canvas.get_logical_size()
        lw, lh = lw or 1280, lh or 900
        return cx + (csx / lw - 0.5) * cw, cy + (csy / lh - 0.5) * ch
    def _hit_test(self, wx, wy):
        line_threshold = self._camera.width / 35
        station_threshold = self._camera.width / 20  # larger = easier to select
        # Check stations FIRST (higher priority)
        for st_id, slines in self._station_lines.items():
            st = next((s for ln in self._lines for s in ln.route
                       if s.id == st_id), None)
            if st is None:
                continue
            if math.hypot(wx - st.position[0], wy - st.position[1]) < station_threshold:
                return {"type": "station", "station": st, "lines": slines}
        # Then lines
        for ln in self._lines:
            if ln.id in self._hidden_lines:
                continue
            pts = self._spline_data[ln.id]
            for i in range(len(pts) - 1):
                if _point_seg_dist(wx, wy, float(pts[i, 0]), float(pts[i, 1]),
                                   float(pts[i+1, 0]), float(pts[i+1, 1])) < line_threshold:
                    return {"type": "line", "line": ln}
        return None

    def _on_ptr_move(self, event):
        self._mouse_screen = (event["x"], event["y"])
        wx, wy = self._screen_to_world(event["x"], event["y"])
        hit = self._hit_test(wx, wy)
        prev = self._hovered
        self._hovered = hit
        if prev != hit:
            self._rebuild_map()
            self._rebuild_ui()
            self._canvas.request_draw(self._render_frame)

    def _on_ptr_down(self, event):
        self._pointer_down_pos = (event["x"], event["y"])

    def _on_ptr_up(self, event):
        if self._pointer_down_pos is None:
            return
        dx = event["x"] - self._pointer_down_pos[0]
        dy = event["y"] - self._pointer_down_pos[1]
        self._pointer_down_pos = None
        if (dx*dx + dy*dy)**0.5 > 5:
            return  # drag, not click

        # Legend click
        ly0 = self._canvas.get_logical_size()[1] - 40
        for i, ln in enumerate(self._lines):
            ly = ly0 - i * 32
            if 25 <= event["x"] <= 180 and ly - 10 <= event["y"] <= ly + 10:
                if ln.id in self._hidden_lines:
                    self._hidden_lines.discard(ln.id)
                else:
                    self._hidden_lines.add(ln.id)
                self._rebuild_map()
                self._rebuild_ui()
                self._canvas.request_draw(self._render_frame)
                return

        # Calibration click
        if self._calibrator.active:
            self._calibrator.register_click((event["x"], event["y"]))
            if not self._calibrator.active:
                # Just finished — compute ideal screen positions
                ideal = []
                for wx, wy in self._calibration_targets:
                    sx, sy = self._world_to_screen_raw(wx, wy)
                    ideal.append((sx, sy))
                self._calibrator.compute_offset(ideal)
                self._rebuild_ui()
            self._rebuild_map()
            self._canvas.request_draw(self._render_frame)
            return

        # Map click
        wx, wy = self._screen_to_world(event["x"], event["y"])
        hit = self._hit_test(wx, wy)
        if hit:
            t = hit["type"]
            name = hit.get("station", hit.get("line")).name
            lid = getattr(hit.get("line"), "id", "")
            print(f"[click] {t}: {name or lid}")
