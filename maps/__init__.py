"""Map registry for Concept UrbanChain.

Each map is identified by a unique string *key* and holds a
``(display_name, factory)`` pair.  Built-in maps are pre-registered;
additional maps can be added, renamed or removed at runtime via the
functions below --- no restart required.
"""

from maps.test_network import create_test_network
from maps.Beijing_Metro import create_beijing_metro
from maps.Shanghai_Metro import create_shanghai_metro

# --- Registry ---------------------------------------------------------------
# key -> (display_name, factory_function)
MAP_REGISTRY: dict[str, tuple[str, callable]] = {
    "test":     ("Test Network",     create_test_network),
    "beijing":  ("Beijing Metro",    create_beijing_metro),
    "shanghai": ("Shanghai Metro",   create_shanghai_metro),
}


# --- Lookup helpers ---------------------------------------------------------

def get_map(name: str) -> tuple[str, callable] | None:
    """Return ``(display_name, factory)`` for *name*, or *None*."""
    return MAP_REGISTRY.get(name)


def list_maps() -> list[tuple[str, str]]:
    """Return ``[(key, display_name), ...]`` for every registered map."""
    return [(k, v[0]) for k, v in MAP_REGISTRY.items()]


def registered_keys() -> list[str]:
    """Return all registered map keys, insertion-ordered (Python 3.7+)."""
    return list(MAP_REGISTRY.keys())


def is_registered(key: str) -> bool:
    """Return *True* if *key* exists in the registry."""
    return key in MAP_REGISTRY


def default_map_key() -> str:
    """Return the key of the first registered map."""
    return next(iter(MAP_REGISTRY))


# --- Mutation helpers -------------------------------------------------------

def register_map(key: str, display_name: str, factory: callable) -> None:
    """Add (or overwrite) a map entry in the registry.

    Args:
        key: Unique lookup key (e.g. ``"london"``).  If it already exists
             the old entry is silently replaced.
        display_name: Human-readable label shown in menus.
        factory: Zero-argument callable that returns a fresh
                 :class:`~main.MetroNetwork`.
    """
    MAP_REGISTRY[key] = (display_name, factory)


def unregister_map(key: str) -> bool:
    """Remove *key* from the registry.

    Returns:
        *True* if the key was removed; *False* if it was not found.
    """
    if key in MAP_REGISTRY:
        del MAP_REGISTRY[key]
        return True
    return False


def rename_map(key: str, new_display_name: str) -> bool:
    """Change the display name of an existing map.

    Returns:
        *True* if the rename succeeded; *False* if *key* was not found.
    """
    entry = MAP_REGISTRY.get(key)
    if entry is None:
        return False
    MAP_REGISTRY[key] = (new_display_name, entry[1])
    return True


def clear_user_maps() -> int:
    """Remove all maps except the built-in set.

    Built-in keys are ``"test"``, ``"beijing"``, ``"shanghai"``.

    Returns:
        Number of entries removed.
    """
    builtins = {"test", "beijing", "shanghai"}
    to_remove = [k for k in MAP_REGISTRY if k not in builtins]
    for k in to_remove:
        del MAP_REGISTRY[k]
    return len(to_remove)
