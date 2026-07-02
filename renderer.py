"""Interactive metro map renderer — GPU-accelerated via pygfx / wgpu."""

import math
from collections import defaultdict

import numpy as np
import pygfx as gfx
from rendercanvas.auto import RenderCanvas, loop

from spline import catmull_rom_spline, build_key_points, line_identifier


def _point_seg_dist(px: float, py: float,
                    x1: float, y1: float,
                    x2: float, y2: float) -> float:
    """Shortest distance from point (px,py) to segment (x1,y1)-(x2,y2)."""
    dx, dy = x2 - x1, y2 - y1
    if dx == 0 and dy == 0:
        return math.hypot(px - x1, py - y1)
    t = max(0, min(1, ((px - x1) * dx + (py - y1) * dy) / (dx * dx + dy * dy)))
    return math.hypot(px - (x1 + t * dx), py - (y1 + t * dy))


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


def _blend_colours(deg: float, angles: list, colours: list) -> tuple:
    """Softmax colour blend at a given angle (degrees)."""
    if len(colours) == 1:
        return colours[0]
    weights = []
    for a in angles:
        diff = abs(deg - a)
        if diff > 180:
            diff = 360 - diff
        w = math.exp(-0.5 * (diff / 25.0) ** 2)
        weights.append(w)
    total = sum(weights) or 1e-10
    r = sum(colours[i][0] * weights[i] / total for i in range(len(colours)))
    g = sum(colours[i][1] * weights[i] / total for i in range(len(colours)))
    b = sum(colours[i][2] * weights[i] / total for i in range(len(colours)))
    return (int(r), int(g), int(b))


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
        self._spline_original: dict[int, np.ndarray] = {}
        self._spline_data: dict[int, np.ndarray] = {}
        for ln in lines:
            keys = build_key_points(ln.route, ln.fine_trajectory)
            pts = catmull_rom_spline(keys, 30, ln.smooth_tension)
            arr = np.array([(x, y, 0.0) for x, y in pts], dtype=np.float32)
            self._spline_original[ln.id] = arr.copy()
            self._spline_data[ln.id] = arr

        # (collinear offsets applied later in _build_scene after camera setup)

        # State
        self._dark_mode = False
        self._scale_factor = 1.0   # secondary zoom: 0.3–3.0
        self._offsets_applied = False
        self._page_stack: list[dict] = []  # navigation stack for detail pages
        self._detail_hotspots: list[dict] = []  # clickable zones in detail pages
        self._hidden_lines: set[int] = set()    # ids of lines hidden via legend
        self._legend_mode = "partial"  # "partial" | "full_show" | "full_hide"

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

        # ---- Viewport (must exist before controller) ----
        self._viewport = gfx.Viewport(self._renderer)

        # ---- Controller (pan + zoom) ----
        self._controller = gfx.PanZoomController(self._camera)
        self._controller.register_events(self._viewport)

        # ---- Keyboard ----
        self._canvas.add_event_handler(self._on_key, "key_down")

        # ---- Pointer ----
        self._hovered = None          # currently hovered object
        self._mouse_screen = (0, 0)   # last mouse screen position
        self._canvas.add_event_handler(self._on_pointer_move, "pointer_move")
        self._canvas.add_event_handler(self._on_click, "click")

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
    # Collinear offset at transfer stations
    # ------------------------------------------------------------------
    def _apply_collinear_offsets(self) -> None:
        """Offset splines of collinear lines at transfer stations.

        When multiple lines approach a station from similar directions,
        their spline points near the station are shifted perpendicular
        to the approach so they appear bundled instead of overlapping.
        """
        # Reset to original spline data
        for lid, orig in self._spline_original.items():
            self._spline_data[lid] = orig.copy()

        angle_threshold = 22.0  # degrees — lines within this are "collinear"
        sigma = 3.0             # world units — decay distance of the offset

        for st_id, slines in self._station_lines.items():
            if len(slines) < 2:
                continue

            # Find the station object
            st_obj = next((s for ln in self._lines for s in ln.route
                           if s.id == st_id), None)
            if st_obj is None:
                continue
            sx, sy = st_obj.position[0], st_obj.position[1]

            # Compute approach angles
            line_angles = []
            for ln in slines:
                pts = self._spline_data[ln.id]
                best_i = int(np.argmin(np.sum((pts[:, :2] - (sx, sy)) ** 2,
                                              axis=1)))
                if best_i <= 0:
                    dx = float(pts[1, 0] - pts[0, 0])
                    dy = float(pts[1, 1] - pts[0, 1])
                else:
                    dx = float(pts[best_i, 0] - pts[best_i - 1, 0])
                    dy = float(pts[best_i, 1] - pts[best_i - 1, 1])
                ang = math.degrees(math.atan2(dy, dx)) % 360
                line_angles.append((ang, ln))

            # Group by angular proximity
            line_angles.sort(key=lambda x: x[0])
            groups: list[list] = []
            used = [False] * len(line_angles)
            for i in range(len(line_angles)):
                if used[i]:
                    continue
                group = [line_angles[i]]
                used[i] = True
                for j in range(i + 1, len(line_angles)):
                    if used[j]:
                        continue
                    diff = abs(line_angles[j][0] - line_angles[i][0])
                    if diff > 180:
                        diff = 360 - diff
                    if diff < angle_threshold:
                        group.append(line_angles[j])
                        used[j] = True
                groups.append(group)

            # Apply offset for each group with ≥ 2 lines
            for group in groups:
                if len(group) < 2:
                    continue
                # Perpendicular direction
                mid_angle = sum(a for a, _ in group) / len(group)
                rad = math.radians(mid_angle)
                px, py = -math.sin(rad), math.cos(rad)  # perpendicular

                # Spread lines evenly
                step = self._line_thickness() * 1.5
                canvas_w = self._canvas.get_logical_size()[0] or 1280
                pu = canvas_w / max(self._camera.width, 0.01)
                offset_step_world = step / pu  # world units

                for k, (ang, ln) in enumerate(group):
                    offset_amt = (k - (len(group) - 1) / 2) * offset_step_world
                    pts = self._spline_data[ln.id]
                    # Apply Gaussian-weighted offset near the station
                    for j in range(len(pts)):
                        dx_w = pts[j, 0] - sx
                        dy_w = pts[j, 1] - sy
                        dist = math.sqrt(dx_w * dx_w + dy_w * dy_w)
                        weight = math.exp(-0.5 * (dist / sigma) ** 2)
                        pts[j, 0] += px * offset_amt * weight
                        pts[j, 1] += py * offset_amt * weight
                    self._spline_data[ln.id] = pts

    # ------------------------------------------------------------------
    # Helpers
    # ------------------------------------------------------------------
    def _visible_lines(self):
        """Yield lines that are not hidden."""
        for ln in self._lines:
            if ln.id not in self._hidden_lines:
                yield ln

    # ------------------------------------------------------------------
    # Scaled sizes (secondary zoom)
    # ------------------------------------------------------------------
    def _line_thickness(self) -> float:
        return LINE_THICKNESS * self._scale_factor

    def _station_size(self) -> float:
        return STATION_SIZE * self._scale_factor

    def _transfer_size(self) -> float:
        return TRANSFER_SIZE * self._scale_factor

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
        if not self._offsets_applied:
            self._apply_collinear_offsets()
            self._offsets_applied = True
        self._scene.clear()

        # Background
        bg = (0.06, 0.06, 0.06, 1) if self._dark_mode else (0.94, 0.94, 0.94, 1)
        self._scene.add(
            gfx.Background(None, gfx.BackgroundMaterial(bg, bg, bg, bg)),
        )

        # Lines (bottom layer)
        for ln in self._visible_lines():
            self._add_line(ln)

        # Stations (top layer — show if at least one line is visible)
        drawn: set[int] = set()
        for ln in self._lines:
            for st in ln.route:
                if st.id in drawn:
                    continue
                drawn.add(st.id)
                visible_lines = [l for l in self._station_lines[st.id]
                                 if l.id not in self._hidden_lines]
                if not visible_lines:
                    continue
                self._add_station(st, visible_lines)

        # Terminal labels
        for ln in self._visible_lines():
            self._add_terminal_label(ln)

    # ------------------------------------------------------------------
    # Line
    # ------------------------------------------------------------------
    def _add_line(self, ln) -> None:
        pts = self._spline_data[ln.id]
        if len(pts) < 2:
            return
        hovered = (self._hovered and self._hovered.get("type") == "line"
                   and self._hovered.get("line", None) is ln)
        thickness = self._line_thickness() * 1.5 if hovered else self._line_thickness()
        self._scene.add(gfx.Line(
            gfx.Geometry(positions=pts),
            gfx.LineMaterial(
                thickness=thickness,
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
        size = self._transfer_size() if is_transfer else self._station_size()

        # Outer ring (fg colour = black or white depending on mode)
        fg = (1, 1, 1, 1) if self._dark_mode else (0, 0, 0, 1)
        # Highlight if hovered
        hovered = (self._hovered and self._hovered.get("type") == "station"
                   and self._hovered.get("station", None) is st)
        ring_size = (size + 8 * self._scale_factor) if hovered else (size + 5 * self._scale_factor)
        ring = gfx.Points(
            gfx.Geometry(positions=np.float32([(x, y, 0.002)])),
            gfx.PointsMaterial(size=ring_size, size_space="screen", color=fg),
        )
        self._scene.add(ring)

        # Centre
        if is_transfer and len(slines) >= 2:
            self._add_transfer_gradient(x, y, slines)
        else:
            centre = gfx.Points(
                gfx.Geometry(positions=np.float32([(x, y, 0.003)])),
                gfx.PointsMaterial(size=size, size_space="screen",
                                   color=_rgba(slines[0].color)),
            )
            self._scene.add(centre)

    def _add_transfer_gradient(self, x, y, slines: list) -> None:
        """Draw a gradient circle for a transfer station using vertex colours."""
        # Approach angles for each line
        angles = []
        colours = []
        for ln in slines:
            pts = self._spline_data[ln.id]
            best_i = int(np.argmin(np.sum((pts[:, :2] - (x, y)) ** 2, axis=1)))
            if best_i == 0:
                dx, dy = float(pts[1, 0] - pts[0, 0]), float(pts[1, 1] - pts[0, 1])
            elif best_i == len(pts) - 1:
                dx, dy = float(pts[-1, 0] - pts[-2, 0]), float(pts[-1, 1] - pts[-2, 1])
            else:
                dx = float(pts[best_i + 1, 0] - pts[best_i - 1, 0])
                dy = float(pts[best_i + 1, 1] - pts[best_i - 1, 1])
            ang = math.degrees(math.atan2(dy, dx)) % 360
            angles.append(ang)
            colours.append(ln.color)

        # Sort by angle
        paired = sorted(zip(angles, colours), key=lambda p: p[0])
        angles = [p[0] for p in paired]
        colours = [p[1] for p in paired]

        # Build fan geometry — radius in world units matching screen px
        canvas_w = self._canvas.get_logical_size()[0] or 1280
        px_per_unit = canvas_w / self._camera.width if self._camera.width > 0 else 70
        r = (self._transfer_size() / 2) / px_per_unit
        n_seg = 64
        verts = [(x, y, 0.003)]
        vert_colors = [_rgba(_blend_colours(0, angles, colours))]
        for i in range(n_seg):
            theta = 2 * math.pi * i / n_seg
            deg = math.degrees(theta) % 360
            px = x + math.cos(theta) * r
            py = y + math.sin(theta) * r
            verts.append((px, py, 0.003))
            vert_colors.append(_rgba(_blend_colours(deg, angles, colours)))

        indices = []
        for i in range(1, n_seg):
            indices.append([0, i, i + 1])
        indices.append([0, n_seg, 1])

        geo = gfx.Geometry(
            positions=np.array(verts, dtype=np.float32),
            indices=np.array(indices, dtype=np.int32).reshape(-1, 3),
            colors=np.array(vert_colors, dtype=np.float32),
        )
        mat = gfx.MeshBasicMaterial(color_mode="vertex")
        self._scene.add(gfx.Mesh(geo, mat))

    # ------------------------------------------------------------------
    # UI overlay
    # ------------------------------------------------------------------
    def _build_ui(self) -> None:
        self._ui_scene.clear()
        if self._page_stack:
            self._draw_detail_page()
        else:
            self._draw_legend()
            self._draw_tooltip()

    def _draw_legend(self) -> None:
        fg = "#ddd" if self._dark_mode else "#222"
        dim = "#555" if self._dark_mode else "#999"
        icon_colour = "#888" if self._dark_mode else "#666"
        y_base = self._ui_camera.height - 40
        self._legend_hotspots = []
        self._legend_icon_spots = []

        if self._legend_mode == "full_hide":
            # Only show a small collapsed icon
            t = gfx.Text(text="☰", font_size=18, screen_space=True,
                          anchor="middle-left",
                          material=gfx.TextMaterial(color=icon_colour))
            t.local.position = (30, y_base, 0)
            self._ui_scene.add(t)
            self._legend_icon_spots.append({
                "x": 30, "y": y_base - 12, "w": 30, "h": 24,
                "action": "expand",
            })
            return

        # Normal legend (partial or full_show)
        for i, ln in enumerate(self._lines):
            y = y_base - i * 32
            hidden = ln.id in self._hidden_lines

            # Swatch
            color = _rgba(ln.color)
            if hidden:
                color = (color[0]*0.3, color[1]*0.3, color[2]*0.3, 0.5)
            geo = gfx.Geometry(
                positions=np.float32([(30, y, 0), (55, y, 0),
                                      (55, y + 16, 0), (30, y + 16, 0)]),
                indices=np.int32([[0, 1, 2], [0, 2, 3]]),
            )
            mat = gfx.MeshBasicMaterial(color=color)
            self._ui_scene.add(gfx.Mesh(geo, mat))

            # Label
            label = line_identifier(ln.id, ln.name)
            label_fg = dim if hidden else fg
            text = gfx.Text(
                text=label,
                font_size=14,
                screen_space=True,
                anchor="middle-left",
                material=gfx.TextMaterial(color=label_fg),
            )
            text.local.position = (62, y + 8, 0)
            self._ui_scene.add(text)

            # Record hotspot
            self._legend_hotspots.append({
                "x": 30, "y": y, "w": 200, "h": 18,
                "line_id": ln.id,
            })

        # Fold icon (bottom of legend)
        icon_y = y_base - len(self._lines) * 32 - 5
        fold_text = "—" if self._legend_mode == "full_show" else "◧"
        t = gfx.Text(text=fold_text, font_size=14, screen_space=True,
                      anchor="middle-left",
                      material=gfx.TextMaterial(color=icon_colour))
        t.local.position = (30, icon_y, 0)
        self._ui_scene.add(t)
        self._legend_icon_spots.append({
            "x": 30, "y": icon_y - 10, "w": 30, "h": 20,
            "action": "fold",
        })
        # Expand icon (only in partial mode)
        if self._legend_mode == "partial":
            t2 = gfx.Text(text="☰", font_size=14, screen_space=True,
                           anchor="middle-left",
                           material=gfx.TextMaterial(color=icon_colour))
            t2.local.position = (62, icon_y, 0)
            self._ui_scene.add(t2)
            self._legend_icon_spots.append({
                "x": 62, "y": icon_y - 10, "w": 30, "h": 20,
                "action": "full_show",
            })

    # ------------------------------------------------------------------
    # Detail pages
    # ------------------------------------------------------------------
    def _draw_detail_page(self) -> None:
        self._detail_hotspots.clear()
        page = self._page_stack[-1]
        pg_type = page["type"]
        fg = "#ddd" if self._dark_mode else "#222"
        bg = (0.12, 0.12, 0.14, 0.95) if self._dark_mode else (0.94, 0.94, 0.96, 0.95)
        accent = (0.2, 0.5, 0.9, 1.0)  # blue accent for buttons

        w, h = self._canvas.get_logical_size()
        w, h = w or 1280, h or 900

        # Full-screen background
        geo = gfx.Geometry(
            positions=np.float32([(0, 0, 0), (w, 0, 0), (w, h, 0), (0, h, 0)]),
            indices=np.int32([[0, 1, 2], [0, 2, 3]]),
        )
        self._ui_scene.add(gfx.Mesh(geo, gfx.MeshBasicMaterial(color=bg)))

        if pg_type == "station_detail":
            self._draw_station_detail(page, fg, accent, w, h)
        elif pg_type == "line_detail":
            self._draw_line_detail(page, fg, accent, w, h)

    def _draw_station_detail(self, page, fg, accent, w, h) -> None:
        st = page["station"]
        slines = page["lines"]
        font = 16
        small = 13

        # Title: [id] name
        title = f"[{st.id:04d}] {st.name}"
        t = gfx.Text(text=title, font_size=22, screen_space=True,
                      anchor="top-left", material=gfx.TextMaterial(color=fg))
        t.local.position = (30, h - 40, 0)
        self._ui_scene.add(t)

        # User-level data
        y = h - 80
        items = [
            f"Station type: {st.station_type.value}",
            f"Position: ({st.position[0]:.1f}, {st.position[1]:.1f}, {st.position[2]:.1f})",
            f"Lines passing: {len(slines)}",
        ]
        for item in items:
            t = gfx.Text(text=item, font_size=font, screen_space=True,
                          anchor="top-left", material=gfx.TextMaterial(color=fg))
            t.local.position = (40, y, 0)
            self._ui_scene.add(t)
            y -= 28

        # Line list (clickable)
        y -= 10
        for ln in slines:
            label = line_identifier(ln.id, ln.name)
            display = f"  ▸ {label}"
            t = gfx.Text(text=display, font_size=font, screen_space=True,
                          anchor="top-left",
                          material=gfx.TextMaterial(color="#6af"))
            t.local.position = (50, y, 0)
            self._ui_scene.add(t)
            # Record hotspot for click detection
            tw = len(display) * font * 0.6
            self._detail_hotspots.append({
                "x": 50, "y": y - 22, "w": tw, "h": 24,
                "action": "push_page",
                "page": {"type": "line_detail", "line": ln},
            })
            y -= 26

        # Back button (top-left)
        self._add_nav_buttons(fg, accent, w, h)

    def _draw_line_detail(self, page, fg, accent, w, h) -> None:
        ln = page["line"]
        font = 16

        # Title
        title = line_identifier(ln.id, ln.name)
        t = gfx.Text(text=title, font_size=22, screen_space=True,
                      anchor="top-left", material=gfx.TextMaterial(color=fg))
        t.local.position = (30, h - 40, 0)
        self._ui_scene.add(t)

        from spline import line_length
        length = line_length(ln.route, ln.fine_trajectory)
        is_circ = (len(ln.route) >= 2 and ln.route[0].id == ln.route[-1].id)
        circ_label = " (circular)" if is_circ else ""

        y = h - 80
        items = [
            f"Stations: {len(ln.route)}{circ_label}",
            f"Line length: {length:.2f} units",
            f"Max speed: {ln.max_speed} km/h",
        ]
        for item in items:
            t = gfx.Text(text=item, font_size=font, screen_space=True,
                          anchor="top-left", material=gfx.TextMaterial(color=fg))
            t.local.position = (40, y, 0)
            self._ui_scene.add(t)
            y -= 28

        # Route stations (clickable)
        y -= 10
        header = gfx.Text(text="Route stations:", font_size=font, screen_space=True,
                           anchor="top-left", material=gfx.TextMaterial(color=fg))
        header.local.position = (40, y, 0)
        self._ui_scene.add(header)
        y -= 26
        for s in ln.route:
            display = f"  ▸ {s.name}"
            t = gfx.Text(text=display, font_size=14, screen_space=True,
                          anchor="top-left", material=gfx.TextMaterial(color="#6af"))
            t.local.position = (50, y, 0)
            self._ui_scene.add(t)
            tw = len(display) * 14 * 0.6
            self._detail_hotspots.append({
                "x": 50, "y": y - 20, "w": tw, "h": 22,
                "action": "push_page",
                "page": {
                    "type": "station_detail",
                    "station": s,
                    "lines": self._station_lines.get(s.id, [ln]),
                },
            })
            y -= 24

        self._add_nav_buttons(fg, accent, w, h)

    def _add_nav_buttons(self, fg, accent, w, h) -> None:
        """Back (←) and close (✕) buttons."""
        # Back button
        t = gfx.Text(text="← Back", font_size=15, screen_space=True,
                      anchor="top-left", material=gfx.TextMaterial(color="#6af"))
        t.local.position = (30, h - 10, 0)
        self._ui_scene.add(t)
        # Close button
        t = gfx.Text(text="✕ Close", font_size=15, screen_space=True,
                      anchor="top-right", material=gfx.TextMaterial(color="#f66"))
        t.local.position = (w - 30, h - 10, 0)
        self._ui_scene.add(t)

    # ------------------------------------------------------------------
    # Tooltip
    # ------------------------------------------------------------------
    def _draw_tooltip(self) -> None:
        if not self._hovered:
            return
        mx, my = self._mouse_screen
        fg = "#eee" if self._dark_mode else "#111"
        bg = (0.15, 0.15, 0.15, 0.88) if self._dark_mode else (0.96, 0.96, 0.96, 0.92)

        if self._hovered["type"] == "station":
            st = self._hovered["station"]
            slines = self._hovered["lines"]
            title = f"{st.name}"
            lines_text = " | ".join(line_identifier(l.id, l.name) for l in slines)
            text = f"{title}\n{lines_text}"
        elif self._hovered["type"] == "line":
            ln = self._hovered["line"]
            text = line_identifier(ln.id, ln.name)
        else:
            return

        # Background rect
        lines = text.split("\n")
        font_size = 13
        line_h = font_size * 1.5
        pad = 8
        max_w = max(len(l) * font_size * 0.6 for l in lines) + pad * 2
        box_h = len(lines) * line_h + pad * 2
        bx = mx + 14
        by = my - box_h - 8

        geo = gfx.Geometry(
            positions=np.float32([(bx, by, 0), (bx + max_w, by, 0),
                                  (bx + max_w, by + box_h, 0), (bx, by + box_h, 0)]),
            indices=np.int32([[0, 1, 2], [0, 2, 3]]),
        )
        mat = gfx.MeshBasicMaterial(color=bg)
        self._ui_scene.add(gfx.Mesh(geo, mat))

        for i, line_text in enumerate(lines):
            t = gfx.Text(
                text=line_text,
                font_size=font_size,
                screen_space=True,
                anchor="top-left",
                material=gfx.TextMaterial(color=fg),
            )
            t.local.position = (bx + pad, by + box_h - pad - i * line_h, 0)
            self._ui_scene.add(t)

    # ------------------------------------------------------------------
    # Terminal labels
    # ------------------------------------------------------------------
    def _add_terminal_label(self, ln) -> None:
        if ln.hide_terminal_label:
            return
        pts = self._spline_data[ln.id]
        if len(pts) < 2:
            return
        text_str = line_identifier(ln.id, ln.name)
        font_size = self._camera.width / 75

        # Determine which stations get labels
        is_circular = (len(ln.route) >= 2
                       and ln.route[0].id == ln.route[-1].id)
        if is_circular:
            # Single label at ring_label_station_id (default: first station)
            label_st = ln.route[0]
            if ln.ring_label_station_id is not None:
                for s in ln.route:
                    if s.id == ln.ring_label_station_id:
                        label_st = s
                        break
            terminals = [(label_st, True)]
        else:
            terminals = [(ln.route[0], True), (ln.route[-1], False)]

        for st, is_start in terminals:
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
    # Pointer / hit-testing
    # ------------------------------------------------------------------
    def _screen_to_world(self, sx: float, sy: float) -> tuple[float, float]:
        """Convert screen pixel coords to world (data) coords."""
        cw = self._camera.width
        ch = self._camera.height
        cx, cy, _ = self._camera.local.position
        lw, lh = self._canvas.get_logical_size()
        lw = lw or 1280
        lh = lh or 900
        wx = cx + (sx / lw - 0.5) * cw
        wy = cy + (sy / lh - 0.5) * ch
        return wx, wy

    def _hit_test(self, wx: float, wy: float) -> dict | None:
        """Return the top-most hit object at world pos, or None."""
        threshold = 1.0  # world units
        # Check stations first (on top)
        for st_id, slines in self._station_lines.items():
            st = slines[0].route[0]
            # Find actual station
            for ln in self._lines:
                for s in ln.route:
                    if s.id == st_id:
                        st = s
                        break
            sx, sy = st.position[0], st.position[1]
            if math.hypot(wx - sx, wy - sy) < threshold:
                return {"type": "station", "station": st, "lines": slines}
        # Check lines
        for ln in self._lines:
            pts = self._spline_data[ln.id]
            if len(pts) < 2:
                continue
            # Simple: check distance to each segment
            for i in range(len(pts) - 1):
                x1, y1 = float(pts[i, 0]), float(pts[i, 1])
                x2, y2 = float(pts[i + 1, 0]), float(pts[i + 1, 1])
                if _point_seg_dist(wx, wy, x1, y1, x2, y2) < threshold:
                    return {"type": "line", "line": ln}
        return None

    def _on_pointer_move(self, event) -> None:
        self._mouse_screen = (event["x"], event["y"])
        wx, wy = self._screen_to_world(event["x"], event["y"])
        hit = self._hit_test(wx, wy)
        prev = self._hovered
        self._hovered = hit
        # Rebuild scene if hover target changed; always update tooltip
        if (prev is None) != (hit is None) or (
            prev and hit and (
                prev.get("type") != hit.get("type")
                or prev.get("station", None) is not hit.get("station", None)
                or prev.get("line", None) is not hit.get("line", None)
            )
        ):
            self._build_scene()
            self._build_ui()
            self._canvas.request_draw()
        elif hit is not None:
            # Mouse moved but same object — update tooltip position only
            self._build_ui()
            self._canvas.request_draw()

    def _on_click(self, event) -> None:
        sx, sy = event["x"], event["y"]

        # If a detail page is open, handle nav button clicks first
        if self._page_stack:
            w, h = self._canvas.get_logical_size()
            w, h = w or 1280, h or 900
            # Check hotspots first (clickable lines/stations in detail)
            for hs in self._detail_hotspots:
                if (hs["x"] <= sx <= hs["x"] + hs["w"]
                        and hs["y"] <= (h - sy) <= hs["y"] + hs["h"]):
                    if hs["action"] == "push_page":
                        self._page_stack.append(hs["page"])
                        self._build_ui()
                        self._canvas.request_draw()
                        return
            # Back button (top-left corner)
            if sx < 80 and sy > h - 30:
                self._page_stack.pop()
                self._build_ui()
                self._canvas.request_draw()
                return
            # Close button (top-right corner)
            if sx > w - 80 and sy > h - 30:
                self._page_stack.clear()
                self._build_ui()
                self._canvas.request_draw()
                return
            return  # in detail page, ignore other clicks for now

        # Check legend icon clicks first
        if hasattr(self, "_legend_icon_spots"):
            for hs in self._legend_icon_spots:
                if (hs["x"] <= sx <= hs["x"] + hs["w"]
                        and hs["y"] <= sy <= hs["y"] + hs["h"]):
                    action = hs["action"]
                    if action == "expand":
                        self._legend_mode = "partial"
                    elif action == "fold":
                        self._legend_mode = "full_hide"
                    elif action == "full_show":
                        self._legend_mode = "full_show"
                    self._build_ui()
                    self._canvas.request_draw()
                    return

        # Check legend item clicks
        if hasattr(self, "_legend_hotspots"):
            for hs in self._legend_hotspots:
                if (hs["x"] <= sx <= hs["x"] + hs["w"]
                        and hs["y"] <= sy <= hs["y"] + hs["h"]):
                    lid = hs["line_id"]
                    if lid in self._hidden_lines:
                        self._hidden_lines.discard(lid)
                    else:
                        self._hidden_lines.add(lid)
                    self._build_scene()
                    self._build_ui()
                    self._canvas.request_draw()
                    return

        wx, wy = self._screen_to_world(event["x"], event["y"])
        hit = self._hit_test(wx, wy)
        if hit:
            if hit["type"] == "station":
                self._page_stack.append({
                    "type": "station_detail",
                    "station": hit["station"],
                    "lines": hit["lines"],
                })
            elif hit["type"] == "line":
                self._page_stack.append({
                    "type": "line_detail",
                    "line": hit["line"],
                })
            self._build_ui()
            self._canvas.request_draw()

    # ------------------------------------------------------------------
    # Keyboard
    # ------------------------------------------------------------------
    def _on_key(self, event) -> None:
        key = event.get("key", "")
        if key in ("b", "B"):
            self._dark_mode = not self._dark_mode
            self._build_scene()
            self._canvas.request_draw()
        elif key == "[":
            self._scale_factor = max(0.3, self._scale_factor - 0.1)
            self._offsets_applied = False  # re-offset with new thickness
            self._build_scene()
            self._canvas.request_draw()
        elif key == "]":
            self._scale_factor = min(3.0, self._scale_factor + 0.1)
            self._offsets_applied = False
            self._build_scene()
            self._canvas.request_draw()
