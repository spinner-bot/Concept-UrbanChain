class Station:
    def __init__(self, id: int, name: str, position: tuple):
        self.id = id
        self.name = name
        self.position = position # 3维坐标，兼容2维（第3维作为扩展功能）

class Line:
    def __init__(self, id: int, name: str, route: list, max_speed: float,
                 color: tuple = (255, 0, 0),
                 fine_trajectory: list = None):
        self.id = id
        self.name = name
        self.route = route
        self.max_speed = max_speed
        self.color = color  # RGB tuple (0-255)
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