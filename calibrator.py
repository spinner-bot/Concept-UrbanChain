"""Screen-to-world coordinate calibration.

F key — 9-point click grid.
G key — manual cursor alignment (WASD/arrows + Space).
"""

import math
import numpy as np


# ===================================================================
# Manual calibration (G key)
# ===================================================================
class ManualCalibrator:
    """Move a crosshair to align with the real cursor, submit offsets."""

    def __init__(self):
        self.active = False
        self._samples = []           # (dx, dy) offsets collected
        self._cursor_x = 640.0       # crosshair screen position
        self._cursor_y = 450.0
        self._fine_step = 0.5        # WASD step (pixels)
        self._coarse_step = 5.0      # arrow key step
        self._offset_x = 0.0
        self._offset_y = 0.0
        self._adjusted = False       # True once user has moved cursor

    def start(self, init_sx, init_sy):
        self.active = True
        self._cursor_x = init_sx
        self._cursor_y = init_sy
        self._samples = []
        self._adjusted = False

    def cancel(self):
        self.active = False

    def set_cursor(self, x, y):
        """Update cursor to follow mouse (before user starts adjusting)."""
        self._cursor_x = x
        self._cursor_y = y

    def move(self, dx, dy):
        self._cursor_x += dx
        self._cursor_y += dy

    def submit(self, mouse_sx, mouse_sy):
        """Record offset: how far the aligned crosshair is from theoretical."""
        # theoretical = where system thinks mouse is (before user moves)
        # actual = where crosshair ended up (aligned with real mouse)
        offset_dx = self._cursor_x - mouse_sx
        offset_dy = self._cursor_y - mouse_sy
        self._samples.append((offset_dx, offset_dy))
        # Recompute average offset
        if self._samples:
            self._offset_x = sum(s[0] for s in self._samples) / len(self._samples)
            self._offset_y = sum(s[1] for s in self._samples) / len(self._samples)
        return len(self._samples)

    def sample_count(self):
        return len(self._samples)

    def cursor_pos(self):
        return (self._cursor_x, self._cursor_y)

    def apply(self, sx, sy):
        return (sx + self._offset_x, sy + self._offset_y)


# ===================================================================
# Grid calibration (F key)
# ===================================================================


class Calibrator:
    """Collects click offsets and computes an average screen-space offset."""

    def __init__(self):
        self.active = False
        self._step = 0
        self._targets = []        # world positions of the 9 targets
        self._raw_clicks = []     # screen coords where user clicked
        self._offset_x = 0.0      # computed offset: corrected = raw + offset
        self._offset_y = 0.0

    # ------------------------------------------------------------------
    def start(self, world_targets):
        self.active = True
        self._step = 0
        self._targets = list(world_targets)
        self._raw_clicks = []

    def cancel(self):
        self.active = False

    def step(self):
        return self._step

    def total(self):
        return len(self._targets)

    def active_target_world(self):
        if not self.active or self._step >= len(self._targets):
            return None
        return self._targets[self._step]

    def register_click(self, raw_screen_pos):
        if not self.active:
            return
        self._raw_clicks.append(raw_screen_pos)
        self._step += 1
        if self._step >= len(self._targets):
            self._finish()

    # ------------------------------------------------------------------
    def apply(self, sx, sy):
        """Apply the calibration offset."""
        return (sx + self._offset_x, sy + self._offset_y)

    # ------------------------------------------------------------------
    def _finish(self):
        """Compute average offset from ideal screen positions."""
        if self.active:
            self.active = False
        n = len(self._raw_clicks)
        if n < 3:
            return

        # Convert world targets to ideal screen positions
        # (done externally — the renderer passes world targets,
        #  and we need to compute ideal screen from them.
        #  For now, we compute the offset directly:
        #  offset = mean(ideal_screen - raw_screen)
        #  But we need ideal_screen which depends on camera state.
        #  Instead, we'll have the renderer compute it.)
        #
        # For simplicity: the Calibrator just stores the raw clicks.
        # The renderer will call _compute_offset with ideal screen positions.
        print(f"Calibrator: collected {n} raw clicks (waiting for ideal positions)")

    def compute_offset(self, ideal_screen_positions):
        """Compute offset from ideal (world-derived) screen positions."""
        n = min(len(self._raw_clicks), len(ideal_screen_positions))
        if n < 3:
            return
        dx = dy = 0.0
        for i in range(n):
            ix, iy = ideal_screen_positions[i]
            rx, ry = self._raw_clicks[i]
            dx += ix - rx
            dy += iy - ry
        self._offset_x = dx / n
        self._offset_y = dy / n
        print(f"Calibration done: offset=({self._offset_x:.1f}, {self._offset_y:.1f})")


