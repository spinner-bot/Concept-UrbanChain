"""Bilingual language system — Chinese (default) / English.

Press L to toggle language at runtime.
"""

STRINGS = {
    "zh": {
        "net_btn": "线网 >",
        "fold": "[_]",
        "expand": "[+]",
        "full_fold": "[-]",
        "back": "< 返回",
        "close": "X 关闭",
        "station_type_label": "类型",
        "position_label": "坐标",
        "lines_label": "线路",
        "route_label": "走向",
        "stations_label": "站数",
        "length_label": "长度",
        "max_speed_label": "最高速度",
        "circular_label": "（环线）",
        "transfer_label": "换乘",
        "line_network_title": "线网总览",
        "show_dev_data": "▶ 显示开发数据",
        "hide_dev_data": "▼ 隐藏开发数据",
        "terminal_theory": "理论终端",
        "cal_samples": "校准次数",
        "underground": "地下站",
        "ground": "地面站",
        "elevated": "地上站",
        "building": "建筑站",
        "structure": "结构站",
        "line_unnamed": "地铁{id}号线",
    },
    "en": {
        "net_btn": "Net >",
        "fold": "[_]",
        "expand": "[+]",
        "full_fold": "[-]",
        "back": "< Back",
        "close": "X Close",
        "station_type_label": "Type",
        "position_label": "Position",
        "lines_label": "Lines",
        "route_label": "Route",
        "stations_label": "Stns",
        "length_label": "Length",
        "max_speed_label": "Max speed",
        "circular_label": "(circular)",
        "transfer_label": "Transfer",
        "line_network_title": "Line Network",
        "show_dev_data": "> Show dev data",
        "hide_dev_data": "v Hide dev data",
        "terminal_theory": "Theory terminal",
        "cal_samples": "Cal samples",
        "underground": "Underground",
        "ground": "Ground",
        "elevated": "Elevated",
        "building": "Building",
        "structure": "Structure",
        "line_unnamed": "Metro Line {id}",
    },
}

_current_lang = "zh"


def t(key: str, **kwargs) -> str:
    """Return the translated string for *key* in the current language."""
    s = STRINGS.get(_current_lang, {}).get(key, key)
    if kwargs:
        s = s.format(**kwargs)
    return s


def set_language(lang: str):
    global _current_lang
    if lang in STRINGS:
        _current_lang = lang


def toggle_language():
    global _current_lang
    _current_lang = "en" if _current_lang == "zh" else "zh"


def current_language() -> str:
    return _current_lang
