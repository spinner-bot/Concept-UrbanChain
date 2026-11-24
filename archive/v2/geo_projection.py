"""Extended geographic projections with virtual-axis support.

While :mod:`geo` provides the standard Web-Mercator / Polar-Stereographic
hybrid, this module adds **oblique projections** where the caller can
choose an arbitrary "virtual North Pole" and a virtual prime meridian.

Use cases
---------
- Polar metro systems:  set *pole_lat* / *pole_lon* so that the polar
  region becomes "equatorial", eliminating the Mercator stretch.
- Networks crossing the antimeridian:  shift *prime_meridian_offset* so
  the cut falls in empty ocean instead of bisecting the city.
"""

import math

# Earth radius (metres) — kept consistent with geo.py.
_EARTH_RADIUS: float = 6_378_137.0


# ---------------------------------------------------------------------------
# Internal — spherical rotation helpers
# ---------------------------------------------------------------------------

def _rotate_globe(lon_rad: float, lat_rad: float,
                  pole_lat_rad: float, pole_lon_rad: float,
                  ) -> tuple[float, float]:
    """Rotate the sphere so that *(pole_lat, pole_lon)* becomes the North Pole.

    Returns ``(new_lon_rad, new_lat_rad)`` — the rotated geographic
    coordinates ready for a standard Mercator projection.
    """
    # Cartesian coords on unit sphere (Z = North).
    cos_lat = math.cos(lat_rad)
    x = cos_lat * math.cos(lon_rad)
    y = cos_lat * math.sin(lon_rad)
    z = math.sin(lat_rad)

    # Step 1 — rotate around Z by -pole_lon.
    cos_pl = math.cos(-pole_lon_rad)
    sin_pl = math.sin(-pole_lon_rad)
    x1 = x * cos_pl - y * sin_pl
    y1 = x * sin_pl + y * cos_pl
    z1 = z

    # Step 2 — rotate around Y by -(pi/2 - pole_lat).
    #          This brings the virtual pole to the Z axis.
    tilt = -(math.pi / 2.0 - pole_lat_rad)
    cos_t = math.cos(tilt)
    sin_t = math.sin(tilt)
    x2 = x1 * cos_t + z1 * sin_t
    y2 = y1
    z2 = -x1 * sin_t + z1 * cos_t

    # Extract new lat / lon from rotated position.
    new_lat_rad = math.asin(max(-1.0, min(1.0, z2)))
    new_lon_rad = math.atan2(y2, x2)

    return new_lon_rad, new_lat_rad


def _unrotate_globe(lon_rad: float, lat_rad: float,
                    pole_lat_rad: float, pole_lon_rad: float,
                    ) -> tuple[float, float]:
    """Inverse of :func:`_rotate_globe`."""
    cos_lat = math.cos(lat_rad)
    x = cos_lat * math.cos(lon_rad)
    y = cos_lat * math.sin(lon_rad)
    z = math.sin(lat_rad)

    # Inverse of step 2 — rotate around Y by +(pi/2 - pole_lat).
    tilt = math.pi / 2.0 - pole_lat_rad
    cos_t = math.cos(tilt)
    sin_t = math.sin(tilt)
    x1 = x * cos_t + z * sin_t
    y1 = y
    z1 = -x * sin_t + z * cos_t

    # Inverse of step 1 — rotate around Z by +pole_lon.
    cos_pl = math.cos(pole_lon_rad)
    sin_pl = math.sin(pole_lon_rad)
    x2 = x1 * cos_pl - y1 * sin_pl
    y2 = x1 * sin_pl + y1 * cos_pl
    z2 = z1

    new_lat_rad = math.asin(max(-1.0, min(1.0, z2)))
    new_lon_rad = math.atan2(y2, x2)

    return new_lon_rad, new_lat_rad


# ---------------------------------------------------------------------------
# Mercator helpers (standard — no polar switch needed after rotation)
# ---------------------------------------------------------------------------

def _mercator_y(lat_rad: float) -> float:
    """Web Mercator northing in metres."""
    return _EARTH_RADIUS * math.log(
        math.tan(math.pi / 4.0 + lat_rad / 2.0)
    )


# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------

def oblique_mercator_to_plane(
    base_x: float,
    base_y: float,
    lon: float,
    lat: float,
    scale: float,
    pole_lat: float = 90.0,
    pole_lon: float = 0.0,
    prime_meridian_offset: float = 0.0,
) -> tuple[float, float]:
    """Convert geographic coordinates to planar coordinates using a
    user-defined virtual Earth axis.

    The globe is rotated so that *(pole_lat, pole_lon)* becomes the new
    North Pole, the standard Web Mercator projection is applied, and the
    resulting longitude is shifted by *prime_meridian_offset*.

    Args:
        base_x: Plane X of the virtual equator / prime-meridian crossing.
        base_y: Plane Y of the virtual equator / prime-meridian crossing.
        lon:   True longitude in degrees.
        lat:   True latitude in degrees.
        scale: Metres per unit in the projected plane.
        pole_lat:  Latitude of the **virtual** North Pole, in degrees
                   (default 90 = real North Pole — no rotation).
        pole_lon:  Longitude of the **virtual** North Pole, in degrees
                   (default 0).
        prime_meridian_offset:  Degrees added to the rotated longitude
                   (default 0 = virtual prime meridian stays where the
                   rotation placed it).

    Returns:
        ``(x, y)`` in the projected plane.

    Example
    -------
    Make the South Pole behave like the equator::

        oblique_mercator_to_plane(0, 0, 0, -90, 1e-5,
                                  pole_lat=-90, pole_lon=0)
        # -> (0, 0)   the South Pole maps to the origin.
    """
    lon_rad = math.radians(lon)
    lat_rad = math.radians(lat)
    pole_lat_rad = math.radians(pole_lat)
    pole_lon_rad = math.radians(pole_lon)

    new_lon_rad, new_lat_rad = _rotate_globe(
        lon_rad, lat_rad, pole_lat_rad, pole_lon_rad,
    )

    # Apply prime meridian shift.
    new_lon_rad += math.radians(prime_meridian_offset)

    x = base_x + scale * _EARTH_RADIUS * new_lon_rad
    y = base_y + scale * _mercator_y(new_lat_rad)
    return x, y