# ---------------------------------------------------------------------------
# Reticle drawing
# ---------------------------------------------------------------------------
def add_crosshair_to_scene(scene, sx, sy, color=(0, 1, 1, 1), size=24):
    """Draw a screen-space crosshair (+) at pixel position (sx, sy)."""
    import pygfx as gfx
    h = size
    gap = 5  # center gap for precision placement
    mat = gfx.LineMaterial(thickness=3, color=color)
    # horizontal (two parts with gap)
    scene.add(gfx.Line(gfx.Geometry(positions=np.float32([(sx - h, sy, 0), (sx - gap, sy, 0)])), mat))
    scene.add(gfx.Line(gfx.Geometry(positions=np.float32([(sx + gap, sy, 0), (sx + h, sy, 0)])), mat))
    # vertical (two parts with gap)
    scene.add(gfx.Line(gfx.Geometry(positions=np.float32([(sx, sy - h, 0), (sx, sy - gap, 0)])), mat))
    scene.add(gfx.Line(gfx.Geometry(positions=np.float32([(sx, sy + gap, 0), (sx, sy + h, 0)])), mat))


def add_reticle_to_scene(scene, x, y, base_r, highlight=False):
    """Draw a calibration target reticle in world space."""
    import pygfx as gfx

    ring_c = (0.2, 0.9, 0.2, 1) if highlight else (0.9, 0.15, 0.15, 1)
    yel_c = (1, 1, 1, 1) if highlight else (1, 0.85, 0.1, 1)
    dot_c = (0.2, 0.9, 0.2, 1) if highlight else (0.9, 0.15, 0.15, 1)

    z = 0.05
    n = 48

    # 1. outer ring (red/green)
    angles = np.linspace(0, 2 * np.pi, n, endpoint=False)
    ring_pts = np.float32([(x + math.cos(a) * base_r,
                             y + math.sin(a) * base_r, z) for a in angles])
    scene.add(gfx.Line(
        gfx.Geometry(positions=ring_pts),
        gfx.LineMaterial(thickness=base_r * 0.05 * 30, color=ring_c,
                         thickness_space="screen"),
    ))

    # 2. yellow disc
    _disc(scene, x, y, base_r * 0.35, yel_c, z - 0.001)

    # 3. black disc
    _disc(scene, x, y, base_r * 0.115, (0, 0, 0, 1), z - 0.002)

    # 4. red/green center dot
    _disc(scene, x, y, base_r * 0.085, dot_c, z - 0.003)


def _disc(scene, cx, cy, r, color, z):
    import pygfx as gfx
    n = 32
    verts = [(cx, cy, z)]
    for i in range(n + 1):
        a = 2 * math.pi * i / n
        verts.append((cx + math.cos(a) * r, cy + math.sin(a) * r, z))
    idx = []
    for i in range(1, n + 1):
        idx.append([0, i, i + 1])
    scene.add(gfx.Mesh(
        gfx.Geometry(positions=np.float32(verts),
                      indices=np.int32(idx).reshape(-1, 3)),
        gfx.MeshBasicMaterial(color=color),
    ))
