from enum import Enum
from PySide6.QtCore import QObject, Signal

class CmdType(Enum):
    def __str__(self):
        if self == self.GET:
            return "GET"
        if self == self.PUT:
            return "PUT"
        return ""
    GET = 1
    PUT = 2

class CmdField(Enum):
    def __str__(self):
        if self == self.RGB:
            return "rgb"
        if self == self.LOC:
            return "loc"
        if self == self.LEVEL:
            return "level"
        if self == self.ONOFF:
            return "onoff"
        return ""
    RGB = 1
    LOC = 2
    LEVEL = 3
    ONOFF = 4

class Location:
    north_limits = (-32768, 32767)
    east_limits = (-32768, 32767)
    def __init__(self, north, east, altitude, floor, uncertainty):
        self.north = int(north)
        self.east = int(east)
        self.altitude = int(altitude)
        self.floor = int(floor)
        self.uncertainty = int(uncertainty)
    def __str__(self):
        return f"N{int(self.north):05}E{int(self.east):05}A{int(self.altitude):05}F{int(self.floor):03}U{int(self.uncertainty):05}"
    def parse(value: str):
        north_idx = value.find("N")
        east_idx = value.find("E")
        altitude_idx = value.find("A")
        floor_idx = value.find("F")
        uncertainty_idx = value.find("U")
        indices = [north_idx, east_idx, altitude_idx, floor_idx, uncertainty_idx]
        if -1 in indices:
            print("Error: could not parse location value - some part is missing")
        for i in range(len(indices)-1):
            if not (indices[i] < indices[i+1]):
                print("Error: could not parse location value")
                return None
        north = int(value[north_idx+1:east_idx])
        east = int(value[east_idx+1:altitude_idx])
        alt = int(value[altitude_idx+1:floor_idx])
        floor = int(value[floor_idx+1:uncertainty_idx])
        uncertainty = int(value[uncertainty_idx+1:])
        return Location(north,east,alt,floor,uncertainty)

class Node:
    def __init__(self, addr, loc = None, rgb = None, level = None, onoff = None):
        self.addr = addr
        self.loc = loc
        self.rgb = rgb
        self.level = level
        self.onoff = onoff
    def __str__(self):
        return f"A:{self.addr}\nC:{self.rgb}\nL:{self.level}\nx={self.loc.north if self.loc is not None else '?'} y={self.loc.east if self.loc is not None else '?'}"

class SerialComm(QObject):

    node_change = Signal(Node)

    def __init__(self):
        self.cache = {}
        self.local_addr = None
        super().__init__()
    
    def parse_response(self, rsp):
        if isinstance(rsp, bytes):
            rsp = rsp.decode("utf-8")
        if not isinstance(rsp, str):
            print(f"Error: bad type of response expected bytes or str (actual={type(rsp)})")
            sys.exit(1)
        rsp = rsp.strip()
        values = rsp.split('=')
        if len(values) != 2:
            print("Error: cannot parse response")
            return
        field,value = values[0], values[1]
        addr = None
        if ':' in field:
            addr, field = field.split(':', 1)
        print(f"{addr=} {field=} {value=}")
        success = False
        if self.local_addr is not None and addr == self.local_addr:
            print("skipping local device")
            return
        if addr not in self.cache:
            self.cache[addr] = Node(addr)
        if field == "loc":
            loc = Location.parse(value)
            if loc is None:
                print("Not saving the value")
                return
            self.cache[addr].loc = loc
            success = True
        elif field == "rgb":
            self.cache[addr].rgb = value
            success = True
        elif field == "level":
            self.cache[addr].level = value
            success = True
        if success:
            print(f"node {addr} changed")
            self.node_change.emit(self.cache[addr])
    
    def format_cmd(cmd_type: CmdType, addr: str, field: CmdField, value):
        cmd_type_str = str(cmd_type)
        if cmd_type_str is None or len(cmd_type_str) == 0:
            print("Error, unknown cmd type")
            return None
        field_str = str(field)
        if field_str is None or len(field_str) == 0:
            print("Error, unknown field")
            return None
        value_str = str(value)
        if cmd_type == CmdType.PUT and len(value_str) == 0:
            print("Error, value cannot be empty for cmd PUT")
            return None
        # format values
        if cmd_type == CmdType.PUT:
            if field == CmdField.ONOFF:
                if isinstance(value, bool):
                    value_str = "ON" if value else "OFF"
        return f"{cmd_type_str} {addr + ':' + field_str if addr is not None else field_str}{' ' + value_str if cmd_type == CmdType.PUT else ''}\n".encode()