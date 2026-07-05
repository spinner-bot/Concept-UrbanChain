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
from lang import t, toggle_language

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
        self._page_stack = []          # detail page navigation stack
        self._label_positions = []     # (screen_x, screen_y, line, station)

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
        self._built = True

    def _rebuild_ui(self):
        self._ui_scene.clear()
        if self._page_stack:
            self._draw_detail_page()
        else:
            self._draw_legend()
            self._draw_tooltip()

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
        font_sz = 14
        self._label_positions = []
        for ln in self._lines:
            if ln.hide_terminal_label or ln.id in self._hidden_lines:
                continue
            pts = self._spline_data[ln.id]
            if len(pts) < 2:
                continue
            label = line_identifier(ln.id, ln.name)
            for st, is_first in [(ln.route[0], True), (ln.route[-1], False)]:
                sx, sy = st.position[0], st.position[1]
                spx, spy = self._world_to_screen(sx, sy)
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
                txt = gfx.Text(text=label, font_size=font_sz,
                              screen_space=True, anchor="middle-center",
                              material=gfx.TextMaterial(color=fg_c))
                txt.local.position = (spx, spy, 0)
                self._scene.add(txt)
                # Store for click detection
                tw = len(label) * font_sz * 0.55
                self._label_positions.append((spx - tw/2, spy - font_sz,
                                              spx + tw/2, spy + font_sz, ln))

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
        self._legend_items = []     # (x1, y1, x2, y2, line)
        self._legend_btns = []      # (x1, y1, x2, y2, action)

        if self._legend_mode == "full_hide":
            # Just a small expand icon
            txt = gfx.Text(text=t("expand"), font_size=19, screen_space=True,
                           anchor="middle-left",
                           material=gfx.TextMaterial(color="#888"))
            txt.local.position = (30, y0, 0)
            self._ui_scene.add(txt)
            self._legend_btns.append((25, y0 - 12, 60, y0 + 12, "expand"))
            return

        for i, ln in enumerate(self._lines):
            y = y0 - i * 38
            hidden = ln.id in self._hidden_lines
            c = _rgba(ln.color)
            if hidden:
                c = (c[0]*0.3, c[1]*0.3, c[2]*0.3, 0.5)
            label = line_identifier(ln.id, ln.name)
            lc = "#555" if hidden else fg
            geo = gfx.Geometry(
                positions=np.float32([(30, y, 0), (50, y, 0),
                                      (50, y + 14, 0), (30, y + 14, 0)]),
                indices=np.int32([[0, 1, 2], [0, 2, 3]]),
            )
            self._ui_scene.add(gfx.Mesh(geo, gfx.MeshBasicMaterial(color=c)))
            txt = gfx.Text(text=label, font_size=17, screen_space=True,
                          anchor="middle-left",
                          material=gfx.TextMaterial(color=lc))
            txt.local.position = (58, y + 7, 0)
            self._ui_scene.add(txt)
            self._legend_items.append((25, y - 4, 200, y + 20, ln))

        # Bottom buttons
        ny = y0 - len(self._lines) * 38 - 10
        # Fold button
        fold_txt = t("full_fold") if self._legend_mode == "full_show" else t("fold")
        btn = gfx.Text(text=fold_txt, font_size=15, screen_space=True,
                       anchor="middle-left",
                       material=gfx.TextMaterial(color="#888"))
        btn.local.position = (30, ny, 0)
        self._ui_scene.add(btn)
        self._legend_btns.append((25, ny - 8, 55, ny + 10, "fold"))
        # Expand/Net buttons
        if self._legend_mode == "partial":
            t2 = gfx.Text(text=t("expand"), font_size=15, screen_space=True,
                           anchor="middle-left",
                           material=gfx.TextMaterial(color="#888"))
            t2.local.position = (62, ny, 0)
            self._ui_scene.add(t2)
            self._legend_btns.append((60, ny - 8, 85, ny + 10, "full_show"))
        tn = gfx.Text(text=t("net_btn"), font_size=15, screen_space=True,
                       anchor="middle-left",
                       material=gfx.TextMaterial(color="#888"))
        tn.local.position = (100, ny, 0)
        self._ui_scene.add(tn)
        self._legend_btns.append((95, ny - 8, 145, ny + 10, "network"))

    # ------------------------------------------------------------------
    # Tooltip
    # ------------------------------------------------------------------
    def _draw_tooltip(self):
        if not self._hovered:
            return
        mx, my_raw = self._mouse_screen
        lh = self._canvas.get_logical_size()[1] or 900
        my = lh - my_raw  # flip: mouse Y is bottom-origin, UI is top-origin
        fg = "#ddd" if self._dark_mode else "#111"
        bg = (0.15, 0.15, 0.15, 0.85) if self._dark_mode else (0.96, 0.96, 0.96, 0.88)

        h = self._hovered
        if h["type"] == "station":
            st = h["station"]
            lines = " | ".join(line_identifier(l.id, l.name) for l in h["lines"])
            text = f"{st.name}\n{lines}"
        elif h["type"] == "line":
            ln = h["line"]
            text = line_identifier(ln.id, ln.name)
        else:
            return

        pad = 10; font_sz = 15; line_h = font_sz * 1.5
        lines = text.split("\n")
        max_w = max(len(l) * font_sz * 0.6 for l in lines) + pad * 2
        box_h = len(lines) * line_h + pad * 2
        bx = mx + 14; by = my - box_h - 6

        geo = gfx.Geometry(
            positions=np.float32([(bx,by,0),(bx+max_w,by,0),(bx+max_w,by+box_h,0),(bx,by+box_h,0)]),
            indices=np.int32([[0,1,2],[0,2,3]]))
        self._ui_scene.add(gfx.Mesh(geo, gfx.MeshBasicMaterial(color=bg)))
        for i, l in enumerate(lines):
            txt = gfx.Text(text=l, font_size=font_sz, screen_space=True,
                          anchor="top-left", material=gfx.TextMaterial(color=fg))
            txt.local.position = (bx+pad, by+box_h-pad-i*line_h, 0)
            self._ui_scene.add(txt)

    # ------------------------------------------------------------------
    # Detail pages
    # ------------------------------------------------------------------
    def _detail_hotspots(self):
        """Return list of clickable zones in current detail page."""
        hs = []
        page = self._page_stack[-1] if self._page_stack else None
        if not page:
            return hs
        pg = page["type"]
        lw, lh = self._canvas.get_logical_size()
        lw, lh = lw or 1280, lh or 900
        y = lh - 90
        if pg == "station":
            y -= 85  # skip header + info
            for ln in page.get("lines", []):
                hs.append({"x": 50, "y": y, "w": 300, "h": 26,
                           "action": "push",
                           "page": {"type": "line", "line": ln}})
                y -= 26
        elif pg == "line":
            ln = page["line"]
            y -= 85
            y -= 40  # skip route header
            for s in ln.route:
                hs.append({"x": 50, "y": y, "w": 300, "h": 24,
                           "action": "push",
                           "page": {"type": "station", "station": s,
                                    "lines": self._station_lines.get(s.id, [ln])}})
                y -= 24
        elif pg == "network":
            y -= 50
            for ln in self._lines:
                hs.append({"x": 40, "y": y, "w": 600, "h": 26,
                           "action": "push",
                           "page": {"type": "line", "line": ln}})
                y -= 26
        return hs
    def _draw_detail_page(self):
        page = self._page_stack[-1]
        pg = page["type"]
        fg = "#ddd" if self._dark_mode else "#222"
        bg = (0.10, 0.10, 0.12, 0.94) if self._dark_mode else (0.94, 0.94, 0.96, 0.94)
        lw, lh = self._canvas.get_logical_size()
        lw, lh = lw or 1280, lh or 900
        # Full-screen bg
        geo = gfx.Geometry(
            positions=np.float32([(0,0,0),(lw,0,0),(lw,lh,0),(0,lh,0)]),
            indices=np.int32([[0,1,2],[0,2,3]]))
        self._ui_scene.add(gfx.Mesh(geo, gfx.MeshBasicMaterial(color=bg)))
        if pg == "station":
            self._draw_station_page(page, fg, lw, lh)
        elif pg == "line":
            self._draw_line_page(page, fg, lw, lh)
        elif pg == "network":
            self._draw_network_page(fg, lw, lh)
        # Nav buttons
        self._detail_btns(fg, lw, lh)

    def _draw_station_page(self, page, fg, lw, lh):
        st = page["station"]
        slines = page.get("lines", [])
        y = lh - 40
        # Title
        txt = gfx.Text(text=f"[{st.id:04d}] {st.name}", font_size=30,
                      screen_space=True, anchor="top-left",
                      material=gfx.TextMaterial(color=fg))
        txt.local.position = (30, y, 0); self._ui_scene.add(txt)
        y -= 40
        for item in [
            f"Type: {st.station_type.display_name}",
            f"Position: ({st.position[0]:.1f}, {st.position[1]:.1f}, {st.position[2]:.1f})",
            t("lines_label") + f": {len(slines)}",
        ]:
            txt = gfx.Text(text=item, font_size=19, screen_space=True,
                          anchor="top-left", material=gfx.TextMaterial(color=fg))
            txt.local.position = (40, y, 0); self._ui_scene.add(txt); y -= 28
        y -= 10
        for ln in slines:
            lid = line_identifier(ln.id, ln.name)
            txt = gfx.Text(text=f"  > {lid}", font_size=18, screen_space=True,
                          anchor="top-left",
                          material=gfx.TextMaterial(color="#6af"))
            txt.local.position = (50, y, 0); self._ui_scene.add(txt); y -= 26

    def _draw_line_page(self, page, fg, lw, lh):
        ln = page["line"]
        from spline import line_length
        y = lh - 40
        txt = gfx.Text(text=line_identifier(ln.id, ln.name), font_size=30,
                      screen_space=True, anchor="top-left",
                      material=gfx.TextMaterial(color=fg))
        txt.local.position = (30, y, 0); self._ui_scene.add(txt)
        y -= 40
        length = line_length(ln.route, ln.fine_trajectory)
        is_circ = len(ln.route) >= 2 and ln.route[0].id == ln.route[-1].id
        for item in [
            f"Stations: {len(ln.route)}{t("circular_label") if is_circ else ''}",
            f"Length: {length:.2f} units",
            f"Max speed: {ln.max_speed} km/h",
        ]:
            txt = gfx.Text(text=item, font_size=19, screen_space=True,
                          anchor="top-left", material=gfx.TextMaterial(color=fg))
            txt.local.position = (40, y, 0); self._ui_scene.add(txt); y -= 28
        y -= 10
        txt = gfx.Text(text=t("route_label") + ":", font_size=19, screen_space=True,
                      anchor="top-left", material=gfx.TextMaterial(color=fg))
        txt.local.position = (40, y, 0); self._ui_scene.add(txt); y -= 22
        for s in ln.route:
            xfer = f" [T:{len(self._station_lines.get(s.id, []))}]" if len(self._station_lines.get(s.id, [])) >= 2 else ""
            txt = gfx.Text(text=f"  > {s.name}{xfer}", font_size=18,
                          screen_space=True, anchor="top-left",
                          material=gfx.TextMaterial(color="#6af"))
            txt.local.position = (50, y, 0); self._ui_scene.add(txt); y -= 24

    def _draw_network_page(self, fg, lw, lh):
        from spline import line_length
        y = lh - 40
        txt = gfx.Text(text=t("line_network_title"), font_size=30, screen_space=True,
                      anchor="top-left", material=gfx.TextMaterial(color=fg))
        txt.local.position = (30, y, 0); self._ui_scene.add(txt)
        y -= 40
        for ln in self._lines:
            is_circ = len(ln.route) >= 2 and ln.route[0].id == ln.route[-1].id
            length = line_length(ln.route, ln.fine_trajectory)
            mid = f"(circ) term: {ln.route[0].name}" if is_circ else f"{ln.route[0].name} - {ln.route[-1].name}"
            label = f"{line_identifier(ln.id, ln.name)}  {mid}  [{len(ln.route)} stn | {length:.1f} u]"
            txt = gfx.Text(text=label, font_size=18, screen_space=True,
                          anchor="top-left", material=gfx.TextMaterial(color="#6af"))
            txt.local.position = (40, y, 0); self._ui_scene.add(txt); y -= 26

    def _detail_btns(self, fg, lw, lh):
        """Back and close buttons."""
        btn_back = gfx.Text(text=t("back"), font_size=19, screen_space=True,
                      anchor="top-left", material=gfx.TextMaterial(color="#6af"))
        btn_back.local.position = (30, lh - 10, 0); self._ui_scene.add(btn_back)
        btn_close = gfx.Text(text=t("close"), font_size=19, screen_space=True,
                      anchor="top-right", material=gfx.TextMaterial(color="#f66"))
        btn_close.local.position = (lw - 30, lh - 10, 0); self._ui_scene.add(btn_close)

    # ------------------------------------------------------------------
    # Input
    # ------------------------------------------------------------------

    def _on_key(self, event):
        k = event.get("key", "")
        if k in ("b", "B"):
            self._dark_mode = not self._dark_mode
            self._rebuild_map()
            self._rebuild_ui()
            self._canvas.request_draw(self._render_frame)
        elif k in ("l", "L"):
            toggle_language()
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

    def _render_frame(self):
        lw, lh = self._canvas.get_logical_size()
        if lw and lh:
            self._ui_camera.width = lw
            self._ui_camera.height = lh
            self._ui_camera.local.position = (lw / 2, lh / 2, 10)
        self._renderer.render(self._scene, self._camera, flush=False)
        self._renderer.render(self._ui_scene, self._ui_camera, flush=True)

    def _screen_to_world(self, sx, sy):
        cw, ch = self._camera.width, self._camera.height
        cx, cy, _ = self._camera.local.position
        lw, lh = self._canvas.get_logical_size()
        lw, lh = lw or 1280, lh or 900
        # Mouse Y is from top; camera Y is from bottom → flip
        return cx + (sx / lw - 0.5) * cw, cy + (0.5 - sy / lh) * ch

    def _world_to_screen(self, wx, wy):
        cw, ch = self._camera.width, self._camera.height
        cx, cy, _ = self._camera.local.position
        lw, lh = self._canvas.get_logical_size()
        lw, lh = lw or 1280, lh or 900
        # Camera Y from bottom; screen Y from top → flip
        return ((wx - cx) / cw * lw + lw / 2,
                lh - ((wy - cy) / ch * lh + lh / 2))

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
        changed = (prev != hit)
        if changed:
            self._rebuild_map()
            self._rebuild_ui()
            self._canvas.request_draw(self._render_frame)
        elif hit is not None:
            # Same object, different position — update tooltip
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
            return

        sx = event["x"]
        sy_top = event["y"]  # mouse: 0=top, lh=bottom
        lw, lh = self._canvas.get_logical_size()
        lw, lh = lw or 1280, lh or 900
        sy_btm = lh - sy_top  # bottom-origin for UI camera

        # --- Detail page ---
        if self._page_stack:
            if sx < 80 and sy_top < 30:
                self._page_stack.pop()
                self._rebuild_ui()
                self._canvas.request_draw(self._render_frame)
                return
            if sx > lw - 80 and sy_top < 30:
                self._page_stack.clear()
                self._rebuild_ui()
                self._canvas.request_draw(self._render_frame)
                return
            for hs in self._detail_hotspots():
                if (hs["x"] <= sx <= hs["x"] + hs["w"]
                        and hs["y"] <= sy_top <= hs["y"] + hs["h"]):
                    if hs["action"] == "push":
                        self._page_stack.append(hs["page"])
                        self._rebuild_ui()
                        self._canvas.request_draw(self._render_frame)
                        return
            return

        # --- Legend buttons (bottom-origin) ---
        for x1, y1, x2, y2, action in getattr(self, "_legend_btns", []):
            if x1 <= sx <= x2 and y1 <= sy_btm <= y2:
                if action == "expand":      self._legend_mode = "partial"
                elif action == "fold":      self._legend_mode = "full_hide"
                elif action == "full_show": self._legend_mode = "full_show"
                elif action == "network":   self._page_stack.append({"type": "network"})
                self._rebuild_ui()
                self._canvas.request_draw(self._render_frame)
                return

        # --- Legend lines ---
        for x1, y1, x2, y2, ln in getattr(self, "_legend_items", []):
            if x1 <= sx <= x2 and y1 <= sy_btm <= y2:
                if ln.id in self._hidden_lines:
                    self._hidden_lines.discard(ln.id)
                else:
                    self._hidden_lines.add(ln.id)
                self._rebuild_map()
                self._rebuild_ui()
                self._canvas.request_draw(self._render_frame)
                return

        # --- Label click ---
        for lx1, ly1, lx2, ly2, ln in self._label_positions:
            if lx1 <= sx <= lx2 and ly1 <= sy_btm <= ly2:
                ln.hide_terminal_label = True
                self._rebuild_map()
                self._rebuild_ui()
                self._canvas.request_draw(self._render_frame)
                return

        # --- Map click ---
        wx, wy = self._screen_to_world(sx, sy_top)
        hit = self._hit_test(wx, wy)
        if hit:
            if hit["type"] == "station":
                self._page_stack.append({
                    "type": "station", "station": hit["station"],
                    "lines": hit["lines"],
                })
            elif hit["type"] == "line":
                self._page_stack.append({
                    "type": "line", "line": hit["line"],
                })
            self._rebuild_ui()
            self._canvas.request_draw(self._render_frame)
