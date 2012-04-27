# -*- coding: utf-8 -*-

# Form implementation generated from reading ui file '/home/wyrdmeister/control/ui/MainWindow.ui'
#
# Created: Fri Apr 27 17:16:58 2012
#      by: PyQt4 UI code generator 4.8.5
#
# WARNING! All changes made in this file will be lost!

from PyQt4 import QtCore, QtGui

try:
    _fromUtf8 = QtCore.QString.fromUtf8
except AttributeError:
    _fromUtf8 = lambda s: s

class Ui_MainWindow(object):
    def setupUi(self, MainWindow):
        MainWindow.setObjectName(_fromUtf8("MainWindow"))
        MainWindow.resize(581, 518)
        MainWindow.setWindowTitle(QtGui.QApplication.translate("MainWindow", "RTnet and ATMD-GPX control", None, QtGui.QApplication.UnicodeUTF8))
        self.centralWidget = QtGui.QWidget(MainWindow)
        self.centralWidget.setObjectName(_fromUtf8("centralWidget"))
        self.groupBox = QtGui.QGroupBox(self.centralWidget)
        self.groupBox.setGeometry(QtCore.QRect(50, 50, 231, 81))
        font = QtGui.QFont()
        font.setBold(True)
        font.setWeight(75)
        self.groupBox.setFont(font)
        self.groupBox.setStyleSheet(_fromUtf8("QGroupBox {\n"
"    background-color: #FFFFFF;\n"
"    border: 2px solid gray;\n"
"    border-radius: 5px;\n"
"    margin-top: 1ex; /* leave space at the top for the title */\n"
"}\n"
"QGroupBox::title {\n"
"    top: 2px;\n"
"    left: 5px;\n"
"}\n"
""))
        self.groupBox.setTitle(QtGui.QApplication.translate("MainWindow", "RTnet", None, QtGui.QApplication.UnicodeUTF8))
        self.groupBox.setObjectName(_fromUtf8("groupBox"))
        self.rtnet_start = QtGui.QPushButton(self.groupBox)
        self.rtnet_start.setGeometry(QtCore.QRect(10, 40, 97, 27))
        self.rtnet_start.setText(QtGui.QApplication.translate("MainWindow", "Start", None, QtGui.QApplication.UnicodeUTF8))
        self.rtnet_start.setObjectName(_fromUtf8("rtnet_start"))
        self.rtnet_stop = QtGui.QPushButton(self.groupBox)
        self.rtnet_stop.setGeometry(QtCore.QRect(120, 40, 97, 27))
        self.rtnet_stop.setText(QtGui.QApplication.translate("MainWindow", "Stop", None, QtGui.QApplication.UnicodeUTF8))
        self.rtnet_stop.setObjectName(_fromUtf8("rtnet_stop"))
        self.groupBox_2 = QtGui.QGroupBox(self.centralWidget)
        self.groupBox_2.setGeometry(QtCore.QRect(50, 140, 231, 81))
        font = QtGui.QFont()
        font.setBold(True)
        font.setWeight(75)
        self.groupBox_2.setFont(font)
        self.groupBox_2.setStyleSheet(_fromUtf8("QGroupBox {\n"
"    background-color: #FFFFFF;\n"
"    border: 2px solid gray;\n"
"    border-radius: 5px;\n"
"    margin-top: 1ex; /* leave space at the top for the title */\n"
"}\n"
"QGroupBox::title {\n"
"    top: 2px;\n"
"    left: 5px;\n"
"}\n"
""))
        self.groupBox_2.setTitle(QtGui.QApplication.translate("MainWindow", "ATMD-GPX server", None, QtGui.QApplication.UnicodeUTF8))
        self.groupBox_2.setObjectName(_fromUtf8("groupBox_2"))
        self.atdm_start = QtGui.QPushButton(self.groupBox_2)
        self.atdm_start.setGeometry(QtCore.QRect(10, 40, 97, 27))
        self.atdm_start.setText(QtGui.QApplication.translate("MainWindow", "Start", None, QtGui.QApplication.UnicodeUTF8))
        self.atdm_start.setObjectName(_fromUtf8("atdm_start"))
        self.atmd_stop = QtGui.QPushButton(self.groupBox_2)
        self.atmd_stop.setGeometry(QtCore.QRect(120, 40, 97, 27))
        self.atmd_stop.setText(QtGui.QApplication.translate("MainWindow", "Stop", None, QtGui.QApplication.UnicodeUTF8))
        self.atmd_stop.setObjectName(_fromUtf8("atmd_stop"))
        self.agent_host = QtGui.QLineEdit(self.centralWidget)
        self.agent_host.setGeometry(QtCore.QRect(131, 15, 151, 27))
        self.agent_host.setText(QtGui.QApplication.translate("MainWindow", "pcl-tdc", None, QtGui.QApplication.UnicodeUTF8))
        self.agent_host.setObjectName(_fromUtf8("agent_host"))
        self.label = QtGui.QLabel(self.centralWidget)
        self.label.setGeometry(QtCore.QRect(50, 20, 81, 17))
        self.label.setText(QtGui.QApplication.translate("MainWindow", "Agent host:", None, QtGui.QApplication.UnicodeUTF8))
        self.label.setObjectName(_fromUtf8("label"))
        self.groupBox_4 = QtGui.QGroupBox(self.centralWidget)
        self.groupBox_4.setGeometry(QtCore.QRect(300, 50, 231, 171))
        font = QtGui.QFont()
        font.setBold(True)
        font.setItalic(False)
        font.setWeight(75)
        self.groupBox_4.setFont(font)
        self.groupBox_4.setStyleSheet(_fromUtf8("QGroupBox {\n"
"    background-color: #FFFFFF;\n"
"    border: 2px solid gray;\n"
"    border-radius: 5px;\n"
"    margin-top: 1ex; /* leave space at the top for the title */\n"
"}\n"
"QGroupBox::title {\n"
"    top: 2px;\n"
"    left: 5px;\n"
"}\n"
""))
        self.groupBox_4.setTitle(QtGui.QApplication.translate("MainWindow", "Status", None, QtGui.QApplication.UnicodeUTF8))
        self.groupBox_4.setObjectName(_fromUtf8("groupBox_4"))
        self.label_2 = QtGui.QLabel(self.groupBox_4)
        self.label_2.setGeometry(QtCore.QRect(10, 40, 111, 17))
        self.label_2.setText(QtGui.QApplication.translate("MainWindow", "RTnet master:", None, QtGui.QApplication.UnicodeUTF8))
        self.label_2.setObjectName(_fromUtf8("label_2"))
        self.label_3 = QtGui.QLabel(self.groupBox_4)
        self.label_3.setGeometry(QtCore.QRect(10, 70, 111, 17))
        self.label_3.setText(QtGui.QApplication.translate("MainWindow", "RTnet slave:", None, QtGui.QApplication.UnicodeUTF8))
        self.label_3.setObjectName(_fromUtf8("label_3"))
        self.label_4 = QtGui.QLabel(self.groupBox_4)
        self.label_4.setGeometry(QtCore.QRect(10, 100, 131, 17))
        self.label_4.setText(QtGui.QApplication.translate("MainWindow", "ATMD-GPX server:", None, QtGui.QApplication.UnicodeUTF8))
        self.label_4.setObjectName(_fromUtf8("label_4"))
        self.label_5 = QtGui.QLabel(self.groupBox_4)
        self.label_5.setGeometry(QtCore.QRect(10, 130, 131, 17))
        self.label_5.setText(QtGui.QApplication.translate("MainWindow", "ATMD-GPX agent:", None, QtGui.QApplication.UnicodeUTF8))
        self.label_5.setObjectName(_fromUtf8("label_5"))
        self.status_rtnet_master = QtGui.QLabel(self.groupBox_4)
        self.status_rtnet_master.setGeometry(QtCore.QRect(150, 40, 67, 17))
        font = QtGui.QFont()
        font.setBold(True)
        font.setItalic(False)
        font.setWeight(75)
        self.status_rtnet_master.setFont(font)
        self.status_rtnet_master.setText(QtGui.QApplication.translate("MainWindow", "Unknown", None, QtGui.QApplication.UnicodeUTF8))
        self.status_rtnet_master.setObjectName(_fromUtf8("status_rtnet_master"))
        self.status_atmd_server = QtGui.QLabel(self.groupBox_4)
        self.status_atmd_server.setGeometry(QtCore.QRect(150, 100, 67, 17))
        font = QtGui.QFont()
        font.setBold(True)
        font.setItalic(False)
        font.setWeight(75)
        self.status_atmd_server.setFont(font)
        self.status_atmd_server.setText(QtGui.QApplication.translate("MainWindow", "Unknown", None, QtGui.QApplication.UnicodeUTF8))
        self.status_atmd_server.setObjectName(_fromUtf8("status_atmd_server"))
        self.status_atmd_agent = QtGui.QLabel(self.groupBox_4)
        self.status_atmd_agent.setGeometry(QtCore.QRect(150, 130, 67, 17))
        font = QtGui.QFont()
        font.setBold(True)
        font.setItalic(False)
        font.setWeight(75)
        self.status_atmd_agent.setFont(font)
        self.status_atmd_agent.setText(QtGui.QApplication.translate("MainWindow", "Unknown", None, QtGui.QApplication.UnicodeUTF8))
        self.status_atmd_agent.setObjectName(_fromUtf8("status_atmd_agent"))
        self.status_rtnet_slave = QtGui.QLabel(self.groupBox_4)
        self.status_rtnet_slave.setGeometry(QtCore.QRect(150, 70, 67, 17))
        font = QtGui.QFont()
        font.setBold(True)
        font.setItalic(False)
        font.setWeight(75)
        self.status_rtnet_slave.setFont(font)
        self.status_rtnet_slave.setText(QtGui.QApplication.translate("MainWindow", "Unknown", None, QtGui.QApplication.UnicodeUTF8))
        self.status_rtnet_slave.setObjectName(_fromUtf8("status_rtnet_slave"))
        self.command_output = QtGui.QPlainTextEdit(self.centralWidget)
        self.command_output.setGeometry(QtCore.QRect(50, 240, 481, 221))
        self.command_output.setObjectName(_fromUtf8("command_output"))
        self.status_toggle = QtGui.QProgressBar(self.centralWidget)
        self.status_toggle.setGeometry(QtCore.QRect(460, 17, 71, 23))
        self.status_toggle.setProperty("value", 0)
        self.status_toggle.setTextVisible(False)
        self.status_toggle.setObjectName(_fromUtf8("status_toggle"))
        self.exit_button = QtGui.QPushButton(self.centralWidget)
        self.exit_button.setGeometry(QtCore.QRect(434, 470, 97, 27))
        self.exit_button.setText(QtGui.QApplication.translate("MainWindow", "Exit", None, QtGui.QApplication.UnicodeUTF8))
        self.exit_button.setObjectName(_fromUtf8("exit_button"))
        MainWindow.setCentralWidget(self.centralWidget)

        self.retranslateUi(MainWindow)
        QtCore.QMetaObject.connectSlotsByName(MainWindow)

    def retranslateUi(self, MainWindow):
        pass


if __name__ == "__main__":
    import sys
    app = QtGui.QApplication(sys.argv)
    MainWindow = QtGui.QMainWindow()
    ui = Ui_MainWindow()
    ui.setupUi(MainWindow)
    MainWindow.show()
    sys.exit(app.exec_())

