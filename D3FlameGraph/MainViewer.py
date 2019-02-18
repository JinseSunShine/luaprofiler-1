#coding=utf-8

# python 3.7
# import  webbrowser
# import time, os


# num = 1
# while num <= 10:
#     num += 1
#     #
#     time.sleep(10)
#     webbrowser.open(u"https://blog.csdn.net/ITLearnHall/article/details/80708483", 0, False)

# webbrowser.open("F:/MyProject/Lua/LuaProfiler/Luaprofiler-mygithub/D3FlameGraph/test.html",0,True)

import sys,os, win32ui
import  time
from PySide2 import QtCore, QtGui, QtWidgets
from PySide2.QtWidgets import QApplication
import tkinter
import tkinter.messagebox

from PySide2.QtWidgets import ( QHeaderView, QMainWindow, QTreeWidgetItem,
                                QGridLayout, QPushButton)

from DataVisualization import DataViewer
# from selenium import webdriver
import webbrowser


class MainWindow(QMainWindow):
    """ main window logic class """
    def __init__(self, parent=None):
        super().__init__()

        self.file_path=None
        self._index = 0

        self.DataViewer = DataViewer()

        self.setObjectName("MainWindow")
        self.setGeometry(400, 400, 400, 200)
        self.setWindowTitle(QtWidgets.QApplication.translate("MainWindow", "Lua Profiler Viewer", None, -1))

        lbl_SplitTime = QtWidgets.QLabel(self)
        lbl_SplitTime.setGeometry(QtCore.QRect(30, 20, 100, 45))
        lbl_SplitTime.setText(u"SplitTime Time")
        #
        self.txt_SplitTime = QtWidgets.QLineEdit(self)
        self.txt_SplitTime.setGeometry(QtCore.QRect(120, 30, 80, 20))
        self.txt_SplitTime.setText("1")

        btnLoadFile = QtWidgets.QPushButton(self)
        btnLoadFile.setGeometry(10, 60, 100, 30)
        btnLoadFile.setText(u"Load file")
        btnLoadFile.clicked.connect(self.load_jsonfile)

        # prograss bar
        self.prograssbar = QtWidgets.QProgressBar(self)
        self.prograssbar.setGeometry(30,120, 340,30)
        self.prograssbar.setValue(0)


        # btnNext = QtWidgets.QPushButton(self)
        # btnNext.setGeometry(10, 100, 100, 30)
        # btnNext.setText(u"Next Frame")
        # btnNext.clicked.connect(self.load_nextdata)
        #
        # btnPre = QtWidgets.QPushButton(self)
        # btnPre.setGeometry(10, 140, 100, 30)
        # btnPre.setText(u"Pre Frame")
        # btnPre.clicked.connect(self.load_predata)
        #
        self.lbl_Print = QtWidgets.QLabel(self)
        self.lbl_Print.setGeometry(QtCore.QRect(50, 160, 300, 30))
        self.lbl_Print.setText(u"Waiting for Analysis...")

    def get_splitTime(self):
        try :
            split_time = float(self.txt_SplitTime.text())
            if split_time <= 0:
                split_time = 1
        except ValueError:
            print("Invalid input value")
            split_time = 1

        return split_time

    # load json file
    def load_jsonfile(self):
        """ sort section """
        dirc = os.getcwd()
        dlg = win32ui.CreateFileDialog(1)
        dlg.SetOFNInitialDir(dirc)
        flag = dlg.DoModal()
        if flag == 1 :
            self.lbl_Print.setText(u"Begin Analysis...")

            self.file_path = dlg.GetPathName()
            split_time = self.get_splitTime()
            if self.DataViewer.OpneJsonFile(self.file_path, split_time) :
                splits_count = self.DataViewer.get_sample_node_count()
                if splits_count <= 0 :
                    self.lbl_Print.setText(u"Not contain avaliable value data")
                    return

                path = os.path.realpath(sys.argv[0])
                path = os.path.split(path)[0]

                dir_name = path +'/'+ time.strftime("%Y-%m-%d-%H-%M-%S",time.localtime())

                if not os.path.exists(dir_name):
                    os.mkdir(dir_name)

                b_success = False
                for i in range(0,splits_count):

                    self.prograssbar.setValue(((i+1)/splits_count) * 100)

                    frameGraph, code = self.DataViewer.FlushData(True)
                    if frameGraph == None:
                        self.lbl_Print.setText("FlushData failed not analysis data" + str(code))
                        break

                    ofile = open('{0}/example_{1}.html'.format(dir_name,i), 'wb')
                    ofile.write(frameGraph.ToHTML().encode('utf-8'))
                    ofile.close()

                    if i == (splits_count-1):
                        b_success = True
                    pass

                # self.refresh_web()

                # export total cost info
                jsData = self.DataViewer.GetTotalCostInfo()
                ofile = open('{0}/totalinfo.json'.format(dir_name), 'w')
                ofile.write(jsData)
                ofile.close()

                self.prograssbar.setValue(100)
                self.lbl_Print.setText("Analysis data Success!")
            else:
                tkinter.messagebox.showerror("Error","File is illegal")
                self.lbl_Print.setText("File is illegal")
        else:
            self.lbl_Print.setText("Not Select File")
            return


    def load_nextdata(self):
        self._index += 1
        self.DataViewer.FlushData(True)
        self.refresh_web()

    def load_predata(self):
        self._index -= 1
        self.DataViewer.FlushData(False)
        self.refresh_web()

    def refresh_web(self):
        # u打开浏览器
        # browser = webdriver.Chrome()

        path = sys.path[0]
        path = path.replace('\\', '/')
        url = 'file:///' + path + '/example.html'
        #
        # browser.get(url)
        webbrowser.open(url, 0, False)

def main():

    app = QApplication(sys.argv)
    form = MainWindow()
    form.show()
    sys.exit(app.exec_())


if __name__ == '__main__':
    main()