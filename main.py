class Station:
    def __init__(self, id: int, name: str, position: tuple):
        self.id = id
        self.name = name
        self.position = position # 3维坐标，兼容2维（第3维作为扩展功能）

class Line:
    def __init__(self, id: int, name: str, route: list, max_speed: float,
                 color: tuple = (255, 0, 0),
                 fine_trajectory: list = None,
                 smooth_tension: float = 0.5):
        self.id = id
        self.name = name
        self.route = route
        self.max_speed = max_speed
        self.color = color  # RGB tuple (0-255)
        # Catmull-Rom alpha: 0=uniform, 0.5=centripetal (default), 1=chordal
        self.smooth_tension = smooth_tension
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


def main():
    """Minimal test — will be expanded in a later commit."""
    # Create a few stations
    s0 = Station(0, "Central",   (0, 0))
    s1 = Station(1, "North",     (0, 5))
    s2 = Station(2, "East",      (5, 0))
    s3 = Station(3, "South",     (0, -5))
    s4 = Station(4, "West",      (-5, 0))

    # Line 0: North-South (Red)
    line0 = Line(0, "Vertical", route=[s1, s0, s3], max_speed=80,
                 color=(220, 50, 50))
    # Line 1: East-West (Blue)
    line1 = Line(1, "Horizontal", route=[s4, s0, s2], max_speed=80,
                 color=(50, 100, 220))

    network = MetroNetwork()
    network.add_line(line0)
    network.add_line(line1)

    from renderer import MetroMapRenderer
    renderer = MetroMapRenderer(network.lines)
    renderer.show()


if __name__ == "__main__":
    main()