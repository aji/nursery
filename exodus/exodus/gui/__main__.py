import sys
from PyQt4 import QtGui

import exodus.gui.main

app = QtGui.QApplication(sys.argv)
w = exodus.gui.main.MainWindow()
sys.exit(app.exec_())
