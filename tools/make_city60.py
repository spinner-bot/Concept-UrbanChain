# -*- coding: utf-8 -*-
"""Generate data/city60.cuc4m: an irregular ~60-station simulated city.

Deliberately avoids the concentric-ring / radial symmetry of earlier demos:
- off-centre main hub, secondary hubs, uneven station spacing;
- meandering arterials rather than circular rings;
- asymmetric river, hills, parks and city boundary;
- background extends well beyond the station network.
"""

from __future__ import annotations

import math
import random
from pathlib import Path

random.seed(20261126)

HUB = (55.0, 48.0)
HUB2 = (93.0, 49.0)


def dist(a, b):
    return math.hypot(a[0] - b[0], a[1] - b[1])


def point_in_polygon(point, polygon):
    x, y = point
    inside = False
    j = len(polygon) - 1
    for i in range(len(polygon)):
        xi, yi = polygon[i]
        xj, yj = polygon[j]
        if ((yi > y) != (yj > y)) and (
            x < (xj - xi) * (y - yi) / (yj - yi + 1e-12) + xi
        ):
            inside = not inside
        j = i
    return inside


def segments_intersect(a, b, c, d):
    def orient(p, q, r):
        return (q[0] - p[0]) * (r[1] - p[1]) - (q[1] - p[1]) * (r[0] - p[0])

    return (orient(a, b, c) * orient(a, b, d) < 0 and
            orient(c, d, a) * orient(c, d, b) < 0)


