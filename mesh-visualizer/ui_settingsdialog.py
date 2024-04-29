# -*- coding: utf-8 -*-

################################################################################
## Form generated from reading UI file 'settingsdialogtlRkMa.ui'
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
from PySide6.QtWidgets import (QApplication, QComboBox, QDialog, QGridLayout,
    QGroupBox, QHBoxLayout, QLabel, QPushButton,
    QSizePolicy, QSpacerItem, QWidget)

class Ui_SettingsDialog(object):
    def setupUi(self, SettingsDialog):
        if not SettingsDialog.objectName():
            SettingsDialog.setObjectName(u"SettingsDialog")
        SettingsDialog.resize(435, 342)
        self.gridLayout_3 = QGridLayout(SettingsDialog)
        self.gridLayout_3.setObjectName(u"gridLayout_3")
        self.horizontalLayout = QHBoxLayout()
        self.horizontalLayout.setObjectName(u"horizontalLayout")
        self.horizontalSpacer = QSpacerItem(96, 20, QSizePolicy.Policy.Expanding, QSizePolicy.Policy.Minimum)

        self.horizontalLayout.addItem(self.horizontalSpacer)

        self.applyButton = QPushButton(SettingsDialog)
        self.applyButton.setObjectName(u"applyButton")

        self.horizontalLayout.addWidget(self.applyButton)


        self.gridLayout_3.addLayout(self.horizontalLayout, 1, 0, 1, 2)

        self.parametersBox = QGroupBox(SettingsDialog)
        self.parametersBox.setObjectName(u"parametersBox")
        self.gridLayout_2 = QGridLayout(self.parametersBox)
        self.gridLayout_2.setObjectName(u"gridLayout_2")
        self.flowControlBox = QComboBox(self.parametersBox)
        self.flowControlBox.setObjectName(u"flowControlBox")

        self.gridLayout_2.addWidget(self.flowControlBox, 5, 1, 1, 1)

        self.flowControlLabel = QLabel(self.parametersBox)
        self.flowControlLabel.setObjectName(u"flowControlLabel")

        self.gridLayout_2.addWidget(self.flowControlLabel, 5, 0, 1, 1)

        self.baudRateLabel = QLabel(self.parametersBox)
        self.baudRateLabel.setObjectName(u"baudRateLabel")

        self.gridLayout_2.addWidget(self.baudRateLabel, 0, 0, 1, 1)

        self.parityLabel = QLabel(self.parametersBox)
        self.parityLabel.setObjectName(u"parityLabel")

        self.gridLayout_2.addWidget(self.parityLabel, 3, 0, 1, 1)

        self.dataBitsLabel = QLabel(self.parametersBox)
        self.dataBitsLabel.setObjectName(u"dataBitsLabel")

        self.gridLayout_2.addWidget(self.dataBitsLabel, 2, 0, 1, 1)

        self.stopBitsBox = QComboBox(self.parametersBox)
        self.stopBitsBox.setObjectName(u"stopBitsBox")

        self.gridLayout_2.addWidget(self.stopBitsBox, 4, 1, 1, 1)

        self.dataBitsBox = QComboBox(self.parametersBox)
        self.dataBitsBox.setObjectName(u"dataBitsBox")

        self.gridLayout_2.addWidget(self.dataBitsBox, 2, 1, 1, 1)

        self.stopBitsLabel = QLabel(self.parametersBox)
        self.stopBitsLabel.setObjectName(u"stopBitsLabel")

        self.gridLayout_2.addWidget(self.stopBitsLabel, 4, 0, 1, 1)

        self.parityBox = QComboBox(self.parametersBox)
        self.parityBox.setObjectName(u"parityBox")

        self.gridLayout_2.addWidget(self.parityBox, 3, 1, 1, 1)

        self.baudRateBox = QComboBox(self.parametersBox)
        self.baudRateBox.setObjectName(u"baudRateBox")

        self.gridLayout_2.addWidget(self.baudRateBox, 0, 1, 1, 1)


        self.gridLayout_3.addWidget(self.parametersBox, 0, 1, 1, 1)

        self.selectBox = QGroupBox(SettingsDialog)
        self.selectBox.setObjectName(u"selectBox")
        self.gridLayout = QGridLayout(self.selectBox)
        self.gridLayout.setObjectName(u"gridLayout")
        self.descriptionLabel = QLabel(self.selectBox)
        self.descriptionLabel.setObjectName(u"descriptionLabel")

        self.gridLayout.addWidget(self.descriptionLabel, 2, 0, 1, 1)

        self.vidLabel = QLabel(self.selectBox)
        self.vidLabel.setObjectName(u"vidLabel")

        self.gridLayout.addWidget(self.vidLabel, 6, 0, 1, 1)

        self.manufacturerLabel = QLabel(self.selectBox)
        self.manufacturerLabel.setObjectName(u"manufacturerLabel")

        self.gridLayout.addWidget(self.manufacturerLabel, 3, 0, 1, 1)

        self.refreshButton = QPushButton(self.selectBox)
        self.refreshButton.setObjectName(u"refreshButton")
        sizePolicy = QSizePolicy(QSizePolicy.Policy.Minimum, QSizePolicy.Policy.Fixed)
        sizePolicy.setHorizontalStretch(0)
        sizePolicy.setVerticalStretch(0)
        sizePolicy.setHeightForWidth(self.refreshButton.sizePolicy().hasHeightForWidth())
        self.refreshButton.setSizePolicy(sizePolicy)
        self.refreshButton.setMinimumSize(QSize(0, 0))
        self.refreshButton.setMaximumSize(QSize(32, 16777215))
        icon = QIcon()
        icon.addFile(u"images/refresh.png", QSize(), QIcon.Normal, QIcon.Off)
        self.refreshButton.setIcon(icon)

        self.gridLayout.addWidget(self.refreshButton, 0, 1, 1, 1)

        self.locationLabel = QLabel(self.selectBox)
        self.locationLabel.setObjectName(u"locationLabel")

        self.gridLayout.addWidget(self.locationLabel, 5, 0, 1, 1)

        self.pidLabel = QLabel(self.selectBox)
        self.pidLabel.setObjectName(u"pidLabel")

        self.gridLayout.addWidget(self.pidLabel, 7, 0, 1, 1)

        self.serialNumberLabel = QLabel(self.selectBox)
        self.serialNumberLabel.setObjectName(u"serialNumberLabel")

        self.gridLayout.addWidget(self.serialNumberLabel, 4, 0, 1, 1)

        self.serialPortInfoListBox = QComboBox(self.selectBox)
        self.serialPortInfoListBox.setObjectName(u"serialPortInfoListBox")

        self.gridLayout.addWidget(self.serialPortInfoListBox, 0, 0, 1, 1)


        self.gridLayout_3.addWidget(self.selectBox, 0, 0, 1, 1)


        self.retranslateUi(SettingsDialog)

        QMetaObject.connectSlotsByName(SettingsDialog)
    # setupUi

    def retranslateUi(self, SettingsDialog):
        SettingsDialog.setWindowTitle(QCoreApplication.translate("SettingsDialog", u"Settings", None))
        self.applyButton.setText(QCoreApplication.translate("SettingsDialog", u"Apply", None))
        self.parametersBox.setTitle(QCoreApplication.translate("SettingsDialog", u"Select Parameters", None))
        self.flowControlLabel.setText(QCoreApplication.translate("SettingsDialog", u"Flow control:", None))
        self.baudRateLabel.setText(QCoreApplication.translate("SettingsDialog", u"BaudRate:", None))
        self.parityLabel.setText(QCoreApplication.translate("SettingsDialog", u"Parity:", None))
        self.dataBitsLabel.setText(QCoreApplication.translate("SettingsDialog", u"Data bits:", None))
        self.stopBitsLabel.setText(QCoreApplication.translate("SettingsDialog", u"Stop bits:", None))
        self.selectBox.setTitle(QCoreApplication.translate("SettingsDialog", u"Select Serial Port", None))
        self.descriptionLabel.setText(QCoreApplication.translate("SettingsDialog", u"Description:", None))
        self.vidLabel.setText(QCoreApplication.translate("SettingsDialog", u"Vendor ID:", None))
        self.manufacturerLabel.setText(QCoreApplication.translate("SettingsDialog", u"Manufacturer:", None))
        self.refreshButton.setText("")
        self.locationLabel.setText(QCoreApplication.translate("SettingsDialog", u"Location:", None))
        self.pidLabel.setText(QCoreApplication.translate("SettingsDialog", u"Product ID:", None))
        self.serialNumberLabel.setText(QCoreApplication.translate("SettingsDialog", u"Serial number:", None))
    # retranslateUi

