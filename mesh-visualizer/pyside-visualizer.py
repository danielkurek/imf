# based on example project Networkx viewer and Terminal
# https://doc.qt.io/qtforpython-6/examples/example_external_networkx.html
# https://doc.qt.io/qtforpython-6/examples/example_serialport_terminal.html


import math
import sys
from enum import Enum

from PySide6.QtCore import (QEasingCurve, QLineF,
                            QParallelAnimationGroup, QPointF,
                            QPropertyAnimation, QRectF, Qt, Signal, Slot, 
                            QTimer, QIODeviceBase, QObject)
from PySide6.QtGui import QBrush, QColor, QPainter, QPen, QPolygonF
from PySide6.QtWidgets import (QApplication, QComboBox, QGraphicsItem,
                               QGraphicsObject, QGraphicsScene, QGraphicsView,
                               QStyleOptionGraphicsItem, QVBoxLayout, QWidget,
                               QMessageBox, QMainWindow, QLabel)
from PySide6.QtSerialPort import QSerialPort

from ui_mainwindow import Ui_MainWindow
from visualizaer_settingdialog import SettingsDialog

def equal_eps(x: float, y: float, eps = 1e-6):
    return abs(x-y) < eps

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
    def __init__(self, north, east, altitude, floor, uncertainty):
        self.north = int(north)
        self.east = int(east)
        self.altitude = int(altitude)
        self.floor = int(floor)
        self.uncertainty = int(uncertainty)
    def __str__(self):
        return f"N{int(self.north):05}E{int(self.east):05}A{int(self.altitude):05}F{int(self.floor):03}U{int(self.uncertainty):05}"
    def parse(value: str):
        if len(value) != 28:
            print(f"Error: unexpected location value - expected length 28, got {len(value)}")
            return None
        north = int(value[1:6])
        east = int(value[7:12])
        alt = int(value[13:18])
        floor = int(value[19:22])
        uncertainty = int(value[23:])
        return Location(north,east,alt,floor,uncertainty)

class Node:
    def __init__(self, addr, loc = None, rgb = None, level = None):
        self.addr = addr
        self.loc = loc
        self.rgb = rgb
        self.level = level
    def __str__(self):
        return f"A:{self.addr}\nC:{self.rgb}\nL:{self.level}\nx={self.loc.north if self.loc is not None else "?"} y={self.loc.east if self.loc is not None else "?"}"

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
            addr, field = field.split(':')
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
        return f"{cmd_type_str} {addr + ":" + field_str if addr is not None else field_str}{" " + value_str if cmd_type == CmdType.PUT else ""}\n".encode()

class NodeG(QGraphicsObject):

    """A QGraphicsItem representing node in a graph"""
    position_change = Signal(str,QPointF)

    def __init__(self, node: Node, parent=None):
        """Node constructor

        Args:
            name (str): Node label
        """
        super().__init__(parent)
        self._node = node
        self._color = "#5AD469"
        self._rect = QRectF(0, 0, 85, 85)

        self.setFlag(QGraphicsItem.ItemIsMovable)
        self.setFlag(QGraphicsItem.ItemSendsGeometryChanges)
        self.setCacheMode(QGraphicsItem.DeviceCoordinateCache)

    def boundingRect(self) -> QRectF:
        """Override from QGraphicsItem

        Returns:
            QRect: Return node bounding rect
        """
        return self._rect

    def paint(self, painter: QPainter, option: QStyleOptionGraphicsItem, widget: QWidget = None):
        """Override from QGraphicsItem

        Draw node

        Args:
            painter (QPainter)
            option (QStyleOptionGraphicsItem)
        """
        painter.setRenderHints(QPainter.Antialiasing)
        painter.setPen(
            QPen(
                QColor(self._color).darker(),
                2,
                Qt.SolidLine,
                Qt.RoundCap,
                Qt.RoundJoin,
            )
        )
        painter.setBrush(QBrush(QColor(self._color)))
        painter.drawRect(self._rect)
        painter.setPen(QPen(QColor("white")))
        painter.drawText(self.boundingRect(), Qt.AlignCenter, str(self._node))

    def itemChange(self, change: QGraphicsItem.GraphicsItemChange, value):
        """Override from QGraphicsItem

        Args:
            change (QGraphicsItem.GraphicsItemChange)
            value (Any)

        Returns:
            Any
        """
        if change == QGraphicsItem.ItemPositionHasChanged:
            print(f"position changed for {self._node.addr=}, {value=}")
            self.position_change.emit(self._node.addr, value)

        return super().itemChange(change, value)


