"""Map registry for Concept UrbanChain.

Each map is a Python module in this package that exposes a single
factory function returning a MetroNetwork instance.

To add a new map:
    1. Create a .py file in this folder with a factory function.
    2. Register it in MAP_REGISTRY below.
"""

from maps.test_network import create_test_network

# Registry:  key → (display_name, factory_function)
MAP_REGISTRY: dict[str, tuple[str, callable]] = {
    "test": ("Test Network", create_test_network),
}


def get_map(name: str) -> tuple[str, callable] | None:
    """Return (display_name, factory) for a registered map, or None."""
    return MAP_REGISTRY.get(name)


def list_maps() -> list[tuple[str, str]]:
    """Return [(key, display_name), ...] for all registered maps."""
    return [(k, v[0]) for k, v in MAP_REGISTRY.items()]


def default_map_key() -> str:
    """Return the key of the default map to load on startup."""
    return next(iter(MAP_REGISTRY))
