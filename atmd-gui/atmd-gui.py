#!/usr/bin/python

from PyQt4 import QtGui
from ui.mainwindow import mainWindow

if __name__ == "__main__":
    import sys
    app = QtGui.QApplication(sys.argv)
    ui = mainWindow()
    ui.show()
    sys.exit(app.exec_())