class GraphView(QGraphicsView):

    get_data = Signal(bytearray)

    def __init__(self, parent=None):
        """GraphView constructor

        This widget can display a directed graph

        Args:
            graph (nx.DiGraph): a networkx directed graph
        """
        super().__init__()
        self._scene = QGraphicsScene()
        self.setScene(self._scene)
        self._node_map = {}

        # Used to add space between nodes
        self._graph_x_scale = 1
        self._graph_y_scale = -1
        self.update_positions()
        self.timer = QTimer(self)
        self.timer.timeout.connect(self.update)
    
    @Slot()
    def start(self):
        if not self.timer.isActive():
            self.timer.start(2000)
    
    @Slot()
    def stop(self):
        if self.timer.isActive():
            self.timer.stop()
    
    @Slot()
    def clear(self):
        self.scene().clear()
        self._node_map.clear()
    
    def loc_to_pos(self, loc: Location):
        x = loc.north * self._graph_x_scale
        y = loc.east  * self._graph_y_scale
        return x,y
    
    def pos_to_loc(self, pos: QPointF):
        north = pos.x() / self._graph_x_scale
        east  = pos.y() / self._graph_y_scale
        return north, east

    def update(self):
        print("sending command")
        self.get_data.emit("GET ffff:loc\n".encode())
        for gnode in self._node_map.values():
            self._update_node(gnode)

    def update_positions(self):
        """Set networkx layout and start animation

        Args:
            name (str): Layout name
        """

        # Compute node position from layout function
        for gnode in self._node_map.values():
            x, y = self.loc_to_pos(gnode._node.loc)
            gnode.setPos(QPointF(x,y))

        # Change position of all nodes using an animation
        # self.animations = QParallelAnimationGroup()
        # for node, pos in positions.items():
        #     x, y = pos
        #     x *= self._graph_scale
        #     y *= self._graph_scale
        #     item = self._nodes_map[node]

        #     animation = QPropertyAnimation(item, b"pos")
        #     animation.setDuration(1000)
        #     animation.setEndValue(QPointF(x, y))
        #     animation.setEasingCurve(QEasingCurve.OutExpo)
        #     self.animations.addAnimation(animation)

        # self.animations.start()
    def _update_node(self, gnode):
        node = gnode._node
        pos = gnode.pos()
        saved_pos = self.loc_to_pos(node.loc)
        # check if node was moved by user
        if not equal_eps(pos.x(), saved_pos[0]) or not equal_eps(pos.y(), saved_pos[1]):
            # set to user location
            north, east = self.pos_to_loc(pos)
            loc = node.loc
            new_loc = Location(north, east, loc.altitude, loc.floor, loc.uncertainty)
            self.get_data.emit(SerialComm.format_cmd(CmdType.PUT, node.addr, CmdField.LOC, new_loc))
        else:
            # update position
            x, y = self.loc_to_pos(node.loc)
            gnode.setPos(QPointF(x,y))
            gnode.update()
        

    def _create_node(self, node):
        if node.addr in self._node_map:
            return
        item = NodeG(node)
        x, y = self.loc_to_pos(node.loc)
        item.setPos(QPointF(x,y))
        self.scene().addItem(item)
        # item.position_change.connect(self.node_moved)
        self._node_map[node.addr] = item

    def _create_nodes(self, nodes: list[Node]):
        """Load graph into QGraphicsScene using Node class and Edge class"""

        self.scene().clear()
        
        # Add nodes
        for node in nodes:
            _create_node(node)
    
    @Slot(Node)
    def node_changed(self, node):
        if node.addr not in self._node_map:
            self._create_node(node)
            return
        else:
            self._update_node(self._node_map[node.addr])


