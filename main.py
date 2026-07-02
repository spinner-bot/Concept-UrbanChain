class Station:
    def __init__(self, id: int, name: str, position: tuple):
        self.id = id
        self.name = name
        self.position = position # 3维坐标，兼容2维（第3维作为扩展功能）

class Line:
    def __init__(self, id: int, name: str, route: list, max_speed: float):
        self.id = id
        self.name = name
        self.route = route
        self.max_speed = max_speed 