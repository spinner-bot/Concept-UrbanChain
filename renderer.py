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

from matplotlib.patches import Polygon as MplPolygon

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

        for ln in self._lines:
            self._draw_line_stroke(ln)
        for ln in self._lines:
            for st in ln.route:
                self._draw_station(st, ln)

        self._fig.canvas.draw_idle()

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

    def _draw_station(self, st, ln) -> None:
        """Draw a station as a simple filled circle."""
        x, y = st.position[0], st.position[1]
        colour_01 = _rgb_01(ln.color)
        circle = plt.Circle((x, y), REGULAR_STATION_RADIUS,
                            facecolor=colour_01, edgecolor="none", zorder=5)
        self._ax.add_patch(circle)

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
