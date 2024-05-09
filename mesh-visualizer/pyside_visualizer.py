##
# @file pyside_visualizer.py
# @author Daniel Kurek (daniel.kurek.dev@gmail.com)
#

# based on example project Networkx viewer and Terminal
# https://doc.qt.io/qtforpython-6/examples/example_external_networkx.html
# https://doc.qt.io/qtforpython-6/examples/example_serialport_terminal.html

import math
import sys

from PySide6.QtCore import (QEasingCurve, QLineF,
                            QParallelAnimationGroup, QPointF, QPoint,
                            QPropertyAnimation, QRectF, Qt, Signal, Slot, 
                            QTimer, QIODeviceBase, QObject)
from PySide6.QtGui import QBrush, QColor, QPainter, QPen, QPolygonF
from PySide6.QtWidgets import (QApplication, QComboBox, QGraphicsItem,
                               QGraphicsObject, QGraphicsScene, QGraphicsView,
                               QStyleOptionGraphicsItem, QVBoxLayout, QWidget,
                               QMessageBox, QMainWindow, QLabel, QSplitter)
from PySide6.QtSerialPort import QSerialPort

from ui_mainwindow import Ui_MainWindow
from visualizer_settingdialog import SettingsDialog
from visualizer_nodeeditor import NodeEditor
from visualizer_common import *


HELP = """<b>Mesh Visualizer</b> helps to manage applications using Interactive
 Mesh Framework. Individual nodes are visually represented. Nodes can be moved
 by dragging. Properties can be set using side menu."""

