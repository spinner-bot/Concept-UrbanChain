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
