from enum import Enum

class SimulationStatus(Enum):
    CONNECTED = 0
    RUNNING = 1
    FINISHED = 2
    TIMEOUT = 4
