# -*- coding: utf-8 -*-

"""
Module implementing MainWindow.
"""

from PyQt4.QtCore import pyqtSignature, pyqtSlot
from PyQt4.QtGui import QMainWindow
from PyQt4 import QtCore
from PyQt4.QtGui import QMessageBox

from .Ui_MainWindow import Ui_MainWindow

import commands, threading, time


class MainWindow(QMainWindow, Ui_MainWindow):
    """
    Class documentation goes here.
    """
    sig_status = QtCore.pyqtSignal()
    def __init__(self, parent=None):
        """
        Constructor
        
        @param parent reference to the parent widget (QWidget)
        """
        QMainWindow.__init__(self, parent)
        self.setupUi(self)
        
        # Statuses
        self.rtnet_master = False
        self.rtnet_slave = False
        self.atmd_server = False
        self.atmd_agent = False
        
        # Connect signal to update status
        self.sig_status.connect(self.update_indicator)
        
        # Start timer
        self.period = 3.0
        self.timer = threading.Timer(self.period, self.update_statuses)
        self.timer.start()
        
    def closeEvent(self, event):
        self.timer.cancel()
        event.accept()

    def error(self, message, title="Error"):
        """ Display an error message
        """
        QMessageBox.critical(self, title, message)
        
    def warning(self, message, title="Warning"):
        """ Display an error message
        """
        QMessageBox.warning(self, title, message)

    def update_statuses(self):
        out = commands.getstatusoutput('cat /proc/xenomai/rtdm/protocol_devices | grep PACKET_DGRAM')
        if out[0] == 0 or (out[0] == 256 and out[1] == ''):
            self.rtnet_master = (out[1] != '')
        else:
            print "Error: ["+str(out[0])+"]" + str(out[1])
        
        out = commands.getstatusoutput('ps -e | grep atmd_server')
        if out[0] == 0 or (out[0] == 256 and out[1] == ''):
            self.atmd_server = (out[1] != '')
        else:
            print "Error: ["+str(out[0])+"]" + str(out[1])

        out = commands.getstatusoutput('ssh root@pcl-tdc "cat /proc/xenomai/rtdm/protocol_devices | grep PACKET_DGRAM"')
        if out[0] == 0 or (out[0] == 256 and out[1] == ''):
            self.rtnet_slave = (out[1] != '')
        else:
            print "Error: ["+str(out[0])+"]" + str(out[1])

        out = commands.getstatusoutput('ssh root@pcl-tdc "ps -e | grep atmd-agent"')
        if out[0] == 0 or (out[0] == 256 and out[1] == ''):
            self.atmd_agent = (out[1] != '')
        else:
            print "Error: ["+str(out[0])+"]" + str(out[1])

        self.sig_status.emit()
        self.timer = threading.Timer(self.period, self.update_statuses)
        self.timer.start()
    
    @pyqtSlot()
    def update_indicator(self):
        if self.rtnet_master:
            self.status_rtnet_master.setText('Running')
            self.status_rtnet_master.setStyleSheet('QLabel { color: #006600; }')
        else:
            self.status_rtnet_master.setText('Stopped')
            self.status_rtnet_master.setStyleSheet('QLabel { color: #C80000; }')

        if self.rtnet_slave:
            self.status_rtnet_slave.setText('Running')
            self.status_rtnet_slave.setStyleSheet('QLabel { color: #006600; }')
        else:
            self.status_rtnet_slave.setText('Stopped')
            self.status_rtnet_slave.setStyleSheet('QLabel { color: #C80000; }')

        if self.atmd_server:
            self.status_atmd_server.setText('Running')
            self.status_atmd_server.setStyleSheet('QLabel { color: #006600; }')
        else:
            self.status_atmd_server.setText('Stopped')
            self.status_atmd_server.setStyleSheet('QLabel { color: #C80000; }')

        if self.atmd_agent:
            self.status_atmd_agent.setText('Running')
            self.status_atmd_agent.setStyleSheet('QLabel { color: #006600; }')
        else:
            self.status_atmd_agent.setText('Stopped')
            self.status_atmd_agent.setStyleSheet('QLabel { color: #C80000; }')

        self.status_toggle.setValue((self.status_toggle.value()+25)%100)

    @pyqtSignature("")
    def on_rtnet_start_released(self):
        """
        Slot documentation goes here.
        """
        self.command_output.setPlainText('')
        if not self.rtnet_master and not self.rtnet_slave:
            # Start rtnet
            self.command_output.appendPlainText(" => Starting RTnet master:")
            out = commands.getstatusoutput('sudo rtnet.master start')
            self.command_output.appendPlainText(out[1])
            if out[0] != 0:
                self.error("Cannot start RTnet master")
            else:
                time.sleep(5.0)
                self.command_output.appendPlainText(" => Starting RTnet slave:")
                out = commands.getstatusoutput('ssh root@'+str(self.agent_host.text())+' "rtnet.slave start"')
                self.command_output.appendPlainText(out[1])
                if out[0] != 0:
                    self.error("Cannot start RTnet slave")
                else:
                    # Check packet delay
                    self.command_output.appendPlainText(" => Checking packet delay:")
                    out = commands.getstatusoutput('ssh root@'+str(self.agent_host.text())+' "dmesg | grep \'TDMA: calibrated\' | tail -n 1"')
                    self.command_output.appendPlainText(out[1])
        else:
            self.error("RTnet already running!")
    
    @pyqtSignature("")
    def on_rtnet_stop_released(self):
        """
        Slot documentation goes here.
        """
        self.command_output.setPlainText('')
        if self.atmd_agent or self.atmd_server:
            self.error("Cannot stop RTnet with the ATMD-GPX running.")
        else:
            # Stop rtnet
            if self.rtnet_slave:
                ans = commands.getstatusoutput('ssh root@'+str(self.agent_host.text())+' "rtnet.slave stop"')
                self.command_output.appendPlainText(ans[1])
                if ans[0] != 0:
                    self.error("Error stopping RTnet slave")
            if self.rtnet_master:
                ans = commands.getstatusoutput('sudo rtnet.master stop')
                self.command_output.appendPlainText(ans[1])
                if ans[0] != 0:
                    self.error("Error stopping RTnet master")

    @pyqtSignature("")
    def on_atdm_start_released(self):
        """
        Slot documentation goes here.
        """
        self.command_output.setPlainText('')
        if self.rtnet_master and self.rtnet_slave:
            if not self.atmd_agent and not self.atmd_server:
                # Start agent
                self.command_output.appendPlainText(" => Starting ATMD-GPX agent:")
                out = commands.getstatusoutput('ssh root@'+str(self.agent_host.text())+' "service atmd_agent start"')
                self.command_output.appendPlainText(out[1])
                if out[0] != 0:
                    self.error("Error starting ATMD-GPX agent.")
                else:
                    # Start server
                    time.sleep(5)
                    self.command_output.appendPlainText(" => Starting ATMD-GPX server:")
                    out = commands.getstatusoutput('sudo service atmd_server start')
                    self.command_output.appendPlainText(out[1])
                    if out[0] != 0:
                        self.error("Error starting ATMD-GPX server.")
                    else:
                        pass
            else:
                self.error("ATMD-GPX system already running.")
        else:
            self.error("Cannot start ATMD-GPX system without RTnet running.")

    @pyqtSignature("")
    def on_atmd_stop_released(self):
        """
        Slot documentation goes here.
        """
        self.command_output.setPlainText('')
        if self.atmd_server:
            out = commands.getstatusoutput('sudo service atmd_server stop')
            self.command_output.appendPlainText(out[1])
        # Check RT handles
        out = commands.getstatusoutput('ssh root@'+str(self.agent_host.text())+' "cat /proc/xenomai/rtdm/open_fildes | grep -v ^Index" | awk \'{print $1}\'')
        if out[0] == 0:
            if out[1] != '':
                id = map(int, out[1].split('\n'))
                out = commands.getstatusoutput('ssh root@'+str(self.agent_host.text())+' "term_rtdev '+str(min(id))+' '+str(max(id))+'"')
                if out[0] != 0:
                    self.error("Error closing RT handles.")
        else:
            self.error("Cannot access RT handle list.")
            
        if self.atmd_agent:
            out = commands.getstatusoutput('ssh root@'+str(self.agent_host.text())+' "service atmd_agent stop"')
            self.command_output.appendPlainText(out[1])
        # Check RT handles
        out = commands.getstatusoutput('cat /proc/xenomai/rtdm/open_fildes | grep -v ^Index | awk \'{print $1}\'')
        if out[0] == 0:
            if out[1] != '':
                id = map(int, out[1].split('\n'))
                out = commands.getstatusoutput('term_rtdev '+str(min(id))+' '+str(max(id)))
                if out[0] != 0:
                    self.error("Error closing RT handles.")
        else:
            self.error("Cannot access RT handle list.")
    
    @pyqtSignature("")
    def on_exit_button_released(self):
        """
        Slot documentation goes here.
        """
        self.close()
