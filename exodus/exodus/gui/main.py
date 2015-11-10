from PyQt4 import QtGui, QtCore

class MainWindow(QtGui.QWidget):
    def __init__(self):
        super(MainWindow, self).__init__()

        hbox = QtGui.QHBoxLayout(self)

        left = QtGui.QFrame(self)
        left.setFrameShape(QtGui.QFrame.StyledPanel)

        tbl = QtGui.QTableWidget(0, 5, self)

        split = QtGui.QSplitter(QtCore.Qt.Horizontal)
        split.addWidget(left)
        split.addWidget(right)

        hbox.addWidget(split)
        self.setLayout(hbox)
        QtGui.QApplication.setStyle(QtGui.QStyleFactory.create('Cleanlooks'))

        self.setGeometry(300, 300, 250, 150)
        self.setWindowTitle('Exodus')
        self.show()
