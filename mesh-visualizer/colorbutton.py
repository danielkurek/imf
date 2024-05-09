##
# @file colorbutton.py
# @author Daniel Kurek (daniel.kurek.dev@gmail.com)
#

# based on https://www.pythonguis.com/widgets/qcolorbutton-a-color-selector-tool-for-pyqt/


from PySide6.QtWidgets import QPushButton, QColorDialog
from PySide6.QtGui import QColor
from PySide6.QtCore import Qt, Signal


class ColorButton(QPushButton):
    '''
    Custom Qt Widget to show a chosen color.

    Left-clicking the button shows the color-chooser, while
    right-clicking resets the color to None (no-color).
    '''

    colorChanged = Signal(QColor)

    def __init__(self, *args, **kwargs):
        super(ColorButton, self).__init__(*args, **kwargs)

        self._color = None
        self.pressed.connect(self.onColorPicker)

        # Set the initial/default state.
        self.setColor(QColor("black"))

    def setColor(self, color):
        if color != self._color:
            self._color = color
            self.colorChanged.emit(color)

        if self._color:
            self.setStyleSheet(f"background-color: {self._color.name()};")
        else:
            self.setStyleSheet("")

    def color(self):
        return self._color

    def onColorPicker(self):
        '''
        Show color-picker dialog to select color.

        Qt will use the native dialog by default.

        '''
        dlg = QColorDialog(self)
        if self._color:
            dlg.setCurrentColor(QColor(self._color))

        if dlg.exec_():
            self.setColor(dlg.currentColor())