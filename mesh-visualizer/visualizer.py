#!/usr/bin/env python3

import serial
import sys
import time
import numpy as np
import cv2
import pygraphviz as pgv

def print_usage():
    print("helper.py <serial_port>")

def MaximizeWithAspectRatio(image, max_width=None, max_height=None, inter=cv2.INTER_AREA):
    dim = None
    (h, w) = image.shape[:2]

    if max_width is None and max_height is None:
        return image
    if max_width is None:
        r = max_height / float(h)
        dim = (int(w * r), max_height)
    elif max_height is None:
        r = max_width / float(w)
        dim = (max_width, int(h * r))
    else:
        if h > w:
            r = max_height / float(h)
            dim = (int(w * r), max_height)
        else:
            r = max_width / float(w)
            dim = (max_width, int(h * r))
    return cv2.resize(image, dim, interpolation=inter)

class Location:
    def __init__(self, north, east, altitude, floor, uncertainty):
        self.north = north
        self.east = east
        self.altitude = altitude
        self.floor = floor
        self.uncertainty = uncertainty
    def __str__(self):
        return f"{self.north=} {self.east=} {self.altitude=} {self.floor=} {self.uncertainty=}"

class Node:
    def __init__(self, addr):
        self.addr = addr
        self.loc = None
        self.rgb = None
        self.level = None
    def __str__(self):
        return f"{{A:{self.addr}|C:{self.rgb}|L:{self.level}|x={self.loc.north * Helper.pos_scale if self.loc is not None else "?"} y={self.loc.east * Helper.pos_scale if self.loc is not None else "?"}}}"

class Helper:
    pos_scale = 1
    def __init__(self, serial_port):
        self.ser = serial.Serial(serial_port, 115200, timeout=0)
        self.cache = {}
        self.local_addr = None

    def parse_loc_value(self, value):
        if len(value) != 28:
            print(f"Error: unexpected location value - expected length 28, got {len(value)}")
            return None,None,None,None,None
        north = int(value[1:6])
        east = int(value[7:12])
        alt = int(value[13:18])
        floor = int(value[19:22])
        uncertainty = int(value[23:])
        return north,east,alt,floor,uncertainty

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
            addr, field = field.split(':')
        print(f"{addr=} {field=} {value=}")
        if self.local_addr is not None and addr == self.local_addr:
            print("skipping local device")
            return
        if addr not in self.cache:
            self.cache[addr] = Node(addr)
        if field == "loc":
            north,east,alt,floor,uncertainty = self.parse_loc_value(value)
            if None in (north,east,alt,floor,uncertainty):
                print("Not saving the value")
                return
            self.cache[addr].loc = Location(north,east,alt,floor,uncertainty)
            print("Saved succesfully")
        elif field == "rgb":
            self.cache[addr].rgb = value
            print("Saved succesfully")
        elif field == "level":
            self.cache[addr].level = value
            print("Saved succesfully")

    def read_responses(self):
        print(f"{self.ser.in_waiting=}")
        while self.ser.in_waiting > 0:
            print(f"{self.ser.in_waiting=}")
            line = self.ser.readline()
            print(f"{line=} {len(line)=}")
            if len(line) == 0:
                break
            self.parse_response(line)
    
    def show(self):
        # create graph
        G = pgv.AGraph(notranslate="true", inputscale=72, size="16,9!", dpi=100)
        for key in self.cache.keys():
            if isinstance(self.cache[key].loc, Location):
                print(self.cache[key].loc)
                pos = f"{self.cache[key].loc.north * self.pos_scale},{self.cache[key].loc.east * self.pos_scale}"
            G.add_node(key, pos=pos, label = str(self.cache[key]), shape="record")
        
        G.layout("neato", args="-n2")

        # create image
        img_bytes = G.draw(format="png")
        nparr = np.fromstring(img_bytes, np.uint8)
        img_np = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
        # resized = MaximizeWithAspectRatio(img_np, max_width=1280, max_height=720)
        
        # show image
        cv2.imshow("image", img_np)
        cv2.waitKey(1)

    def main_task(self):
        for _ in range(10):
            self.ser.write(b"GET addr\n")
            time.sleep(1)
            while self.ser.in_waiting > 0:
                rsp = self.ser.readline()
                rsp = rsp.decode("utf-8")
                field,value = rsp.split('=')
                if field == "addr":
                    self.local_addr = value.strip()
                    break
            if self.local_addr is not None:
                break
        while True:
            self.read_responses()
            self.show()
            self.ser.write(b"GET ffff:loc\n")
            self.ser.write(b"GET ffff:rgb\n")
            self.ser.write(b"GET ffff:level\n")
            # tune this value if Ble-mesh network becomes overwhelmed
            # the network becomes overwhelmed when there are a lot of
            # devices and relay is enabled
            time.sleep(5)

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print_usage()
        sys.exit(1)
    helper = Helper(sys.argv[1])
    helper.main_task()