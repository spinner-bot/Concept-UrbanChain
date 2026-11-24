"""Application configuration for Concept UrbanChain.

Defines the ``AppConfig`` dataclass that drives the simple / full UI mode
switch and persists user preferences.
"""

from dataclasses import dataclass, field
from pathlib import Path


@dataclass
class AppConfig:
    """User-facing application settings.

    Attributes:
        mode: UI mode — ``"simple"`` (full-screen map, minimal chrome)
              or ``"full"`` (side panels, search, advanced tools).
        language: ``"zh"`` or ``"en"``.
        dark_mode: *True* for dark colour scheme.
        scale_factor: Visual scale of station / line elements (0.3–3.0).
        last_map_key: Most-recently loaded map key.
        show_legend: Whether the legend panel is visible (full mode).
        show_status_bar: Whether the status bar is visible.
        show_dev_tools: Developer panel with internal data display.
    """

    mode: str = "simple"
    language: str = "zh"
    dark_mode: bool = False
    scale_factor: float = 1.0
    last_map_key: str = ""

    # Full-mode panels
    show_legend: bool = True
    show_status_bar: bool = True
    show_dev_tools: bool = False


# Global singleton — mutated at runtime.
config = AppConfig()


def reset_config() -> None:
    """Restore all settings to their defaults."""
    global config
    config = AppConfig()
