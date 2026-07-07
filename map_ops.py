"""Map-level operations: copy, merge and split metro networks.

All functions in this module return new :class:`~main.MetroNetwork`
instances — the originals are never mutated.
"""

from main import MetroNetwork, Line, Station


# ---------------------------------------------------------------------------
# Copy
# ---------------------------------------------------------------------------

def copy_network(network: MetroNetwork) -> MetroNetwork:
    """Create a deep, independent copy of *network*.

    The copy shares no mutable state with the original; edits to one
    will not affect the other.

    This is implemented via the standard to_dict / from_dict round-trip,
    which is both simple and exhaustive.
    """
    return MetroNetwork.from_dict(network.to_dict())


# ---------------------------------------------------------------------------
# Merge
# ---------------------------------------------------------------------------

def _next_id(existing: set[int], start: int = 0) -> int:
    """Smallest integer >= *start* not in *existing*."""
    while start in existing:
        start += 1
    return start


def merge_networks(*networks: MetroNetwork,
                   renumber: bool = True) -> MetroNetwork:
    """Merge two or more networks into a single new one.

    Args:
        networks:  The networks to combine.
        renumber:  If *True* (default), station and line IDs are
                   renumbered to avoid collisions.  If *False*, conflicts
                   cause stations with identical IDs to be treated as the
                   same station (the first occurrence wins).

    Returns:
        A new :class:`MetroNetwork` containing all stations and lines.

    Raises:
        ValueError: if fewer than two networks are supplied.
    """
    if len(networks) < 2:
        raise ValueError("Provide at least two networks to merge.")

    if not renumber:
        merged = MetroNetwork()
        for net in networks:
            for st in net.stations.values():
                if st.id not in merged.stations:
                    merged.stations[st.id] = Station(
                        id=st.id, name=st.name,
                        position=st.position, station_type=st.station_type,
                    )
            for ln in net.lines:
                merged.add_line(Line(
                    id=ln.id, name=ln.name,
                    route=[s.id for s in ln.route],
                    max_speed=ln.max_speed,
                    color=ln.color,
                    fine_trajectory=ln.fine_trajectory,
                ))
        return merged

    # Renumbering path
    merged = MetroNetwork()
    id_map: dict[tuple[int, int], int] = {}   # (src_net_index, old_id) -> new_id
    all_station_ids: set[int] = set()

    # Pass 1 — assign new station IDs.
    for i, net in enumerate(networks):
        for old_id, st in net.stations.items():
            new_id = _next_id(all_station_ids, 1)
            id_map[(i, old_id)] = new_id
            all_station_ids.add(new_id)
            merged.stations[new_id] = Station(
                id=new_id, name=st.name,
                position=st.position, station_type=st.station_type,
            )

    # Pass 2 — add lines with remapped IDs.
    all_line_ids: set[int] = set()
    for i, net in enumerate(networks):
        for ln in net.lines:
            new_line_id = _next_id(all_line_ids, 1)
            all_line_ids.add(new_line_id)
            new_route_ids = [id_map[(i, s.id)] for s in ln.route]
            merged.add_line(Line(
                id=new_line_id, name=ln.name,
                route=new_route_ids,
                max_speed=ln.max_speed,
                color=ln.color,
                fine_trajectory=ln.fine_trajectory,
            ))

    return merged


# ---------------------------------------------------------------------------
# Split
# ---------------------------------------------------------------------------

def split_network(network: MetroNetwork,
                  line_ids: list[int]) -> tuple[MetroNetwork, MetroNetwork]:
    """Split *network* into two independent networks.

    Args:
        network:   The source network.
        line_ids:  IDs of lines that go into the **first** (extracted)
                   network.  All remaining lines go into the second.

    Returns:
        ``(extracted_network, remainder_network)``.  Station IDs are
        preserved; a station appears in a result only if it is used by
        at least one line in that result.
    """
    extracted = MetroNetwork()
    remainder = MetroNetwork()

    line_id_set = set(line_ids)

    for ln in network.lines:
        target = extracted if ln.id in line_id_set else remainder
        # Copy referenced stations if not already present.
        for st in ln.route:
            if st.id not in target.stations:
                target.stations[st.id] = Station(
                    id=st.id, name=st.name,
                    position=st.position, station_type=st.station_type,
                )
        target.add_line(Line(
            id=ln.id, name=ln.name,
            route=[s.id for s in ln.route],
            max_speed=ln.max_speed,
            color=ln.color,
            fine_trajectory=ln.fine_trajectory,
        ))

    return extracted, remainder
