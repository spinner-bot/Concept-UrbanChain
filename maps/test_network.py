"""Test network — a small synthetic metro network with 3 lines and 8 stations.

Transfer stations:
    S0 (Central)      — 3-line transfer (Lines 1, 2, 3)
    S2 (East Market)  — 2-line transfer (Lines 1, 3)
    S3 (South Gate)   — 2-line transfer (Lines 2, 3)
    S4 (West Lake)    — 2-line transfer (Lines 1, 2)
"""

from main import Station, StationType, Line, MetroNetwork


def create_test_network() -> MetroNetwork:
    """Create a test metro network with 3 lines, 8 stations."""
    # -- Stations -----------------------------------------------------------
    s0 = Station(0, "Central",      (2, 2),   StationType.UNDERGROUND)
    s1 = Station(1, "North Park",   (2, 7),   StationType.GROUND)
    s2 = Station(2, "East Market",  (7, 7),   StationType.ELEVATED)
    s3 = Station(3, "South Gate",   (2, -3),  StationType.UNDERGROUND)
    s4 = Station(4, "West Lake",    (-4, 2),  StationType.GROUND)
    s5 = Station(5, "Hill View",    (-5, 6),  StationType.ELEVATED)
    s6 = Station(6, "River East",   (8, 2),   StationType.BUILDING)
    s7 = Station(7, "Ocean Term.",  (8, -3),  StationType.STRUCTURE)

    # -- Line 1 (Red / named) -----------------------------------------------
    #  S4 → S0 → S1 → S2 → S6 → S7
    #  fine_trajectory: curved alignment through intermediate waypoints
    line1 = Line(
        id=1, name="Urban Line", max_speed=80,
        route=[s4, s0, s1, s2, s6, s7],
        color=(220, 50, 50),
        smooth_tension=0.5,
        fine_trajectory=[
            [(0, 0), (1, 0.5)],   # S4→S0: sweep through origin
            [],                    # S0→S1: straight
            [(4, 7)],              # S1→S2: slight dip
            [(7.5, 4.5)],          # S2→S6: gentle curve
            [],                    # S6→S7: straight
        ],
    )

    # -- Line 2 (Blue / unnamed → "地铁2号线") -------------------------------
    #  S5 → S4 → S0 → S3
    #  No fine_trajectory: all segments are straight
    line2 = Line(
        id=2, name=None, max_speed=80,
        route=[s5, s4, s0, s3],
        color=(50, 100, 220),
        smooth_tension=0.5,
        fine_trajectory=None,
    )

    # -- Line 3 (Green / named) ---------------------------------------------
    #  S5 → S2 → S0 → S3 → S7
    #  fine_trajectory: S-curves and bows
    line3 = Line(
        id=3, name="River Line", max_speed=80,
        route=[s5, s2, s0, s3, s7],
        color=(50, 180, 80),
        smooth_tension=0.5,
        fine_trajectory=[
            [(-2, 5.5), (3, 6.5)],  # S5→S2: S-curve
            [(4, 4)],                # S2→S0: bow outward
            [],                      # S0→S3: straight
            [(5, -1)],               # S3→S7: gentle curve
        ],
    )

    # -- Line 4 (Orange / ID-based route demo) --------------------------------
    #  Same as [s0, s1, s5, s6] but using station IDs
    line4 = Line(
        id=4, name="ID Demo", max_speed=60,
        route=[0, 1, 5, 6],
        color=(220, 140, 50),
    )

    network = MetroNetwork()
    network.add_line(line1)
    network.add_line(line2)
    network.add_line(line3)
    network.add_line(line4)
    return network