class City:
    def __init__(self):
        self.nodes = []
        self.node_by_key = {}
        self.edges = []
        self.roads = []  # (name, width, points, stationized)
        self.river = [(-24, 22), (4, 29), (22, 38), (40, 52), (58, 63),
                      (76, 71), (96, 79), (118, 85), (142, 92), (166, 99)]
        self.hills = [(104, 12), (148, 18), (158, 52), (146, 86),
                      (118, 94), (104, 68), (100, 34)]
        self.lake = [(18, 86), (26, 100), (44, 104), (52, 92),
                     (44, 78), (26, 76)]
        self.parks = {
            "RiversidePark": [(46, 76), (60, 80), (64, 92), (50, 96), (38, 88)],
            "EastPark": [(100, 54), (114, 50), (122, 60), (112, 68), (98, 64)],
            "HillPark": [(118, 70), (130, 72), (136, 82), (126, 88), (114, 82)],
        }
        self.hospital = [(78, 60), (88, 58), (91, 66), (81, 69)]

    def add_node(self, point, name):
        key = (round(point[0], 3), round(point[1], 3))
        if key in self.node_by_key:
            return self.node_by_key[key]
        index = len(self.nodes)
        self.nodes.append({"point": point, "name": name})
        self.node_by_key[key] = index
        return index

    def add_road(self, prefix, points, width, stationize=True, short=None):
        indices = []
        for i, point in enumerate(points):
            indices.append(self.add_node(point, f"{short or prefix}{i + 1}"))
        self.roads.append((prefix, width, points, stationize))
        for i in range(len(points) - 1):
            a, b = indices[i], indices[i + 1]
            if a != b:
                self.edges.append((a, b))
        return indices

    def add_background_road(self, prefix, points, width):
        self.roads.append((prefix, width, points, False))

    def extend(self, points, prefix, width):
        if len(points) < 2:
            return
        def outward(a, b, length):
            dx, dy = b[0] - a[0], b[1] - a[1]
            norm = math.hypot(dx, dy) or 1.0
            return (b[0] + dx / norm * length, b[1] + dy / norm * length)

        head = outward(points[1], points[0], 14.0)
        tail = outward(points[-2], points[-1], 18.0)
        self.add_background_road(prefix + "ExtA", [head, points[0]], width)
        self.add_background_road(prefix + "ExtB", [points[-1], tail], width)

    def unit_cost(self, a, b):
        pa = self.nodes[a]["point"]
        pb = self.nodes[b]["point"]
        mid = ((pa[0] + pb[0]) / 2.0, (pa[1] + pb[1]) / 2.0)
        value = 1.0 + random.uniform(-0.07, 0.07)
        if point_in_polygon(mid, self.hills):
            value = 1.32 + random.uniform(-0.04, 0.08)
        for i in range(len(self.river) - 1):
            if segments_intersect(pa, pb, self.river[i], self.river[i + 1]):
                value = 1.62 + random.uniform(-0.06, 0.10)
                break
        return max(0.9, value)

    def build(self):
        # Main arterials: meandering, unevenly spaced, no concentric rings.
        grand = [(2, 56), (22, 59), (46, 55), HUB,
                 (68, 51), (83, 57), (97, 63), (116, 61), (134, 67)]
        market = [(48, 4), (52, 16), (46, 30), HUB,
                  (60, 62), (57, 76), (64, 90), (69, 104), (74, 120)]
        diagonal = [(24, 96), (38, 84), (52, 72), (62, 66),
                    (78, 58), (93, 49), (108, 40), (140, 22)]
        riverside = [(0, 30), (14, 36), (44, 58), (58, 66),
                     (74, 74), (90, 79), (108, 86), (142, 98)]
        north = [(16, 118), (52, 116), (68, 108),
                 (84, 112), (102, 106), (138, 104)]

        self.add_road("GrandAve", grand, 2.6, short="G")
        self.add_road("MarketSt", market, 2.4, short="M")
        self.add_road("DiagAve", diagonal, 2.2, short="D")
        self.add_road("Riverside", riverside, 2.0, short="R")
        self.add_road("NorthRidge", north, 1.8, short="N")

        # Asymmetric loops and connectors.
        self.add_road("WestLoop", [(14, 18), (12, 50), (10, 68),
                                   (18, 84), (31, 84), (38, 84)], 1.5, short="W")
        self.add_road("EastLoop", [HUB2, (110, 42), (134, 64),
                                   (129, 80), (116, 84), (108, 86)], 1.5, short="E")
        self.add_road("DockRd", [(40, 18), (52, 16), (80, 32), HUB2], 1.6, short="K")
        self.add_road("HillRd", [HUB2, (128, 66), (136, 78), (142, 90)], 1.3, short="H")
        self.add_road("NorthLink", [(74, 120), (84, 112)], 1.2, short="Y")
        self.add_road("OldTown", [(20, 80), (28, 74), (38, 70), (44, 58)], 1.1, short="O")
        self.add_road("DockLane", [(74, 30), (82, 38), HUB2], 1.0, short="L")
        self.add_road("ParkRd", [(68, 51), (74, 60), (78, 72)], 1.1, short="P")
        self.add_road("HospitalRd", [(83, 57), (86, 64), (90, 79)], 1.2, short="U")
        self.add_road("BridgeRd", [(34, 24), (40, 38), (46, 55)], 1.7, short="B")
        self.add_road("EastBridge", [(90, 79), (102, 88), (116, 94)], 1.6, short="X")

        # Background-only approaches: roads continue past the outer stations.
        for prefix, points, width in [
            ("GrandAve", grand, 2.6),
            ("MarketSt", market, 2.4),
            ("DiagAve", diagonal, 2.2),
            ("NorthRidge", north, 1.8),
        ]:
            self.extend(points, prefix, width)
        self.add_background_road("WestEndRd", [(10, 68), (-4, 72), (-18, 78)], 1.3)
        self.add_background_road("SouthDockRd", [(40, 18), (34, 8), (28, -4)], 1.3)

        # Irregular local streets: short links between nearby stations,
        # creating blocks and cycles instead of a tree-like network.
        self.add_local_links(limit=16)

    def add_local_links(self, limit):
        existing = {(min(a, b), max(a, b)) for a, b in self.edges}
        degree = [0] * len(self.nodes)
        for a, b in self.edges:
            degree[a] += 1
            degree[b] += 1
        candidates = []
        for i in range(len(self.nodes)):
            for j in range(i + 1, len(self.nodes)):
                d = dist(self.nodes[i]["point"], self.nodes[j]["point"])
                if 6.0 <= d <= 13.0 and (i, j) not in existing:
                    candidates.append((d, i, j))
        candidates.sort()
        added = 0
        for d, i, j in candidates:
            if added >= limit:
                break
            if degree[i] >= 4 or degree[j] >= 4:
                continue
            key = (min(i, j), max(i, j))
            if key in existing:
                continue
            self.edges.append((i, j))
            existing.add(key)
            degree[i] += 1
            degree[j] += 1
            self.add_background_road(
                f"Local{added + 1}",
                [self.nodes[i]["point"], self.nodes[j]["point"]],
                1.0,
            )
            added += 1

    def write(self, path):
        lines = ["CUC4M 1"]
        region = [(-28, -22), (30, -30), (92, -24), (148, -14), (168, 26),
                  (164, 78), (156, 132), (104, 146), (44, 150), (-10, 138),
                  (-24, 94), (-30, 38)]
        features = [
            ("region", True, "City", 0.0, region),
            ("water", True, "Lake", 0.0, self.lake),
        ]
        for name, poly in self.parks.items():
            features.append(("park", True, name, 0.0, poly))
        features.append(("hospital", True, "Hospital", 0.0, self.hospital))
        features.append(("river", False, "River", 3.4, self.river))
        features.append(("mountain", True, "Hills", 0.0, self.hills))
        for name, width, points, _ in self.roads:
            features.append(("road", False, name, width, points))
        features.append(("boundary", False, "DistrictLine", 0.9,
                         [(-24, 94), (-10, 138), (44, 150)]))

        lines.append(f"BACKGROUND {len(features)}")
        for kind, closed, name, width, points in features:
            body = " ".join(f"{p[0]:.3f} {p[1]:.3f}" for p in points)
            lines.append(f"BG {kind} {1 if closed else 0} {name} {width:.3f} {body}")

        for node in self.nodes:
            x, y = node["point"]
            lines.append(f"STATION {node['name']} {x:.3f} {y:.3f} {node['name']}")

        seen = set()
        for a, b in self.edges:
            key = (min(a, b), max(a, b))
            if key in seen:
                continue
            seen.add(key)
            pa = self.nodes[a]["point"]
            pb = self.nodes[b]["point"]
            lines.append(
                f"EDGE {self.nodes[a]['name']} {self.nodes[b]['name']} "
                f"{self.unit_cost(a, b):.4f} {dist(pa, pb):.4f}"
            )
        lines.append("END")

        # Connectivity check before writing.
        parent = list(range(len(self.nodes)))

        def find(x):
            while parent[x] != x:
                parent[x] = parent[parent[x]]
                x = parent[x]
            return x

        for a, b in self.edges:
            ra, rb = find(a), find(b)
            if ra != rb:
                parent[rb] = ra
        components = len({find(i) for i in range(len(self.nodes))})

        Path(path).write_text("\n".join(lines) + "\n", encoding="utf-8")
        print(f"wrote {path}: {len(self.nodes)} stations, "
              f"{len(seen)} edges, {components} component(s)")


def main():
    city = City()
    city.build()
    out = Path(__file__).resolve().parent.parent / "data" / "city60.cuc4m"
    city.write(out)


if __name__ == "__main__":
    main()
