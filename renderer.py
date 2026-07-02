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
        self._scene = gfx.Scene()
        self._camera = gfx.OrthographicCamera(20, 15)
        self._camera.maintain_aspect = True
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

    # ------------------------------------------------------------------
    def show(self):
        self._auto_fit()
        self._rebuild()

        def draw():
            self._renderer.render(self._scene, self._camera)

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
        self._camera.width = ex + pad * 2
        self._camera.height = ey + pad * 2
        self._camera.local.position = (
            (min(xs) + max(xs)) / 2,
            (min(ys) + max(ys)) / 2,
            10,
        )

    # ------------------------------------------------------------------
    def _rebuild(self):
        self._scene.clear()
        bg = (0.06, 0.06, 0.06, 1) if self._dark_mode else (0.94, 0.94, 0.94, 1)
        self._scene.add(gfx.Background(
            None, gfx.BackgroundMaterial(bg, bg, bg, bg)))

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
        self._draw_legend()
        self._built = True

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

        # ring
        self._scene.add(gfx.Points(
            gfx.Geometry(positions=np.float32([(x, y, 0.003)])),
            gfx.PointsMaterial(size=ring_sz, size_space="screen", color=fg),
        ))
        # centre
        if is_xfer:
            c = _rgba(slines[0].color)  # simplified
        else:
            c = _rgba(slines[0].color)
        self._scene.add(gfx.Points(
            gfx.Geometry(positions=np.float32([(x, y, 0.004)])),
            gfx.PointsMaterial(size=size, size_space="screen", color=c),
        ))

    # ------------------------------------------------------------------
    def _draw_labels(self):
        font_sz = max(self._camera.width / 75, 5)
        for ln in self._lines:
            if ln.hide_terminal_label or ln.id in self._hidden_lines:
                continue
            pts = self._spline_data[ln.id]
            if len(pts) < 2:
                continue
            label = line_identifier(ln.id, ln.name)
            for st, is_first in [(ln.route[0], True), (ln.route[-1], False)]:
                sx, sy = st.position[0], st.position[1]
                bi = int(np.argmin(np.sum((pts[:, :2] - (sx, sy))**2, axis=1)))
                if bi <= 0:
                    dx, dy = float(pts[1, 0] - pts[0, 0]), float(pts[1, 1] - pts[0, 1])
                else:
                    dx = float(pts[-1, 0] - pts[-2, 0])
                    dy = float(pts[-1, 1] - pts[-2, 1])
                mag = math.sqrt(dx*dx + dy*dy) or 1e-10
                nx, ny = -dy/mag, dx/mag
                sign = -1 if is_first else 1
                offset = font_sz * 1.8 * sign
                lx, ly = sx + nx * offset, sy + ny * offset
                fg_c = "#fff" if self._dark_mode else "#111"
                t = gfx.Text(text=label, font_size=font_sz,
                              screen_space=False, anchor="middle-center",
                              material=gfx.TextMaterial(color=fg_c))
                t.local.position = (lx, ly, 0.006)
                self._scene.add(t)

    # ------------------------------------------------------------------
    def _draw_legend(self):
        fg = "#ddd" if self._dark_mode else "#222"
        y0 = self._canvas.get_logical_size()[1] - 40
        for i, ln in enumerate(self._lines):
            y = y0 - i * 32
            hidden = ln.id in self._hidden_lines
            c = _rgba(ln.color)
            if hidden:
                c = (c[0]*0.3, c[1]*0.3, c[2]*0.3, 0.5)
            label = line_identifier(ln.id, ln.name)
            lc = "#555" if hidden else fg
            # swatch
            self._scene.add(gfx.Points(
                gfx.Geometry(positions=np.float32([(30, y, 0)])),
                gfx.PointsMaterial(size=14, color=c),
            ))
            # text
            t = gfx.Text(text=label, font_size=14, screen_space=True,
                          anchor="middle-left",
                          material=gfx.TextMaterial(color=lc))
            t.local.position = (42, y, 0)
            self._scene.add(t)

    # ------------------------------------------------------------------
    # Input
    # ------------------------------------------------------------------
    def _on_key(self, event):
        k = event.get("key", "")
        if k in ("b", "B"):
            self._dark_mode = not self._dark_mode
            self._rebuild()
            self._canvas.request_draw(self._render_frame)
        elif k == "[":
            self._scale_factor = max(0.3, self._scale_factor - 0.1)
            self._rebuild()
            self._canvas.request_draw(self._render_frame)
        elif k == "]":
            self._scale_factor = min(3.0, self._scale_factor + 0.1)
            self._rebuild()
            self._canvas.request_draw(self._render_frame)

    def _render_frame(self):
        self._renderer.render(self._scene, self._camera)

    def _screen_to_world(self, sx, sy):
        cw, ch = self._camera.width, self._camera.height
        cx, cy, _ = self._camera.local.position
        lw, lh = self._canvas.get_logical_size()
        lw, lh = lw or 1280, lh or 900
        return cx + (sx / lw - 0.5) * cw, cy + (sy / lh - 0.5) * ch

    def _hit_test(self, wx, wy):
        threshold = self._camera.width / 30
        # stations
        for st_id, slines in self._station_lines.items():
            st = slines[0].route[0]
            for ln in self._lines:
                for s in ln.route:
                    if s.id == st_id:
                        st = s
                        break
            if math.hypot(wx - st.position[0], wy - st.position[1]) < threshold:
                return {"type": "station", "station": st, "lines": slines}
        # lines
        for ln in self._lines:
            if ln.id in self._hidden_lines:
                continue
            pts = self._spline_data[ln.id]
            for i in range(len(pts) - 1):
                if _point_seg_dist(wx, wy, float(pts[i, 0]), float(pts[i, 1]),
                                   float(pts[i+1, 0]), float(pts[i+1, 1])) < threshold:
                    return {"type": "line", "line": ln}
        return None

    def _on_ptr_move(self, event):
        self._mouse_screen = (event["x"], event["y"])
        wx, wy = self._screen_to_world(event["x"], event["y"])
        hit = self._hit_test(wx, wy)
        prev = self._hovered
        self._hovered = hit
        if prev != hit:
            self._rebuild()
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
                self._rebuild()
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
