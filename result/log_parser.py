import sys
from enum import Enum
import re
import json
import argparse

parser = argparse.ArgumentParser(description="Process logs from devices using IMF framework")
parser.add_argument("input_file", type=str, help="input log file")
parser.add_argument("output_file", type=str, help="output file (json)")
parser.add_argument("-p", "--parser", dest="parser", choices=["mlat", "colors"], 
                    default="mlat", help="which parser to use")

class LogParser:
    line_regex = re.compile(r"^.\|. \((\d+)\) (\w+): (.*)[\n\r]*$")

class MlatDataEncoder(json.JSONEncoder):
    def default(self, obj):
        if isinstance(obj, MlatAnchor):
            return {"x":obj.x, "y": obj.y, "dist": obj.dist}
        if isinstance(obj, MlatData):
            return obj.__dict__
        # Let the base class default method raise the TypeError
        return super().default(obj)

class MlatAnchor:
    def __init__(self, x, y, dist):
        self.x = x
        self.y = y
        self.dist = dist

class MlatData:
    def __init__(self, timestamp_start=None, timestamp_end=None, anchors=None, chosen_anchors=None, result_pos=[None, None], current_color=None):
        self.timestamp_start = timestamp_start
        self.timestamp_end = timestamp_end
        self.anchors = anchors if anchors else []
        self.chosen_anchors = chosen_anchors if chosen_anchors else []
        self.result_pos = result_pos
        self.current_color = current_color


class MlatParser:
    class State(Enum):
        READING = 0
        READING_MLAT = 1
        READING_ANCHORS = 2
        READING_CLOSEST_ANCHORS = 3

    tag_MLAT = "MLAT_LOC"
    tag_APP = "APP"
    def __init__(self):
        self.state = self.State.READING
        self.data = []
        self._current_color = None
        self._reading_obj = None
        self.position_regex = re.compile(r"x=(-?\d+\.?\d*),y=(-?\d+\.?\d*)")
        self.anchor_regex = re.compile(r"^-> x=(-?\d+\.?\d*),y=(-?\d+\.?\d*),d=(-?\d+\.?\d*)")
        self.color_regex = re.compile(r"color to ([0-9a-f]{6})")
    
    def reading_state(self, timestamp, tag, msg):
        if tag == self.tag_MLAT:
            self._reading_obj = MlatData(timestamp_start=timestamp, current_color=self._current_color)
            return self.reading_mlat(timestamp, tag, msg)
        if tag == self.tag_APP:
            if msg.startswith("Setting new color"):
                color_result = self.color_regex.search(msg)
                if color_result:
                    self._current_color = color_result.group(1)
        return self.State.READING
    
    def reading_mlat(self, timestamp, tag, msg):
        if tag != self.tag_MLAT:
            if self._reading_obj:
                self._reading_obj.timestamp_end = timestamp
                self.data.append(self._reading_obj)
                self._reading_obj = None
            return self.reading_state(self.State.READING)
        if msg.startswith("no anchors, setting pos") or msg.startswith("resulting pos"):
            result = self.position_regex.search(msg)
            if not result:
                print(f"Error parsing resulting position {msg=}")
                return self.State.READING_MLAT
            x_pos = result.group(1)
            y_pos = result.group(2)
            self._reading_obj.result_pos = [float(x_pos), float(y_pos)]
            # end of one iteration
            self._reading_obj.timestamp_end = timestamp
            self.data.append(self._reading_obj)
            self._reading_obj = None
            return self.State.READING
        elif msg.startswith("Anchors"):
            return self.State.READING_ANCHORS
        elif msg.startswith("Closest anchors"):
            return self.State.READING_CLOSEST_ANCHORS
        return self.State.READING_MLAT
    
    def reading_anchors(self, timestamp, tag, msg):
        result = self.anchor_regex.match(msg)
        if not result:
            return self.reading_mlat(timestamp, tag, msg)
        x_pos = float(result.group(1))
        y_pos = float(result.group(2))
        dist  = float(result.group(3))
        if self.state == self.State.READING_ANCHORS:
            self._reading_obj.anchors.append(MlatAnchor(x_pos, y_pos, dist))
        elif self.state == self.State.READING_CLOSEST_ANCHORS:
            self._reading_obj.chosen_anchors.append(MlatAnchor(x_pos, y_pos, dist))
        return self.state

    def parse_line(self, line):
        result = LogParser.line_regex.match(line)
        if not result:
            return
        timestamp = int(result.group(1))
        tag = result.group(2)
        msg = result.group(3)
        if self.state == self.State.READING:
            self.state = self.reading_state(timestamp, tag, msg)
        elif self.state == self.State.READING_MLAT:
            self.state = self.reading_mlat(timestamp, tag, msg)
        elif self.state == self.State.READING_ANCHORS or self.state == self.State.READING_CLOSEST_ANCHORS:
            self.state = self.reading_anchors(timestamp, tag, msg)
    def output(self, filename):
        with open(filename, "w") as f:
            f.write(json.dumps(self.data, cls=MlatDataEncoder))
        print(f"found {len(self.data)} data points")

class ColorChangeParser:
    def __init__(self):
        self.data = []
        self.color_regex = re.compile(r"color to ([0-9a-f]{6})")
    
    def parse_line(self, line):
        result = LogParser.line_regex.match(line)
        if not result:
            return
        timestamp = int(result.group(1))
        tag = result.group(2)
        msg = result.group(3)
        if tag != "APP":
            return
        if msg.startswith("Setting new color"):
            color_result = self.color_regex.search(msg)
            if not color_result:
                print("Error: could not parse new color")
                return
            self.data.append({"timestamp": timestamp, "color": color_result.group(1)})
    def output(self, filename):
        with open(filename, "w") as f:
            f.write(json.dumps(self.data))
        print(f"found {len(self.data)} data points")

def parse_log(filename, output_filename, parser):
    f = open(filename, "r")
    while True:
        line = f.readline()
        if not line:
            break # end of file
        parser.parse_line(line)
    f.close()
    parser.output(output_filename)

if __name__ == "__main__":
    args = parser.parse_args()
    parser = None
    if args.parser == "colors":
        parser = ColorChangeParser()
    else:
        parser = MlatParser()
    parse_log(args.input_file, args.output_file, parser)