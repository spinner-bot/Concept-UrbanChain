from enum import Enum


class StationType(Enum):
    """Station construction type."""
    UNDERGROUND = "underground"   # 地下站
    GROUND = "ground"             # 地面站
    ELEVATED = "elevated"         # 地上站
    BUILDING = "building"         # 建筑站（预留）
    STRUCTURE = "structure"       # 结构站（预留）


class Station:
    # Z-coordinate defaults per station type
    _Z_DEFAULTS = {
        StationType.UNDERGROUND: -10,
        StationType.GROUND: 1,
        StationType.ELEVATED: 10,
        StationType.BUILDING: 0,
        StationType.STRUCTURE: 0,
    }

    def __init__(self, id: int, name: str, position: tuple,
                 station_type: StationType = StationType.UNDERGROUND):
        self.id = id
        self.name = name
        self.station_type = station_type
        # Auto-fill Z if position has only 2 components
        if len(position) == 2:
            z = self._Z_DEFAULTS.get(station_type, 0)
            self.position = (position[0], position[1], z)
        else:
            self.position = tuple(position)

class Line:
    def __init__(self, id: int, name: str, route: list, max_speed: float,
                 color: tuple = (255, 0, 0),
                 fine_trajectory: list = None,
                 smooth_tension: float = 0.5,
                 hide_terminal_label: bool = False):
        self.id = id
        self.name = name
        self.route = route
        self.max_speed = max_speed
        self.color = color  # RGB tuple (0-255)
        # Catmull-Rom alpha: 0=uniform, 0.5=centripetal (default), 1=chordal
        self.smooth_tension = smooth_tension
        self.hide_terminal_label = hide_terminal_label
        # fine_trajectory[i] = list of (x, y) waypoints between route[i] and route[i+1]
        # len(fine_trajectory) == len(route) - 1
        if fine_trajectory is None:
            self.fine_trajectory = [[] for _ in range(len(route) - 1)]
        else:
            # pad or truncate to match len(route) - 1
            target_len = len(route) - 1
            if len(fine_trajectory) < target_len:
                self.fine_trajectory = list(fine_trajectory) + \
                    [[] for _ in range(target_len - len(fine_trajectory))]
            else:
                self.fine_trajectory = list(fine_trajectory[:target_len])


class MetroNetwork:
    """Container for all metro lines and stations."""

    def __init__(self):
        self.lines: list[Line] = []
        self.stations: dict[int, Station] = {}

    def add_line(self, line: Line) -> None:
        self.lines.append(line)
        for station in line.route:
            if station.id not in self.stations:
                self.stations[station.id] = station

    def get_transfer_count(self, station_id: int) -> int:
        """Return how many distinct lines pass through a station."""
        count = 0
        for ln in self.lines:
            if any(s.id == station_id for s in ln.route):
                count += 1
        return count

    def is_transfer_station(self, station_id: int) -> bool:
        return self.get_transfer_count(station_id) >= 2

    def get_station_lines(self, station_id: int) -> list[Line]:
        """Return all lines that pass through the given station."""
        return [ln for ln in self.lines
                if any(s.id == station_id for s in ln.route)]


# ---------------------------------------------------------------------------
# Test network
# ---------------------------------------------------------------------------
def _create_test_network() -> MetroNetwork:
    """Create a test metro network with 3 lines, 8 stations.

    Transfer stations:
        S0 (Central)      — 3-line transfer (Lines 1, 2, 3)
        S2 (East Market)  — 2-line transfer (Lines 1, 3)
        S3 (South Gate)   — 2-line transfer (Lines 2, 3)
        S4 (West Lake)    — 2-line transfer (Lines 1, 2)
    """
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

    network = MetroNetwork()
    network.add_line(line1)
    network.add_line(line2)
    network.add_line(line3)
    return network


def main():
    """Launch the interactive metro map."""
    network = _create_test_network()
    from renderer import MetroMapRenderer
    renderer = MetroMapRenderer(network.lines)
    renderer.show()


if __name__ == "__main__":
    main()