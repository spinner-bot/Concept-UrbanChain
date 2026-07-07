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
        # Map menu
        "map_menu_title": "地图列表 — 按 M 关闭",
        "new_map_btn": "+  新建地图",
        "new_map_default_name": "新地图",
        # Edit mode
        "edit_mode_on": "[编辑模式] E=退出 N=加站 Del=删除 Shift+S=保存",
        "edit_mode_new_station": "新站{id}",
        "central_station": "中心站",
        # Archive
        "save_ok": "已保存",
        "export_menu": "导出",
        "import_menu": "导入",
        # UI modes
        "mode_simple": "简约版",
        "mode_full": "完全版",
        "switch_mode": "切换界面模式",
        # Search
        "search_placeholder": "搜索站点或线路...",
        "no_results": "无结果",
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
        # Map menu
        "map_menu_title": "Maps — press M to close",
        "new_map_btn": "+  New Map",
        "new_map_default_name": "New Map",
        # Edit mode
        "edit_mode_on": "[Edit] E=exit N=add Del=delete Shift+S=save",
        "edit_mode_new_station": "Stn {id}",
        "central_station": "Central",
        # Archive
        "save_ok": "Saved",
        "export_menu": "Export",
        "import_menu": "Import",
        # UI modes
        "mode_simple": "Simple",
        "mode_full": "Full",
        "switch_mode": "Switch UI Mode",
        # Search
        "search_placeholder": "Search station or line...",
        "no_results": "No results",
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