def equal_eps(x: float, y: float, eps = 1e-6):
    return abs(x-y) < eps

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
        width = 85
        height = 85
        self._rect = QRectF(-width/2, -height/2, width, height)
        self.last_pos_update = self.pos()

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

        self.nodes_enabled = True

        # Used to add space between nodes
        self._graph_x_scale = 1
        self._graph_y_scale = -1
        self._flip_x = False
        self._flip_y = False
        self.timer = QTimer(self)
        self.timer.timeout.connect(self.update)

        self.setResizeAnchor(QGraphicsView.AnchorUnderMouse)
        self.updateSceneBounds()
        self.centerOn(0.0,0.0)
    
    @Slot()
    def start(self):
        if not self.timer.isActive():
            self.timer.start(5000)
    
    @Slot()
    def stop(self):
        if self.timer.isActive():
            self.timer.stop()
    
    @Slot(bool)
    def flip_x(self, state):
        print(f"flip x {state}")
        self._flip_x = state
        self.updateSceneBounds()
        self.update_positions()
    
    @Slot(bool)
    def flip_y(self, state):
        print(f"flip y {state}")
        self._flip_y = state
        self.updateSceneBounds()
        self.update_positions()
    
    @Slot()
    def clear(self):
        self.scene().clear()
        self._node_map.clear()
    
    @Slot(bool)
    def setEnabled_nodes(self, enabled):
        self.nodes_enabled = enabled
        if self.nodes_enabled:
            self.start()
        else:
            self.stop()
        for gnode in self._node_map.values():
            gnode.setEnabled(enabled)
    
    def updateSceneBounds(self):
        x1,y1 = self.loc_to_pos(Location(Location.north_limits[0], Location.east_limits[0], 0,0,0))
        x2,y2 = self.loc_to_pos(Location(Location.north_limits[1], Location.east_limits[1], 0,0,0))
        x_min, x_max = (x1,x2) if x1 < x2 else (x2,x1)
        y_min, y_max = (y1,y2) if y1 < y2 else (y2,y1)
        self._scene.setSceneRect(x_min, y_min, x_max - x_min, y_max - y_min)
    
    def drawBackground(self, painter: QPainter, rect: QRectF):
        lines = []
        major_step = math.floor(abs(self._graph_x_scale)*100)
        left = int(math.ceil(rect.left()/float(major_step))*major_step)
        right = int(math.floor(rect.right()/float(major_step))*major_step)
        for x in range(left, right+1, major_step):
            lines.append(QLineF(QPointF(x, rect.top()), QPointF(x, rect.bottom())))
        top = int(math.ceil(rect.top()/float(major_step))*major_step)
        bottom = int(math.floor(rect.bottom()/float(major_step))*major_step)
        for y in range(top, bottom+1, major_step):
            lines.append(QLineF(QPointF(rect.left(), y), QPointF(rect.right(), y)))
        painter.drawLines(lines)
    
    def wheelEvent(self, event):
        factor = 1.1
        if event.angleDelta().y() < 0:
            factor = 0.9
        if QApplication.keyboardModifiers() == Qt.ControlModifier:
            factor = 1.5
            if event.angleDelta().y() < 0:
                factor = 0.5
        view_pos = event.position()
        view_pos = QPoint(view_pos.x(), view_pos.y())
        scene_pos = self.mapToScene(view_pos)
        self.centerOn(scene_pos)
        self.scale(factor, factor)
        delta = self.mapToScene(view_pos) - self.mapToScene(self.viewport().rect().center())
        self.centerOn(scene_pos - delta)
    
    def loc_to_pos(self, loc: Location):
        if loc is None:
            return 0,0
        x = loc.east * self._graph_x_scale * (-1 if self._flip_x else 1)
        y = loc.north  * self._graph_y_scale * (-1 if self._flip_y else 1)
        return x,y
    
    def pos_to_loc(self, pos: QPointF):
        north  = (pos.y() * (-1 if self._flip_y else 1)) / self._graph_y_scale
        east = (pos.x() * (-1 if self._flip_x else 1)) / self._graph_x_scale 
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
            gnode.last_pos_update = QPointF(x,y)

    def _update_nodes(self):
        for gnode in self._node_map.values():
            self._update_node(gnode)
    
    def _update_node(self, gnode):
        node = gnode._node
        pos = gnode.pos()
        last_pos = gnode.last_pos_update
        north, east = self.pos_to_loc(pos)
        # check if node was moved by user
        print(f"{pos.x()-last_pos.x()=} {pos.y()-last_pos.y()=}")
        if (not equal_eps(pos.x(), last_pos.x(), abs(self._graph_x_scale)) or not equal_eps(pos.y(), last_pos.y(), abs(self._graph_y_scale))) \
            and (node.loc and node.loc.north != north and node.loc.east != east):
            # set to user location
            north, east = self.pos_to_loc(pos)
            loc = node.loc
            new_loc = Location(north, east, loc.altitude, loc.floor, loc.uncertainty)
            self.get_data.emit(SerialComm.format_cmd(CmdType.PUT, node.addr, CmdField.LOC, new_loc))
        else:
            # update position
            x, y = self.loc_to_pos(node.loc)
            gnode.setPos(QPointF(x,y))
            gnode.last_pos_update = QPointF(x,y)
            gnode.update()
        

    def _create_node(self, node):
        if node.addr in self._node_map:
            return
        item = NodeG(node)
        x, y = self.loc_to_pos(node.loc)
        item.setPos(QPointF(x,y))
        item.last_pos_update = QPointF(x,y)
        item.setEnabled(self.nodes_enabled)
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

        self.node_editor = NodeEditor(self.serial_comm)

        self.graph = GraphView()
        self.graph.setDragMode(QGraphicsView.ScrollHandDrag)
        self.splitter = QSplitter(Qt.Orientation.Horizontal)
        self.splitter.addWidget(self.graph)
        self.splitter.addWidget(self.node_editor)
        self.splitter.setSizes([350,250])
        self.setCentralWidget(self.splitter)

        self.m_ui.actionConnect.setEnabled(True)
        self.m_ui.actionDisconnect.setEnabled(False)
        self.m_ui.actionQuit.setEnabled(True)
        self.m_ui.actionConfigure.setEnabled(True)
        self.node_editor.setEnabled(False)

        self.m_ui.statusBar.addWidget(self.m_status)

        self.m_ui.actionConnect.triggered.connect(self.open_serial_port)
        self.m_ui.actionDisconnect.triggered.connect(self.close_serial_port)
        self.m_ui.actionQuit.triggered.connect(self.close)
        self.m_ui.actionConfigure.triggered.connect(self.m_settings.show)
        self.m_ui.actionClear.triggered.connect(self.graph.clear)
        self.m_ui.actionAbout.triggered.connect(self.about)
        self.m_ui.actionAboutQt.triggered.connect(qApp.aboutQt)
        self.m_ui.actionFlip_X.toggled.connect(self.graph.flip_x)
        self.m_ui.actionFlip_Y.toggled.connect(self.graph.flip_y)

        self.m_serial.errorOccurred.connect(self.handle_error)
        self.m_serial.readyRead.connect(self.read_data)
        
        self.serial_comm.node_change.connect(self.graph.node_changed)
        self.graph.get_data.connect(self.write_data)
        self.node_editor.serial_write.connect(self.write_data)
    
    @Slot()
    def about(self):
        QMessageBox.about(self, "About Mesh Visualizer", HELP)
    
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
            self.graph.setEnabled_nodes(True)
            self.node_editor.setEnabled(True)
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
        self.graph.setEnabled_nodes(False)
        self.node_editor.setEnabled(False)
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