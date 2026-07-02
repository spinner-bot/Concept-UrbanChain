"""Interactive metro map renderer using matplotlib.

Provides MetroMapRenderer — a pan-able, zoom-able map view that draws metro
lines and stations with data-coordinate sizes.
"""

import math
from collections import defaultdict

import matplotlib
matplotlib.use("TkAgg")
import matplotlib.pyplot as plt
from matplotlib.backend_bases import MouseButton

from matplotlib.patches import (Polygon as MplPolygon, Wedge,
                                 Circle as MplCircle, Rectangle as MplRectangle)

from spline import catmull_rom_spline, build_key_points


# ---------------------------------------------------------------------------
# Geometry helpers
# ---------------------------------------------------------------------------
def _build_line_polygon(
    points: list[tuple[float, float]],
    half_width: float,
) -> list[tuple[float, float]] | None:
    """Build a closed polygon that represents a thick line through *points*.

    For each point a normal direction is estimated via central finite
    differences and the point is offset by ±*half_width* along the normal.
    The left-side offsets are chained, then the right-side offsets are
    chained in reverse to form a closed polygon.

    Returns ``None`` when the point list is too short to form a polygon.
    """
    n = len(points)
    if n < 2:
        return None

    left: list[tuple[float, float]] = []
    right: list[tuple[float, float]] = []

    for i in range(n):
        if i == 0:
            dx = points[1][0] - points[0][0]
            dy = points[1][1] - points[0][1]
        elif i == n - 1:
            dx = points[-1][0] - points[-2][0]
            dy = points[-1][1] - points[-2][1]
        else:
            dx = points[i + 1][0] - points[i - 1][0]
            dy = points[i + 1][1] - points[i - 1][1]

        length = math.sqrt(dx * dx + dy * dy)
        if length < 1e-10:
            nx, ny = 0.0, 1.0
        else:
            nx = -dy / length
            ny = dx / length

        left.append((points[i][0] + nx * half_width,
                      points[i][1] + ny * half_width))
        right.append((points[i][0] - nx * half_width,
                       points[i][1] - ny * half_width))

    # Closed polygon: left chain + reversed right chain
    return left + right[::-1]


# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------
REGULAR_STATION_RADIUS = 5.0   # base centre-circle radius in data coords
RING_GAP = 2.0                 # radial gap between centre and outer ring
RING_WIDTH = 1.5               # thickness of outer ring stroke
TRANSFER_MULTIPLIER = 1.65     # centre-circle radius multiplier for transfers
LINE_WIDTH = REGULAR_STATION_RADIUS * 2  # line thickness = diameter of regular centre
GRADIENT_STEPS = 12            # number of thin wedges per gradient band
GRADIENT_ANGLE = 12.0          # degrees of each gradient transition band
ZOOM_FACTOR = 1.2              # scroll-wheel zoom multiplier
NUM_SPLINE_SAMPLES = 30        # spline sample points per key-point segment


# ---------------------------------------------------------------------------
# Colour helpers
# ---------------------------------------------------------------------------
def _rgb_01(rgb: tuple[int, int, int]) -> tuple[float, float, float]:
    """Normalise (0-255) RGB to (0-1)."""
    return tuple(c / 255.0 for c in rgb)


def _luminance(rgb: tuple[int, int, int]) -> float:
    """Relative luminance (ITU-R BT.601)."""
    return 0.299 * rgb[0] + 0.587 * rgb[1] + 0.114 * rgb[2]


def _fg_for_bg(rgb: tuple[int, int, int]) -> tuple[int, int, int]:
    """Return black (0,0,0) or white (255,255,255) for text on *rgb*."""
    return (0, 0, 0) if _luminance(rgb) > 128 else (255, 255, 255)


def _interpolate_rgb(a: tuple[int, int, int], b: tuple[int, int, int],
                     t: float) -> tuple[int, int, int]:
    """Linear interpolation between two (0-255) RGB colours."""
    return tuple(max(0, min(255, int(a[i] + (b[i] - a[i]) * t)))
                 for i in range(3))


