from enum import Enum


class StationType(Enum):
    """Station construction type."""
    UNDERGROUND = "underground"
    GROUND = "ground"
    ELEVATED = "elevated"
    BUILDING = "building"
    STRUCTURE = "structure"

    @property
    def display_name(self) -> str:
        from lang import t
        return t(self.value)


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

    # -- serialisation -------------------------------------------------------

    def to_dict(self) -> dict:
        """Encode the station as a JSON-serialisable dict."""
        return {
            "id": self.id,
            "name": self.name,
            "position": list(self.position),
            "station_type": self.station_type.value,
        }

    @classmethod
    def from_dict(cls, d: dict) -> "Station":
        """Decode a station from a dict produced by :meth:`to_dict`."""
        return cls(
            id=d["id"],
            name=d["name"],
            position=tuple(d["position"]),
            station_type=StationType(d["station_type"]),
        )

from spline import line_identifier  # noqa: E402 — used by renderer via spline


class Line:
    def __init__(self, id: int, name: str, route: list[Station | int], max_speed: float,
                 color: tuple = (255, 0, 0),
                 fine_trajectory: list = None,
                 smooth_tension: float = 0.5,
                 hide_terminal_label: bool = False,
                 ring_label_station_id: int = None):
        self.id = id
        self.name = name
        self.route = route
        self.max_speed = max_speed
        self.color = color  # RGB tuple (0-255)
        # Catmull-Rom alpha: 0=uniform, 0.5=centripetal (default), 1=chordal
        self.smooth_tension = smooth_tension
        self.hide_terminal_label = hide_terminal_label
        # For circular lines: station id where the single label is placed
        self.ring_label_station_id = ring_label_station_id
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

    # -- serialisation -------------------------------------------------------

    def to_dict(self) -> dict:
        """Encode the line as a JSON-serialisable dict.

        Station references in *route* are written as their integer IDs.
        """
        route_ids = [
            s.id if isinstance(s, Station) else s for s in self.route
        ]
        return {
            "id": self.id,
            "name": self.name,
            "route": route_ids,
            "max_speed": self.max_speed,
            "color": list(self.color),
            "fine_trajectory": [
                [list(pt) for pt in seg] for seg in self.fine_trajectory
            ],
            "smooth_tension": self.smooth_tension,
            "hide_terminal_label": self.hide_terminal_label,
            "ring_label_station_id": self.ring_label_station_id,
        }

    @classmethod
    def from_dict(cls, d: dict, stations: dict[int, "Station"]) -> "Line":
        """Decode a line from a dict produced by :meth:`to_dict`.

        Args:
            d: The dict representation.
            stations: ``{id: Station}`` lookup used to resolve route IDs.
        """
        route = [stations[rid] for rid in d["route"]]
        ft_raw = d.get("fine_trajectory", [])
        fine_trajectory = [
            [tuple(pt) for pt in seg] for seg in ft_raw
        ]
        return cls(
            id=d["id"],
            name=d["name"],
            route=route,
            max_speed=d["max_speed"],
            color=tuple(d.get("color", (255, 0, 0))),
            fine_trajectory=fine_trajectory,
            smooth_tension=d.get("smooth_tension", 0.5),
            hide_terminal_label=d.get("hide_terminal_label", False),
            ring_label_station_id=d.get("ring_label_station_id"),
        )


class MetroNetwork:
    """Container for all metro lines and stations."""

    # Increment when the save format changes in a breaking way.
    FORMAT_VERSION = 1

    def __init__(self):
        self.lines: list[Line] = []
        self.stations: dict[int, Station] = {}

    def add_line(self, line: Line) -> None:
        # Resolve int IDs to Station objects
        for i, item in enumerate(line.route):
            if isinstance(item, int):
                if item not in self.stations:
                    raise KeyError(
                        f"Station ID {item} not found in network. "
                        "Add the station first or ensure it exists."
                    )
                line.route[i] = self.stations[item]
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

    @staticmethod
    def is_circular(line: "Line") -> bool:
        """Return True if the line is a circle (first and last station same)."""
        return len(line.route) >= 2 and line.route[0].id == line.route[-1].id

    # -- serialisation -------------------------------------------------------

    def to_dict(self) -> dict:
        """Encode the full network as a JSON-serialisable dict.

        Returns a dict with ``"format_version"``, ``"stations"`` and
        ``"lines"`` keys suitable for :meth:`from_dict`.
        """
        return {
            "format_version": self.FORMAT_VERSION,
            "stations": {
                str(sid): s.to_dict() for sid, s in self.stations.items()
            },
            "lines": [ln.to_dict() for ln in self.lines],
        }

    @classmethod
    def from_dict(cls, d: dict) -> "MetroNetwork":
        """Decode a network from a dict produced by :meth:`to_dict`.

        Supports future format versions by inspecting ``"format_version"``.
        """
        version = d.get("format_version", 1)

        net = cls()
        # Rebuild stations first so lines can reference them.
        for sid_str, sdata in d["stations"].items():
            st = Station.from_dict(sdata)
            net.stations[st.id] = st

        if version == 1:
            for ldata in d["lines"]:
                net.add_line(Line.from_dict(ldata, net.stations))
        else:
            raise ValueError(f"Unsupported format version: {version}")

        return net


def main(map_key: str | None = None):
    """Launch the interactive metro map.

    Args:
        map_key: Key of the map to load from the registry.
                 If None, the default map is used.
    """
    from maps import get_map, default_map_key

    if map_key is None:
        map_key = default_map_key()

    entry = get_map(map_key)
    if entry is None:
        available = ", ".join(k for k, _ in __import__("maps").list_maps())
        raise KeyError(
            f"Map '{map_key}' not found. Available: {available}"
        )

    display_name, factory = entry
    print(f"Loading map: {display_name}")
    network = factory()

    from renderer import MetroMapRenderer
    renderer = MetroMapRenderer(network.lines, map_key=map_key,
                                 network=network)
    renderer.show()


if __name__ == "__main__":
    main()