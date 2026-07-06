"""Geographic coordinate projection utilities.

Convert between geographic coordinates (longitude, latitude) and planar
projection coordinates using the Web Mercator projection (EPSG:3857).
"""

import math

# Earth radius used by Web Mercator — WGS84 semi-major axis, metres.
_EARTH_RADIUS: float = 6_378_137.0

# Web Mercator latitude bound — beyond this the projection diverges.
_MAX_LAT: float = 85.051_129


def geo_to_plane(
    base_x: float,
    base_y: float,
    lon: float,
    lat: float,
    scale: float,
) -> tuple[float, float]:
    """Convert geographic coordinates to planar projection coordinates.

    Projects *(lon, lat)* onto a plane using the Web Mercator formula.
    ``(base_x, base_y)`` is the image of the equator / prime-meridian
    intersection (0° N, 0° E), and *scale* controls metres-per-unit zoom.

    Args:
        base_x: X coordinate of the origin (lon=0, lat=0) in the plane.
        base_y: Y coordinate of the origin (lon=0, lat=0) in the plane.
        lon:  Longitude in degrees, positive east (-180 … 180).
        lat:  Latitude  in degrees, positive north (-90 … 90).  Values
              outside ±85.051° are clamped to the Web Mercator limit.
        scale: Scale factor — metres of ground distance per one unit in
               the projected plane.

    Returns:
        ``(x, y)`` coordinates in the projected plane.

    Edge cases:
        - Latitude is silently clamped to ``±_MAX_LAT`` so the
          projection never diverges.
        - Longitude is not wrapped — callers should normalise to
          ``[-180, 180]`` if needed.
    """
    lat = max(-_MAX_LAT, min(_MAX_LAT, lat))

    lon_rad = math.radians(lon)
    lat_rad = math.radians(lat)

    x = base_x + scale * _EARTH_RADIUS * lon_rad
    y = base_y + scale * _EARTH_RADIUS * math.log(
        math.tan(math.pi / 4.0 + lat_rad / 2.0)
    )

    return x, y


def plane_to_geo(
    base_x: float,
    base_y: float,
    x: float,
    y: float,
    scale: float,
) -> tuple[float, float]:
    """Convert planar projection coordinates back to geographic coordinates.

    Inverse of :func:`geo_to_plane`.  Takes a point in the projected plane
    and recovers its *(longitude, latitude)* under the Web Mercator
    projection.

    Args:
        base_x: X coordinate of the origin (lon=0, lat=0) in the plane.
        base_y: Y coordinate of the origin (lon=0, lat=0) in the plane.
        x:     X coordinate in the projected plane.
        y:     Y coordinate in the projected plane.
        scale: Scale factor — same value used during the forward
               projection.

    Returns:
        ``(longitude, latitude)`` in degrees.  Longitude is in
        ``[-180, 180]``; latitude may exceed ±85.051° if the input
        *y* maps beyond the Web Mercator range (rare in practice).

    Edge cases:
        - The inverse of the latitude clamp in :func:`geo_to_plane` is
          lossy — points projected from latitudes beyond ±85.051° cannot
          be distinguished from those exactly at the bound.
    """
    lon_rad = (x - base_x) / (scale * _EARTH_RADIUS)
    lat_rad = (
        2.0 * math.atan(math.exp((y - base_y) / (scale * _EARTH_RADIUS)))
        - math.pi / 2.0
    )

    return math.degrees(lon_rad), math.degrees(lat_rad)
