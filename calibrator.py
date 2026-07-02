"""Screen-to-world coordinate calibration via 9-point user click grid.

Press F to enter/exit calibration mode.  A 3×3 grid of target reticles is
shown.  The user clicks each highlighted target in sequence.  After all 9
clicks an affine correction (matrix + offset) is computed and applied to
all subsequent coordinate conversions.
"""

import math
import numpy as np


class Calibrator:
    """Manages calibration state and computes affine correction."""

    def __init__(self):
        self.active = False
        self._step = 0            # 0–8: which target is active
        self._screen_pts = []     # collected screen coords
        self._world_pts = []      # known world-coord targets
        self._correction_A = np.eye(2, dtype=float)   # 2×2 matrix
        self._correction_b = np.zeros(2, dtype=float)  # offset vector

    # ------------------------------------------------------------------
    def start(self, world_targets: list[tuple[float, float]]):
        """Begin a new calibration session with the given world positions."""
        self.active = True
        self._step = 0
        self._screen_pts = []
        self._world_pts = list(world_targets)

    def cancel(self):
        self.active = False

    def active_target(self) -> tuple[float, float] | None:
        """World position of the target the user should click now."""
        if not self.active or self._step >= len(self._world_pts):
            return None
        return self._world_pts[self._step]

    def step(self) -> int:
        return self._step

    def total(self) -> int:
        return len(self._world_pts)

    def register_click(self, screen_pos: tuple[float, float]):
        """Record a click at *screen_pos* for the current target."""
        if not self.active:
            return
        self._screen_pts.append(screen_pos)
        self._step += 1
        if self._step >= len(self._world_pts):
            self._finish()

    # ------------------------------------------------------------------
    def apply(self, sx: float, sy: float) -> tuple[float, float]:
        """Apply calibration correction: screen → corrected screen."""
        v = np.array([sx, sy])
        corrected = self._correction_A @ v + self._correction_b
        return (float(corrected[0]), float(corrected[1]))

    # ------------------------------------------------------------------
    def _finish(self):
        """Compute best-fit affine transform from collected pairs."""
        self.active = False
        n = len(self._screen_pts)
        if n < 3:
            return

        # Build overdetermined system:  world = A @ screen + b
        # → [sx, sy, 1] @ [a11, a21; a12, a22; b1, b2] = [wx, wy]
        S = np.array(self._screen_pts)   # n×2
        W = np.array(self._world_pts)    # n×2

        # Design matrix: each row is [sx, sy, 1]
        D = np.hstack([S, np.ones((n, 1))])  # n×3

        # Solve D @ X = W  →  X = pinv(D) @ W  (3×2)
        X, residuals, rank, s = np.linalg.lstsq(D, W, rcond=None)

        self._correction_A = X[:2, :].T   # 2×2
        self._correction_b = X[2, :]      # (2,)
        print(f"Calibration done.  "
              f"A={self._correction_A.tolist()}, b={self._correction_b.tolist()}")


# ---------------------------------------------------------------------------
# Reticle drawing helpers (world-space)
# ---------------------------------------------------------------------------
def build_reticle(x: float, y: float, base_r: float,
                  highlight: bool = False) -> list:
    """Return a list of (geometry, material) tuples for a calibration target.

    Layers (from outside in):
      1. red ring          radius = base_r            width = 0.05 * base_r
      2. yellow solid      radius = 0.35 * base_r
      3. black solid       radius = 0.115 * base_r
      4. red solid         radius = 0.085 * base_r

    When *highlight* is True the red layers become green and yellow→white.
    """
    import pygfx as gfx

    ring_c = (0.2, 0.8, 0.2, 1) if highlight else (1, 0.15, 0.15, 1)
    yel_c  = (1, 1, 1, 1) if highlight else (1, 0.85, 0.1, 1)
    blk_c  = (0, 0, 0, 1)
    dot_c  = (0.2, 0.8, 0.2, 1) if highlight else (1, 0.15, 0.15, 1)

    parts = []

    # 1. Ring — approximate as a thick line circle
    n = 48
    angles = np.linspace(0, 2 * np.pi, n, endpoint=False)
    ring_pts = np.float32([(x + math.cos(a) * base_r,
                             y + math.sin(a) * base_r, 0.05)
                            for a in angles])
    parts.append(("line", ring_pts, ring_c, base_r * 0.05))

    # 2. Yellow disc
    parts.append(("disc", x, y, base_r * 0.35, yel_c))

    # 3. Black disc
    parts.append(("disc", x, y, base_r * 0.115, blk_c))

    # 4. Red dot
    parts.append(("disc", x, y, base_r * 0.085, dot_c))

    return parts


def add_reticle_to_scene(scene, x, y, base_r, highlight=False):
    """Create reticle meshes and add them to *scene*."""
    import pygfx as gfx
    parts = build_reticle(x, y, base_r, highlight)
    for kind, *args in parts:
        if kind == "line":
            pts, color, thickness = args
            scene.add(gfx.Line(
                gfx.Geometry(positions=pts),
                gfx.LineMaterial(thickness=thickness, color=color,
                                 thickness_space="screen"),
            ))
        elif kind == "disc":
            cx, cy, r, color = args
            n = 32
            disc_pts = []
            disc_idx = []
            disc_pts.append((cx, cy, 0.045))
            for i in range(n + 1):
                a = 2 * math.pi * i / n
                disc_pts.append((cx + math.cos(a) * r, cy + math.sin(a) * r, 0.045))
            for i in range(1, n + 1):
                disc_idx.append([0, i, i + 1])
            geo = gfx.Geometry(
                positions=np.float32(disc_pts),
                indices=np.int32(disc_idx).reshape(-1, 3),
            )
            scene.add(gfx.Mesh(geo, gfx.MeshBasicMaterial(color=color)))