def plane_to_oblique_mercator(
    base_x: float,
    base_y: float,
    x: float,
    y: float,
    scale: float,
    pole_lat: float = 90.0,
    pole_lon: float = 0.0,
    prime_meridian_offset: float = 0.0,
) -> tuple[float, float]:
    """Inverse of :func:`oblique_mercator_to_plane`.

    Recovers the true geographic *(longitude, latitude)* from a point in
    the obliquely-projected plane.

    Args:
        base_x, base_y, scale: Same values used during forward projection.
        x, y:  Coordinates in the projected plane.
        pole_lat, pole_lon, prime_meridian_offset:
               Same virtual-axis parameters used during forward projection.

    Returns:
        ``(longitude, latitude)`` in degrees.
    """
    pole_lat_rad = math.radians(pole_lat)
    pole_lon_rad = math.radians(pole_lon)

    new_lon_rad = (x - base_x) / (scale * _EARTH_RADIUS)
    new_lat_rad = (
        2.0 * math.atan(
            math.exp((y - base_y) / (scale * _EARTH_RADIUS))
        )
        - math.pi / 2.0
    )

    # Remove prime meridian shift.
    new_lon_rad -= math.radians(prime_meridian_offset)

    lon_rad, lat_rad = _unrotate_globe(
        new_lon_rad, new_lat_rad, pole_lat_rad, pole_lon_rad,
    )

    return math.degrees(lon_rad), math.degrees(lat_rad)


# ---------------------------------------------------------------------------
# Geographic toolkit
# ---------------------------------------------------------------------------

def haversine_distance(lat1: float, lon1: float,
                       lat2: float, lon2: float) -> float:
    """Great-circle distance between two points on the WGS84 sphere (metres).

    Uses the haversine formula, which is numerically stable for both
    small and large distances.

    Args:
        lat1, lon1: Start point in degrees.
        lat2, lon2: End   point in degrees.

    Returns:
        Distance in metres.
    """
    phi1 = math.radians(lat1)
    phi2 = math.radians(lat2)
    dphi = math.radians(lat2 - lat1)
    dlam = math.radians(lon2 - lon1)

    a = (math.sin(dphi / 2.0) ** 2
         + math.cos(phi1) * math.cos(phi2) * math.sin(dlam / 2.0) ** 2)
    return _EARTH_RADIUS * 2.0 * math.atan2(math.sqrt(a), math.sqrt(1.0 - a))


def bearing(lat1: float, lon1: float,
            lat2: float, lon2: float) -> float:
    """Initial bearing (forward azimuth) from point 1 to point 2.

    Args:
        lat1, lon1: Start point in degrees.
        lat2, lon2: End   point in degrees.

    Returns:
        Bearing in degrees (0 = North, 90 = East, clockwise).
    """
    phi1 = math.radians(lat1)
    phi2 = math.radians(lat2)
    dlam = math.radians(lon2 - lon1)

    y = math.sin(dlam) * math.cos(phi2)
    x = (math.cos(phi1) * math.sin(phi2)
         - math.sin(phi1) * math.cos(phi2) * math.cos(dlam))

    return (math.degrees(math.atan2(y, x)) + 360.0) % 360.0


def destination_point(lat: float, lon: float,
                      brng: float, distance: float) -> tuple[float, float]:
    """Given a start point, bearing and distance, compute the destination.

    Args:
        lat, lon: Start point in degrees.
        brng:     Forward azimuth in degrees (0 = North).
        distance: Distance to travel in metres.

    Returns:
        ``(latitude, longitude)`` of the destination in degrees.
    """
    phi = math.radians(lat)
    brng_rad = math.radians(brng)
    delta = distance / _EARTH_RADIUS  # angular distance

    phi2 = math.asin(
        math.sin(phi) * math.cos(delta)
        + math.cos(phi) * math.sin(delta) * math.cos(brng_rad)
    )
    lon2 = math.radians(lon) + math.atan2(
        math.sin(brng_rad) * math.sin(delta) * math.cos(phi),
        math.cos(delta) - math.sin(phi) * math.sin(phi2),
    )

    return math.degrees(phi2), math.degrees(lon2)


def wrap_longitude(lon: float) -> float:
    """Normalise a longitude value to ``[-180, 180]`` degrees.

    >>> wrap_longitude(190)
    -170.0
    >>> wrap_longitude(-190)
    170.0
    """
    return ((lon + 180.0) % 360.0) - 180.0


def is_antimeridian_crossing(lon1: float, lon2: float) -> bool:
    """Return *True* if the shorter arc between two longitudes crosses
    the ±180° antimeridian.

    This is useful for detecting routes that would "teleport" across
    the map edge in a simple equirectangular or Mercator display.

    >>> is_antimeridian_crossing(170, -170)
    True
    >>> is_antimeridian_crossing(10, 20)
    False
    """
    return abs(lon1 - lon2) > 180.0
