"""Catmull-Rom spline with configurable tension parameter, and line-id utility.

Generalized Catmull-Rom using alpha parameterization:
  alpha=0   : uniform (equal parameter spacing per segment)
  alpha=0.5 : centripetal (sqrt chord length) — default, best for metro curves
  alpha=1   : chordal (proportional to chord length)

The spline passes through ALL input key points exactly.
"""

import math
from typing import Sequence


def catmull_rom_spline(
    points: Sequence[tuple[float, float]],
    num_samples: int = 30,
    alpha: float = 0.5,
) -> list[tuple[float, float]]:
    """Compute a Catmull-Rom spline through a sequence of key points.

    Args:
        points: Sequence of (x, y) key points. The spline will pass through
                every point in order.
        num_samples: Number of output samples per segment between two
                     consecutive key points.
        alpha: Tension/parameterization type.
               0.0 = uniform, 0.5 = centripetal (default), 1.0 = chordal.

    Returns:
        Flat list of (x, y) points forming a smooth curve through all input
        points.  For N input points the output has roughly
        (N-1) * (num_samples + 1) + 1 samples.

    Edge cases:
        - 0 or 1 points → returns a copy of the input.
        - 2 points → linear interpolation with num_samples steps.
        - Consecutive coincident points → epsilon guard prevents division by
          zero in tangent / knot computation.
    """
    if len(points) < 2:
        return list(points)

    pts = [tuple(map(float, p)) for p in points]
    n = len(pts)

    if n == 2:
        p0, p1 = pts
        result = []
        for i in range(num_samples + 1):
            t = i / num_samples
            result.append((
                p0[0] + (p1[0] - p0[0]) * t,
                p0[1] + (p1[1] - p0[1]) * t,
            ))
        return result

    # ---------- Build phantom-augmented point list ----------
    # Reflect endpoints so the spline passes through the first and last real
    # points exactly.
    phantom_first = (2.0 * pts[0][0] - pts[1][0],
                     2.0 * pts[0][1] - pts[1][1])
    phantom_last = (2.0 * pts[-1][0] - pts[-2][0],
                    2.0 * pts[-1][1] - pts[-2][1])
    augmented = [phantom_first] + pts + [phantom_last]

    # ---------- Compute knot vector ----------
    m = len(augmented)
    t = [0.0]
    for i in range(1, m):
        dx = augmented[i][0] - augmented[i - 1][0]
        dy = augmented[i][1] - augmented[i - 1][1]
        dist = math.sqrt(dx * dx + dy * dy)
        t.append(t[-1] + max(dist, 1e-10) ** alpha)

    # ---------- Sample every real segment ----------
    result: list[tuple[float, float]] = [pts[0]]
    # The original points are augmented[1] through augmented[-2].
    # Segments connecting augmented[i] → augmented[i+1] for i in [1, m-3]
    # correspond to real segments.
    for i in range(1, m - 2):
        p0 = augmented[i - 1]
        p1 = augmented[i]
        p2 = augmented[i + 1]
        p3 = augmented[i + 2]

        t0 = t[i - 1]
        t1 = t[i]
        t2 = t[i + 1]
        t3 = t[i + 2]

        for k in range(1, num_samples + 1):
            tt = t1 + (t2 - t1) * k / num_samples
            result.append(_cr_point(p0, p1, p2, p3, t0, t1, t2, t3, tt))

    result.append(pts[-1])
    return result


def _cr_point(
    p0: tuple[float, float],
    p1: tuple[float, float],
    p2: tuple[float, float],
    p3: tuple[float, float],
    t0: float, t1: float, t2: float, t3: float,
    tt: float,
) -> tuple[float, float]:
    """Return one sample point on the Catmull-Rom segment P1→P2."""
    # Recursive linear interpolation (de Casteljau-style)
    def _lerp(a: float, b: float, u: float) -> float:
        return a + (b - a) * u

    a1_x = _lerp(p0[0], p1[0], (tt - t0) / max(t1 - t0, 1e-12))
    a1_y = _lerp(p0[1], p1[1], (tt - t0) / max(t1 - t0, 1e-12))
    a2_x = _lerp(p1[0], p2[0], (tt - t1) / max(t2 - t1, 1e-12))
    a2_y = _lerp(p1[1], p2[1], (tt - t1) / max(t2 - t1, 1e-12))
    a3_x = _lerp(p2[0], p3[0], (tt - t2) / max(t3 - t2, 1e-12))
    a3_y = _lerp(p2[1], p3[1], (tt - t2) / max(t3 - t2, 1e-12))

    b1_x = _lerp(a1_x, a2_x, (tt - t0) / max(t2 - t0, 1e-12))
    b1_y = _lerp(a1_y, a2_y, (tt - t0) / max(t2 - t0, 1e-12))
    b2_x = _lerp(a2_x, a3_x, (tt - t1) / max(t3 - t1, 1e-12))
    b2_y = _lerp(a2_y, a3_y, (tt - t1) / max(t3 - t1, 1e-12))

    cx = _lerp(b1_x, b2_x, (tt - t1) / max(t2 - t1, 1e-12))
    cy = _lerp(b1_y, b2_y, (tt - t1) / max(t2 - t1, 1e-12))

    return (cx, cy)


def build_key_points(
    route: list,
    fine_trajectory: list[list[tuple[float, float]]] | None = None,
) -> list[tuple[float, float]]:
    """Build a flat list of key points by interleaving station positions and
    fine-trajectory waypoints.

    Args:
        route: List of Station objects (each has .position as (x, y, …)).
        fine_trajectory: Optional waypoint lists.  fine_trajectory[i] is a
            list of (x, y) waypoints between route[i] and route[i+1].
            May be None (treated as all-empty).

    Returns:
        Flat list of (x, y) key points suitable for catmull_rom_spline().
    """
    keys: list[tuple[float, float]] = []
    for i, station in enumerate(route):
        x, y = station.position[0], station.position[1]
        keys.append((x, y))
        if fine_trajectory is not None and i < len(fine_trajectory):
            for wp in fine_trajectory[i]:
                keys.append((float(wp[0]), float(wp[1])))
    return keys


def line_identifier(line_id: int, line_name: str | None) -> str:
    """Return the standard identifier string for a line.

    Named line:   "name（id）"    e.g. "Urban Line（1）"
    Unnamed line:  "地铁id号线"   e.g. "地铁2号线"
    """
    if line_name:
        return f"{line_name}（{line_id}）"
    return f"地铁{line_id}号线"
