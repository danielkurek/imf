# -*- coding: utf-8 -*-

################################################################################
## Form generated from reading UI file 'node_editorAiDTIA.ui'
##
## Created by: Qt User Interface Compiler version 6.6.3
##
## WARNING! All changes made in this file will be lost when recompiling UI file!
################################################################################

from PySide6.QtCore import (QCoreApplication, QDate, QDateTime, QLocale,
    QMetaObject, QObject, QPoint, QRect,
    QSize, QTime, QUrl, Qt)
from PySide6.QtGui import (QBrush, QColor, QConicalGradient, QCursor,
    QFont, QFontDatabase, QGradient, QIcon,
    QImage, QKeySequence, QLinearGradient, QPainter,
    QPalette, QPixmap, QRadialGradient, QTransform)
from PySide6.QtWidgets import (QApplication, QCheckBox, QComboBox, QFormLayout,
    QFrame, QGridLayout, QGroupBox, QLabel,
    QLayout, QPushButton, QScrollArea, QSizePolicy,
    QSpinBox, QWidget)

from colorbutton import ColorButton

class Ui_NodeEditor(object):
    def setupUi(self, NodeEditor):
        if not NodeEditor.objectName():
            NodeEditor.setObjectName(u"NodeEditor")
        NodeEditor.resize(323, 443)
        sizePolicy = QSizePolicy(QSizePolicy.Policy.Preferred, QSizePolicy.Policy.Preferred)
        sizePolicy.setHorizontalStretch(0)
        sizePolicy.setVerticalStretch(0)
        sizePolicy.setHeightForWidth(NodeEditor.sizePolicy().hasHeightForWidth())
        NodeEditor.setSizePolicy(sizePolicy)
        self.scrollArea = QScrollArea(NodeEditor)
        self.scrollArea.setObjectName(u"scrollArea")
        self.scrollArea.setGeometry(QRect(0, 0, 381, 481))
        self.scrollArea.setWidgetResizable(True)
        self.scrollAreaWidgetContents = QWidget()
        self.scrollAreaWidgetContents.setObjectName(u"scrollAreaWidgetContents")
        self.scrollAreaWidgetContents.setGeometry(QRect(0, 0, 379, 479))
        self.line = QFrame(self.scrollAreaWidgetContents)
        self.line.setObjectName(u"line")
        self.line.setGeometry(QRect(20, 147, 169, 3))
        self.line.setFrameShape(QFrame.HLine)
        self.line.setFrameShadow(QFrame.Sunken)
        self.gridLayoutWidget = QWidget(self.scrollAreaWidgetContents)
        self.gridLayoutWidget.setObjectName(u"gridLayoutWidget")
        self.gridLayoutWidget.setGeometry(QRect(10, 10, 303, 321))
        self.gridLayout = QGridLayout(self.gridLayoutWidget)
        self.gridLayout.setObjectName(u"gridLayout")
        self.gridLayout.setSizeConstraint(QLayout.SetDefaultConstraint)
        self.gridLayout.setContentsMargins(0, 0, 0, 0)
        self.sendColorButton = QPushButton(self.gridLayoutWidget)
        self.sendColorButton.setObjectName(u"sendColorButton")
        self.sendColorButton.setMaximumSize(QSize(32, 16777215))
        icon = QIcon()
        icon.addFile(u"images/send.png", QSize(), QIcon.Normal, QIcon.On)
        self.sendColorButton.setIcon(icon)

        self.gridLayout.addWidget(self.sendColorButton, 1, 4, 1, 1)

        self.colorLabel = QLabel(self.gridLayoutWidget)
        self.colorLabel.setObjectName(u"colorLabel")
        sizePolicy1 = QSizePolicy(QSizePolicy.Policy.Minimum, QSizePolicy.Policy.Minimum)
        sizePolicy1.setHorizontalStretch(0)
        sizePolicy1.setVerticalStretch(0)
        sizePolicy1.setHeightForWidth(self.colorLabel.sizePolicy().hasHeightForWidth())
        self.colorLabel.setSizePolicy(sizePolicy1)

        self.gridLayout.addWidget(self.colorLabel, 1, 2, 1, 1, Qt.AlignLeft)

        self.onoffLabel = QLabel(self.gridLayoutWidget)
        self.onoffLabel.setObjectName(u"onoffLabel")
        sizePolicy1.setHeightForWidth(self.onoffLabel.sizePolicy().hasHeightForWidth())
        self.onoffLabel.setSizePolicy(sizePolicy1)

        self.gridLayout.addWidget(self.onoffLabel, 3, 2, 1, 1, Qt.AlignLeft)

        self.nodeComboBox = QComboBox(self.gridLayoutWidget)
        self.nodeComboBox.setObjectName(u"nodeComboBox")

        self.gridLayout.addWidget(self.nodeComboBox, 0, 2, 1, 2)

        self.sendOnOffButton = QPushButton(self.gridLayoutWidget)
        self.sendOnOffButton.setObjectName(u"sendOnOffButton")
        self.sendOnOffButton.setMaximumSize(QSize(32, 16777215))
        self.sendOnOffButton.setIcon(icon)

        self.gridLayout.addWidget(self.sendOnOffButton, 3, 4, 1, 1)

        self.onoffCheckBox = QCheckBox(self.gridLayoutWidget)
        self.onoffCheckBox.setObjectName(u"onoffCheckBox")
        self.onoffCheckBox.setChecked(True)

        self.gridLayout.addWidget(self.onoffCheckBox, 3, 3, 1, 1)

        self.sendLevelButton = QPushButton(self.gridLayoutWidget)
        self.sendLevelButton.setObjectName(u"sendLevelButton")
        self.sendLevelButton.setMaximumSize(QSize(32, 16777215))
        self.sendLevelButton.setIcon(icon)

        self.gridLayout.addWidget(self.sendLevelButton, 2, 4, 1, 1)

        self.colorButton = ColorButton(self.gridLayoutWidget)
        self.colorButton.setObjectName(u"colorButton")

        self.gridLayout.addWidget(self.colorButton, 1, 3, 1, 1)

        self.levelSpinBox = QSpinBox(self.gridLayoutWidget)
        self.levelSpinBox.setObjectName(u"levelSpinBox")

        self.gridLayout.addWidget(self.levelSpinBox, 2, 3, 1, 1)

        self.levelLabel = QLabel(self.gridLayoutWidget)
        self.levelLabel.setObjectName(u"levelLabel")
        sizePolicy1.setHeightForWidth(self.levelLabel.sizePolicy().hasHeightForWidth())
        self.levelLabel.setSizePolicy(sizePolicy1)

        self.gridLayout.addWidget(self.levelLabel, 2, 2, 1, 1, Qt.AlignLeft)

        self.sendLocationButton = QPushButton(self.gridLayoutWidget)
        self.sendLocationButton.setObjectName(u"sendLocationButton")
        self.sendLocationButton.setMaximumSize(QSize(32, 16777215))
        icon1 = QIcon()
        icon1.addFile(u"images/send.png", QSize(), QIcon.Normal, QIcon.Off)
        self.sendLocationButton.setIcon(icon1)

        self.gridLayout.addWidget(self.sendLocationButton, 4, 4, 2, 1)

        self.refreshDevicesButton = QPushButton(self.gridLayoutWidget)
        self.refreshDevicesButton.setObjectName(u"refreshDevicesButton")
        self.refreshDevicesButton.setMaximumSize(QSize(32, 16777215))
        icon2 = QIcon()
        icon2.addFile(u"images/refresh.png", QSize(), QIcon.Normal, QIcon.On)
        self.refreshDevicesButton.setIcon(icon2)

        self.gridLayout.addWidget(self.refreshDevicesButton, 0, 4, 1, 1)

        self.groupBox = QGroupBox(self.gridLayoutWidget)
        self.groupBox.setObjectName(u"groupBox")
        self.formLayoutWidget = QWidget(self.groupBox)
        self.formLayoutWidget.setObjectName(u"formLayoutWidget")
        self.formLayoutWidget.setGeometry(QRect(9, 20, 251, 171))
        self.formLayout = QFormLayout(self.formLayoutWidget)
        self.formLayout.setObjectName(u"formLayout")
        self.formLayout.setContentsMargins(0, 0, 0, 0)
        self.northLabel = QLabel(self.formLayoutWidget)
        self.northLabel.setObjectName(u"northLabel")
        sizePolicy1.setHeightForWidth(self.northLabel.sizePolicy().hasHeightForWidth())
        self.northLabel.setSizePolicy(sizePolicy1)

        self.formLayout.setWidget(0, QFormLayout.LabelRole, self.northLabel)

        self.northSpinBox = QSpinBox(self.formLayoutWidget)
        self.northSpinBox.setObjectName(u"northSpinBox")
        self.northSpinBox.setMinimum(-32768)
        self.northSpinBox.setMaximum(32767)

        self.formLayout.setWidget(0, QFormLayout.FieldRole, self.northSpinBox)

        self.eastLabel = QLabel(self.formLayoutWidget)
        self.eastLabel.setObjectName(u"eastLabel")
        sizePolicy1.setHeightForWidth(self.eastLabel.sizePolicy().hasHeightForWidth())
        self.eastLabel.setSizePolicy(sizePolicy1)

        self.formLayout.setWidget(1, QFormLayout.LabelRole, self.eastLabel)

        self.eastSpinBox = QSpinBox(self.formLayoutWidget)
        self.eastSpinBox.setObjectName(u"eastSpinBox")
        self.eastSpinBox.setMinimum(-32768)
        self.eastSpinBox.setMaximum(32767)

        self.formLayout.setWidget(1, QFormLayout.FieldRole, self.eastSpinBox)

        self.altitudeSpinBox = QSpinBox(self.formLayoutWidget)
        self.altitudeSpinBox.setObjectName(u"altitudeSpinBox")
        self.altitudeSpinBox.setMinimum(-32768)
        self.altitudeSpinBox.setMaximum(32767)
        self.altitudeSpinBox.setValue(0)

        self.formLayout.setWidget(2, QFormLayout.FieldRole, self.altitudeSpinBox)

        self.altitudeLabel = QLabel(self.formLayoutWidget)
        self.altitudeLabel.setObjectName(u"altitudeLabel")
        sizePolicy1.setHeightForWidth(self.altitudeLabel.sizePolicy().hasHeightForWidth())
        self.altitudeLabel.setSizePolicy(sizePolicy1)

        self.formLayout.setWidget(2, QFormLayout.LabelRole, self.altitudeLabel)

        self.floorLabel = QLabel(self.formLayoutWidget)
        self.floorLabel.setObjectName(u"floorLabel")
        sizePolicy1.setHeightForWidth(self.floorLabel.sizePolicy().hasHeightForWidth())
        self.floorLabel.setSizePolicy(sizePolicy1)

        self.formLayout.setWidget(3, QFormLayout.LabelRole, self.floorLabel)

        self.floorSpinBox = QSpinBox(self.formLayoutWidget)
        self.floorSpinBox.setObjectName(u"floorSpinBox")
        self.floorSpinBox.setMinimum(0)
        self.floorSpinBox.setMaximum(255)

        self.formLayout.setWidget(3, QFormLayout.FieldRole, self.floorSpinBox)

        self.uncertaintyLabel = QLabel(self.formLayoutWidget)
        self.uncertaintyLabel.setObjectName(u"uncertaintyLabel")
        sizePolicy1.setHeightForWidth(self.uncertaintyLabel.sizePolicy().hasHeightForWidth())
        self.uncertaintyLabel.setSizePolicy(sizePolicy1)

        self.formLayout.setWidget(4, QFormLayout.LabelRole, self.uncertaintyLabel)

        self.uncertaintySpinBox = QSpinBox(self.formLayoutWidget)
        self.uncertaintySpinBox.setObjectName(u"uncertaintySpinBox")
        self.uncertaintySpinBox.setMaximum(65535)

        self.formLayout.setWidget(4, QFormLayout.FieldRole, self.uncertaintySpinBox)


        self.gridLayout.addWidget(self.groupBox, 4, 2, 2, 2)

        self.scrollArea.setWidget(self.scrollAreaWidgetContents)

        self.retranslateUi(NodeEditor)

        QMetaObject.connectSlotsByName(NodeEditor)
    # setupUi

    def retranslateUi(self, NodeEditor):
        NodeEditor.setWindowTitle(QCoreApplication.translate("NodeEditor", u"Form", None))
        self.sendColorButton.setText("")
        self.colorLabel.setText(QCoreApplication.translate("NodeEditor", u"Color", None))
        self.onoffLabel.setText(QCoreApplication.translate("NodeEditor", u"OnOff", None))
        self.sendOnOffButton.setText("")
        self.onoffCheckBox.setText(QCoreApplication.translate("NodeEditor", u"On", None))
        self.sendLevelButton.setText("")
        self.colorButton.setText(QCoreApplication.translate("NodeEditor", u"Color", None))
        self.levelLabel.setText(QCoreApplication.translate("NodeEditor", u"Level", None))
        self.sendLocationButton.setText("")
        self.refreshDevicesButton.setText("")
        self.groupBox.setTitle(QCoreApplication.translate("NodeEditor", u"Location", None))
        self.northLabel.setText(QCoreApplication.translate("NodeEditor", u"North", None))
        self.eastLabel.setText(QCoreApplication.translate("NodeEditor", u"East", None))
        self.altitudeLabel.setText(QCoreApplication.translate("NodeEditor", u"Altitude", None))
        self.floorLabel.setText(QCoreApplication.translate("NodeEditor", u"Floor", None))
        self.uncertaintyLabel.setText(QCoreApplication.translate("NodeEditor", u"Uncertainty", None))
    # retranslateUi

