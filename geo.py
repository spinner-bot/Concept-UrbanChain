"""Geographic coordinate projection utilities.

Convert between geographic coordinates (longitude, latitude) and planar
projection coordinates using a hybrid approach:

- |lat| ≤ 80° : Web Mercator (EPSG:3857) — conformal, standard for web maps.
- |lat| > 80° : Polar Stereographic — does not diverge near the poles.

The two projections are joined seamlessly at ±80° latitude: at every point
on the boundary the forward formulas yield the same (x, y), and the inverse
reconstructs the same (lon, lat).  The entire globe is covered with a
finite coordinate range.
"""

import math

# Earth radius — WGS84 semi-major axis, metres.
_EARTH_RADIUS: float = 6_378_137.0

# Latitude where we switch from Mercator to Polar Stereographic.
_THRESHOLD: float = 80.0


# ---------------------------------------------------------------------------
# Internal helpers (all work in metres on the reference sphere)
# ---------------------------------------------------------------------------

def _mercator_y(lat_rad: float) -> float:
    """Web Mercator northing in metres (without base / scale)."""
    return _EARTH_RADIUS * math.log(
        math.tan(math.pi / 4.0 + lat_rad / 2.0)
    )


def _north_polar_rho(lat_rad: float) -> float:
    """Distance in metres from the **North Pole** under Polar Stereographic."""
    return 2.0 * _EARTH_RADIUS * math.tan(math.pi / 4.0 - lat_rad / 2.0)


def _south_polar_rho(lat_rad: float) -> float:
    """Distance in metres from the **South Pole** under Polar Stereographic.

    *lat_rad* is expected to be negative for southern latitudes.
    """
    return 2.0 * _EARTH_RADIUS * math.tan(math.pi / 4.0 + lat_rad / 2.0)


# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------

def geo_to_plane(
    base_x: float,
    base_y: float,
    lon: float,
    lat: float,
    scale: float,
) -> tuple[float, float]:
    """Convert geographic coordinates to planar projection coordinates.

    Uses Web Mercator for ``|lat| ≤ 80°`` and Polar Stereographic for
    ``|lat| > 80°``.  The two projections meet continuously at ±80°.

    Args:
        base_x: X coordinate of the origin (lon=0, lat=0) in the plane.
        base_y: Y coordinate of the origin (lon=0, lat=0) in the plane.
        lon:  Longitude in degrees, positive east.
        lat:  Latitude  in degrees, positive north.
        scale: Scale factor — metres of ground distance per one unit in
               the projected plane.

    Returns:
        ``(x, y)`` coordinates in the projected plane.  Both the North
        and South Poles map to finite y values.
    """
    lon_rad = math.radians(lon)
    lat_rad = math.radians(lat)

    # ---- Mercator region (including the boundary) ------------------------
    if abs(lat) <= _THRESHOLD:
        x = base_x + scale * _EARTH_RADIUS * lon_rad
        y = base_y + scale * _mercator_y(lat_rad)
        return x, y

    # ---- Polar region ----------------------------------------------------
    threshold_rad = math.radians(_THRESHOLD)
    x = base_x + scale * _EARTH_RADIUS * lon_rad  # x stays Mercator-style

    if lat > _THRESHOLD:
        # North polar
        y_boundary = base_y + scale * _mercator_y(threshold_rad)
        rho_boundary = _north_polar_rho(threshold_rad)
        rho_actual = _north_polar_rho(lat_rad)
        delta = rho_boundary - rho_actual  # metres from boundary toward pole
        y = y_boundary + scale * delta
    else:
        # South polar  (lat < -_THRESHOLD)
        y_boundary = base_y - scale * _mercator_y(threshold_rad)
        rho_boundary = _south_polar_rho(-threshold_rad)
        rho_actual = _south_polar_rho(lat_rad)
        delta = rho_boundary - rho_actual  # metres from boundary toward pole
        y = y_boundary - scale * delta

    return x, y


def plane_to_geo(
    base_x: float,
    base_y: float,
    x: float,
    y: float,
    scale: float,
) -> tuple[float, float]:
    """Convert planar projection coordinates back to geographic coordinates.

    Inverse of :func:`geo_to_plane`.  Automatically detects whether the
    point lies in the Mercator region or either polar region.

    Args:
        base_x: X coordinate of the origin (lon=0, lat=0) in the plane.
        base_y: Y coordinate of the origin (lon=0, lat=0) in the plane.
        x:     X coordinate in the projected plane.
        y:     Y coordinate in the projected plane.
        scale: Scale factor — same value used during the forward
               projection.

    Returns:
        ``(longitude, latitude)`` in degrees.
    """
    threshold_rad = math.radians(_THRESHOLD)
    y_north_boundary = base_y + scale * _mercator_y(threshold_rad)
    y_south_boundary = base_y - scale * _mercator_y(threshold_rad)

    # Longitude is always recovered the same way (x is Mercator-style
    # everywhere).
    lon_rad = (x - base_x) / (scale * _EARTH_RADIUS)

    # ---- Mercator region -------------------------------------------------
    if y_south_boundary <= y <= y_north_boundary:
        lat_rad = (
            2.0 * math.atan(math.exp((y - base_y) / (scale * _EARTH_RADIUS)))
            - math.pi / 2.0
        )
        return math.degrees(lon_rad), math.degrees(lat_rad)

    # ---- North polar region ----------------------------------------------
    if y > y_north_boundary:
        rho_boundary = _north_polar_rho(threshold_rad)
        delta = (y - y_north_boundary) / scale
        rho = max(rho_boundary - delta, 0.0)
        lat_rad = math.pi / 2.0 - 2.0 * math.atan(
            rho / (2.0 * _EARTH_RADIUS)
        )
        return math.degrees(lon_rad), math.degrees(lat_rad)

    # ---- South polar region ----------------------------------------------
    rho_boundary = _south_polar_rho(-threshold_rad)
    delta = (y_south_boundary - y) / scale
    rho = max(rho_boundary - delta, 0.0)
    lat_rad = -math.pi / 2.0 + 2.0 * math.atan(
        rho / (2.0 * _EARTH_RADIUS)
    )
    return math.degrees(lon_rad), math.degrees(lat_rad)