# ===================================================================
# MetroMapRenderer
# ===================================================================
class MetroMapRenderer:
    """Interactive matplotlib-based metro map.

    Parameters
    ----------
    lines : list
        List of Line objects (from main.py).  Each line must have at least
        ``.route`` (list of Station), ``.color``, ``.fine_trajectory`` and
        ``.smooth_tension`` attributes.
    """

    def __init__(self, lines: list) -> None:
        self._lines = lines

        # ---- Build station → lines index ----
        self._station_lines: dict[int, list] = defaultdict(list)
        for ln in lines:
            seen: set[int] = set()
            for st in ln.route:
                if st.id not in seen:
                    self._station_lines[st.id].append(ln)
                    seen.add(st.id)

        # ---- Precompute spline data for each line ----
        self._spline_data: dict[int, list[tuple[float, float]]] = {}
        for ln in lines:
            keys = build_key_points(ln.route, ln.fine_trajectory)
            self._spline_data[ln.id] = catmull_rom_spline(
                keys, NUM_SPLINE_SAMPLES, ln.smooth_tension,
            )

        # ---- State ----
        self._dark_mode = False
        self._pan_start: tuple[float, float] | None = None

        # ---- Matplotlib ----
        self._fig, self._ax = plt.subplots(figsize=(12, 9))
        self._fig.canvas.manager.set_window_title("Concept UrbanChain")
        self._ax.set_aspect("equal")
        self._ax.set_facecolor("#FFFFFF")
        self._fig.set_facecolor("#F0F0F0")

        # ---- Connect events ----
        self._fig.canvas.mpl_connect("button_press_event", self._on_press)
        self._fig.canvas.mpl_connect("button_release_event", self._on_release)
        self._fig.canvas.mpl_connect("motion_notify_event", self._on_motion)
        self._fig.canvas.mpl_connect("scroll_event", self._on_scroll)
        self._fig.canvas.mpl_connect("key_press_event", self._on_key)

        # ---- Initial draw ----
        self._auto_fit()
        self._draw_all()

    # ------------------------------------------------------------------
    # Public API
    # ------------------------------------------------------------------
    def show(self) -> None:
        """Display the map window (blocking)."""
        plt.show()

    # ------------------------------------------------------------------
    # Drawing
    # ------------------------------------------------------------------
    def _draw_all(self) -> None:
        """Clear axes and redraw everything."""
        self._ax.clear()
        self._ax.set_aspect("equal")
        self._ax.set_facecolor("#1A1A1A" if self._dark_mode else "#FFFFFF")
        self._fg = (255, 255, 255) if self._dark_mode else (0, 0, 0)

        # Draw lines first, then stations on top
        for ln in self._lines:
            self._draw_line_stroke(ln)

        # Collect unique stations and draw each once
        drawn: set[int] = set()
        for ln in self._lines:
            for st in ln.route:
                if st.id in drawn:
                    continue
                drawn.add(st.id)
                s_lines = self._station_lines[st.id]
                if len(s_lines) >= 2:
                    self._draw_transfer_station(st, s_lines)
                else:
                    self._draw_regular_station(st, s_lines[0])

        self._draw_legend()
        self._fig.canvas.draw_idle()

    # ------------------------------------------------------------------
    # Line drawing
    # ------------------------------------------------------------------
    def _draw_line_stroke(self, ln) -> None:
        """Draw a line as a filled polygon with data-coordinate width.

        The polygon has no edge (no border) and its width equals the
        diameter of a regular station centre circle (LINE_WIDTH).
        """
        pts = self._spline_data[ln.id]
        if len(pts) < 2:
            return
        poly_verts = _build_line_polygon(pts, LINE_WIDTH / 2.0)
        if poly_verts is None:
            return
        colour_01 = _rgb_01(ln.color)
        patch = MplPolygon(poly_verts, facecolor=colour_01, edgecolor="none",
                           zorder=2, closed=True)
        self._ax.add_patch(patch)

    # ------------------------------------------------------------------
    # Station drawing
    # ------------------------------------------------------------------
    def _station_outer_radius(self, is_transfer: bool) -> float:
        centre = (REGULAR_STATION_RADIUS * TRANSFER_MULTIPLIER
                  if is_transfer else REGULAR_STATION_RADIUS)
        return centre + RING_GAP

    def _draw_regular_station(self, st, ln) -> None:
        """Draw a non-transfer station: solid centre + outer ring."""
        x, y = st.position[0], st.position[1]
        r = REGULAR_STATION_RADIUS
        ring_r = r + RING_GAP

        # Outer ring
        ring = MplCircle((x, y), ring_r, facecolor="none",
                         edgecolor=_rgb_01(self._fg), linewidth=RING_WIDTH,
                         zorder=4)
        self._ax.add_patch(ring)
        # Centre
        centre = MplCircle((x, y), r, facecolor=_rgb_01(ln.color),
                           edgecolor="none", zorder=5)
        self._ax.add_patch(centre)

    def _draw_transfer_station(self, st, lines: list) -> None:
        """Draw a transfer station with angular gradient centre.

        The centre radius is 165 % of a regular station.  The gradient
        transitions between line colours at sector boundaries determined
        by each line's approach angle.
        """
        x, y = st.position[0], st.position[1]
        r = REGULAR_STATION_RADIUS * TRANSFER_MULTIPLIER
        ring_r = r + RING_GAP

        # Outer ring
        ring = MplCircle((x, y), ring_r, facecolor="none",
                         edgecolor=_rgb_01(self._fg), linewidth=RING_WIDTH,
                         zorder=4)
        self._ax.add_patch(ring)

        # Compute approach angles and sort lines by angle
        angled_lines: list[tuple[float, object]] = []
        for ln in lines:
            angle = self._approach_angle(st, ln)
            angled_lines.append((angle, ln))
        angled_lines.sort(key=lambda item: item[0])

        n = len(angled_lines)
        colours = [ln.color for _, ln in angled_lines]
        angles = [a for a, _ in angled_lines]

        # Draw 360 1-degree wedges with softmax colour blending
        for deg in range(360):
            theta = float(deg)
            blended = self._blend_colours(theta, angles, colours)
            wedge = Wedge((x, y), r, theta, theta + 1.0,
                          facecolor=_rgb_01(blended), edgecolor="none",
                          zorder=5)
            self._ax.add_patch(wedge)

    # ------------------------------------------------------------------
    # Gradient helpers
    # ------------------------------------------------------------------
    def _approach_angle(self, st, ln) -> float:
        """Angle (degrees) at which *ln* approaches station *st*.

        Uses the precomputed spline tangent at the sample point closest to
        the station position.
        """
        pts = self._spline_data[ln.id]
        sx, sy = st.position[0], st.position[1]

        # Find closest spline sample
        best_i = 0
        best_d2 = float("inf")
        for i, (px, py) in enumerate(pts):
            d2 = (px - sx) ** 2 + (py - sy) ** 2
            if d2 < best_d2:
                best_d2 = d2
                best_i = i

        # Tangent at best_i
        if best_i == 0:
            dx = pts[1][0] - pts[0][0]
            dy = pts[1][1] - pts[0][1]
        elif best_i == len(pts) - 1:
            dx = pts[-1][0] - pts[-2][0]
            dy = pts[-1][1] - pts[-2][1]
        else:
            dx = pts[best_i + 1][0] - pts[best_i - 1][0]
            dy = pts[best_i + 1][1] - pts[best_i - 1][1]

        angle = math.degrees(math.atan2(dy, dx))
        return angle % 360

    def _blend_colours(
        self,
        theta: float,
        angles: list[float],
        colours: list[tuple[int, int, int]],
    ) -> tuple[int, int, int]:
        """Softmax colour blend at *theta* (degrees) given line angles."""
        n = len(colours)
        if n == 1:
            return colours[0]

        # Angular distance from theta to each line angle
        weights: list[float] = []
        for a in angles:
            diff = abs(theta - a)
            if diff > 180:
                diff = 360 - diff
            # Gaussian-like weight; sigma controls blend sharpness
            sigma = 20.0
            w = math.exp(-0.5 * (diff / sigma) ** 2)
            weights.append(w)

        total = sum(weights)
        if total < 1e-10:
            return colours[0]

        r = g = b = 0.0
        for i in range(n):
            w = weights[i] / total
            r += colours[i][0] * w
            g += colours[i][1] * w
            b += colours[i][2] * w

        return (int(round(r)), int(round(g)), int(round(b)))

    # ------------------------------------------------------------------
    # Legend
    # ------------------------------------------------------------------
    def _draw_legend(self) -> None:
        """Draw a colour legend in the bottom-left corner."""
        fg_01 = _rgb_01(self._fg)
        for i, ln in enumerate(self._lines):
            y_pos = 0.04 + i * 0.05
            # Colour swatch (rectangle in axes coords)
            swatch = MplRectangle(
                (0.02, y_pos), 0.03, 0.03,
                transform=self._ax.transAxes,
                facecolor=_rgb_01(ln.color),
                edgecolor="none",
                zorder=10,
            )
            self._ax.add_patch(swatch)
            # Label text
            label = ln.name if ln.name else f"地铁{ln.id}号线"
            self._ax.text(
                0.06, y_pos + 0.015, label,
                transform=self._ax.transAxes,
                fontsize=9, color=fg_01, va="center", zorder=10,
            )

    # ------------------------------------------------------------------
    # Viewport
    # ------------------------------------------------------------------
    def _auto_fit(self) -> None:
        """Set initial axis limits to enclose all stations with padding."""
        xs, ys = [], []
        for ln in self._lines:
            for st in ln.route:
                xs.append(st.position[0])
                ys.append(st.position[1])
        if not xs:
            self._ax.set_xlim(-10, 10)
            self._ax.set_ylim(-10, 10)
            return
        pad_x = max((max(xs) - min(xs)) * 0.15, 2)
        pad_y = max((max(ys) - min(ys)) * 0.15, 2)
        self._ax.set_xlim(min(xs) - pad_x, max(xs) + pad_x)
        self._ax.set_ylim(min(ys) - pad_y, max(ys) + pad_y)

    # ------------------------------------------------------------------
    # Interaction handlers
    # ------------------------------------------------------------------
    def _on_press(self, event) -> None:
        if event.button is MouseButton.LEFT and event.inaxes is self._ax:
            self._pan_start = (event.xdata, event.ydata)

    def _on_release(self, event) -> None:
        self._pan_start = None

    def _on_motion(self, event) -> None:
        if self._pan_start is None or event.inaxes is not self._ax:
            return
        if event.xdata is None or event.ydata is None:
            return
        dx = self._pan_start[0] - event.xdata
        dy = self._pan_start[1] - event.ydata
        xlim = self._ax.get_xlim()
        ylim = self._ax.get_ylim()
        self._ax.set_xlim(xlim[0] + dx, xlim[1] + dx)
        self._ax.set_ylim(ylim[0] + dy, ylim[1] + dy)
        self._fig.canvas.draw_idle()

    def _on_scroll(self, event) -> None:
        if event.inaxes is not self._ax:
            return
        factor = ZOOM_FACTOR if event.button == "down" else 1.0 / ZOOM_FACTOR
        xdata, ydata = event.xdata, event.ydata
        if xdata is None or ydata is None:
            return
        xlim = self._ax.get_xlim()
        ylim = self._ax.get_ylim()
        new_w = (xlim[1] - xlim[0]) * factor
        new_h = (ylim[1] - ylim[0]) * factor
        rx = (xdata - xlim[0]) / (xlim[1] - xlim[0])
        ry = (ydata - ylim[0]) / (ylim[1] - ylim[0])
        self._ax.set_xlim(xdata - new_w * rx, xdata + new_w * (1 - rx))
        self._ax.set_ylim(ydata - new_h * ry, ydata + new_h * (1 - ry))
        self._fig.canvas.draw_idle()

    def _on_key(self, event) -> None:
        if event.key in ("b", "B"):
            self._dark_mode = not self._dark_mode
            self._draw_all()