class MainWindow(QMainWindow):
    def __init__(self, parent=None):
        super().__init__()

        self.m_ui = Ui_MainWindow()
        self.m_status = QLabel()
        self.m_settings = SettingsDialog(self)
        self.m_serial = QSerialPort(self)
        self.serial_comm = SerialComm()
        self.m_ui.setupUi(self)

        self.graph = GraphView()
        self.setCentralWidget(self.graph)

        self.m_ui.actionConnect.setEnabled(True)
        self.m_ui.actionDisconnect.setEnabled(False)
        self.m_ui.actionQuit.setEnabled(True)
        self.m_ui.actionConfigure.setEnabled(True)

        self.m_ui.statusBar.addWidget(self.m_status)

        self.m_ui.actionConnect.triggered.connect(self.open_serial_port)
        self.m_ui.actionDisconnect.triggered.connect(self.close_serial_port)
        self.m_ui.actionQuit.triggered.connect(self.close)
        self.m_ui.actionConfigure.triggered.connect(self.m_settings.show)
        self.m_ui.actionClear.triggered.connect(self.graph.clear)
        # self.m_ui.actionAbout.triggered.connect(self.about)
        self.m_ui.actionAboutQt.triggered.connect(qApp.aboutQt)

        self.m_serial.errorOccurred.connect(self.handle_error)
        self.m_serial.readyRead.connect(self.read_data)
        
        self.serial_comm.node_change.connect(self.graph.node_changed)
        self.graph.get_data.connect(self.write_data)
    
    @Slot()
    def open_serial_port(self):
        s = self.m_settings.settings()
        self.m_serial.setPortName(s.name)
        self.m_serial.setBaudRate(s.baud_rate)
        self.m_serial.setDataBits(s.data_bits)
        self.m_serial.setParity(s.parity)
        self.m_serial.setStopBits(s.stop_bits)
        self.m_serial.setFlowControl(s.flow_control)
        if self.m_serial.open(QIODeviceBase.ReadWrite):
            self.graph.setEnabled(True)
            self.graph.start()
            self.m_ui.actionConnect.setEnabled(False)
            self.m_ui.actionDisconnect.setEnabled(True)
            self.m_ui.actionConfigure.setEnabled(False)
            self.show_status_message("Connected")
        else:
            self.graph.stop()
            QMessageBox.critical(self, "Error", self.m_serial.errorString())
            self.show_status_message("Open error")
    @Slot()
    def close_serial_port(self):
        if self.m_serial.isOpen():
            self.m_serial.close()
        self.graph.setEnabled(False)
        self.m_ui.actionConnect.setEnabled(True)
        self.m_ui.actionDisconnect.setEnabled(False)
        self.m_ui.actionConfigure.setEnabled(True)
        self.show_status_message("Disconnected")
        self.graph.stop()
    
    @Slot(bytearray)
    def write_data(self, data):
        print(f"sending to serial {data=}")
        self.m_serial.write(data)

    @Slot()
    def read_data(self):
        while self.m_serial.canReadLine():
            rsp = self.m_serial.readLine().data().decode()
            rsp = rsp.strip()
            print(f"received {rsp=}")
            self.serial_comm.parse_response(rsp)

    @Slot(QSerialPort.SerialPortError)
    def handle_error(self, error):
        if error == QSerialPort.ResourceError:
            QMessageBox.critical(self, "Critical Error",
                                 self.m_serial.errorString())
            self.close_serial_port()
    @Slot(str)
    def show_status_message(self, message):
        self.m_status.setText(message)


if __name__ == "__main__":

    app = QApplication(sys.argv)

    widget = MainWindow()
    widget.show()
    widget.resize(800, 600)
    sys.exit(app.exec())