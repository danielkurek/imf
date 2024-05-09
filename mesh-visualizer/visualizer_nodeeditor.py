##
# @file visualizer_nodeeditor.py
# @author Daniel Kurek (daniel.kurek.dev@gmail.com)
#


import sys

from PySide6.QtCore import Slot, Signal, Qt
from PySide6.QtGui import QIntValidator
from PySide6.QtWidgets import QComboBox
from PySide6.QtSerialPort import QSerialPort, QSerialPortInfo

from PySide6.QtWidgets import QWidget

from visualizer_common import Location, SerialComm, CmdType, CmdField
from ui_node_editor import Ui_NodeEditor


class NodeEditor(QWidget):

    serial_write = Signal(bytearray)

    def __init__(self, serial_comm, parent=None):
        super().__init__(parent)
        
        self.serial_comm = serial_comm
        self.m_ui = Ui_NodeEditor()
        self._custom_addr_index = -1
        self.m_ui.setupUi(self)
        self.m_intValidator = QIntValidator(0, 4000000, self)

        self.m_ui.nodeComboBox.setInsertPolicy(QComboBox.NoInsert)

        self.m_ui.refreshDevicesButton.clicked.connect(self.fill_node_addr)
        self.m_ui.nodeComboBox.currentIndexChanged.connect(self.update_info)
        self.m_ui.nodeComboBox.currentIndexChanged.connect(self.check_custom_addr_policy)

        self.m_ui.sendColorButton.clicked.connect(self.send_color)
        self.m_ui.sendLevelButton.clicked.connect(self.send_level)
        self.m_ui.sendOnOffButton.clicked.connect(self.send_onoff)
        self.m_ui.sendLocationButton.clicked.connect(self.send_location)
        self.m_ui.onoffCheckBox.checkStateChanged.connect(self.onoff_update)

        self.fill_node_addr()

    @Slot(bool)
    def setEnabled(self, enable):
        self.m_ui.refreshDevicesButton.setEnabled(enable)
        self.m_ui.sendColorButton.setEnabled(enable)
        self.m_ui.sendLevelButton.setEnabled(enable)
        self.m_ui.sendOnOffButton.setEnabled(enable)
        self.m_ui.sendLocationButton.setEnabled(enable)
        self.m_ui.nodeComboBox.setEnabled(enable)
    
    def getCurrentAddr(self):
        addr_index = self.m_ui.nodeComboBox.currentIndex()
        if addr_index == self._custom_addr_index:
            text = self.m_ui.nodeComboBox.currentText()
            return text
        else:
            return self.m_ui.nodeComboBox.currentData()

    @Slot()
    def send_color(self):
        addr = self.getCurrentAddr()
        color = self.m_ui.colorButton.color()
        r,g,b,a = color.getRgb()
        color_rgb = f"{r:02x}{g:02x}{b:02x}"
        self.serial_write.emit(SerialComm.format_cmd(CmdType.PUT, addr, CmdField.RGB, color_rgb))
        print(f"Sending color: {addr}:rgb={color_rgb}")
    
    @Slot()
    def send_level(self):
        addr = self.getCurrentAddr()
        value = self.m_ui.levelSpinBox.value()
        self.serial_write.emit(SerialComm.format_cmd(CmdType.PUT, addr, CmdField.LEVEL, value))
    
    @Slot()
    def send_onoff(self):
        addr = self.getCurrentAddr()
        checked = self.m_ui.onoffCheckBox.checkState() == Qt.Checked
        self.serial_write.emit(SerialComm.format_cmd(CmdType.PUT, addr, CmdField.ONOFF, checked))
        print(f"Sending onoff: {addr}:onoff={checked}")
    
    @Slot()
    def send_location(self):
        addr = self.getCurrentAddr()
        north = self.m_ui.northSpinBox.value()
        east = self.m_ui.eastSpinBox.value()
        alt = self.m_ui.altitudeSpinBox.value()
        floor = self.m_ui.floorSpinBox.value()
        uncert = self.m_ui.uncertaintySpinBox.value()
        value = Location(north, east, alt, floor, uncert)
        self.serial_write.emit(SerialComm.format_cmd(CmdType.PUT, addr, CmdField.LOC, value))
        print(f"Sending location: {addr}:loc={str(value)}")
    
    @Slot(Qt.CheckState)
    def onoff_update(self, checked):
        self.m_ui.onoffCheckBox.setText("On" if checked == Qt.Checked else "Off")

    @Slot(int)
    def check_custom_addr_policy(self, idx):
        is_custom_addr = idx == self._custom_addr_index
        self.m_ui.nodeComboBox.setEditable(is_custom_addr)
        if is_custom_addr:
            self.m_ui.nodeComboBox.clearEditText()
            edit = self.m_ui.nodeComboBox.lineEdit()
            edit.setInputMask("HHHH")
    
    @Slot()
    def fill_node_addr(self):
        self.m_ui.nodeComboBox.clear()
        for addr in self.serial_comm.cache.keys():
            self.m_ui.nodeComboBox.addItem(f"{addr.upper()}", addr)
        self.m_ui.nodeComboBox.addItem("FFFF (all devices)", "ffff")
        self.m_ui.nodeComboBox.setCurrentIndex(0)
        self._custom_addr_index = self.m_ui.nodeComboBox.count()
        self.m_ui.nodeComboBox.addItem("Custom")

    @Slot(int)
    def update_info(self, idx):
        addr = self.getCurrentAddr()
        if addr not in self.serial_comm.cache:
            return
        node = self.serial_comm.cache[addr]
        if node.onoff is not None:
            self.m_ui.onoffCheckBox.setChecked(node.onoff)
        if node.level is not None:
            self.m_ui.levelSpinBox.setValue(node.level)
        if node.rgb is not None:
            # self.m_ui.colorButton.setColor()
            pass
        if node.loc is not None:
            self.m_ui.northSpinBox.setValue(node.loc.north)
            self.m_ui.eastSpinBox.setValue(node.loc.east)
            self.m_ui.altitudeSpinBox.setValue(node.loc.altitude)
            self.m_ui.floorSpinBox.setValue(node.loc.floor)
            self.m_ui.uncertaintySpinBox.setValue(node.loc.uncertainty)
