#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""

RTnet Control GUI

Copyright (C) Michele Devetta 2013 <michele.devetta@gmail.com>

This program is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program.  If not, see <http://www.gnu.org/licenses/>.

"""

from PyQt4 import QtGui
from PyQt4 import QtCore
from ui.Ui_MainWindow import Ui_MainWindow
import commands, time


class RTnetControl(QtGui.QMainWindow, Ui_MainWindow):
    """
    Class documentation goes here.
    """

    sig_status = QtCore.pyqtSignal()

    def __init__(self, parent=None):
        """ Constructor. """
        # Parent constructor
        QtGui.QMainWindow.__init__(self, parent)

        # Setup UI
        self.setupUi(self)

        # Statuses
        self.rtnet_master = False
        self.rtnet_slave = False
        self.atmd_server = False
        self.atmd_agent = False

        # Connect signal to update status
        self.sig_status.connect(self.update_indicator)

        # Start QWidget timer
        self.startTimer(3000)

    def error(self, message, title="Error"):
        """ Display an error message
        """
        QtGui.QMessageBox.critical(self, title, message)

    def warning(self, message, title="Warning"):
        """ Display a warning message
        """
        QtGui.QMessageBox.warning(self, title, message)

    @QtCore.pyqtSlot(QtCore.QTimerEvent)
    def timerEvent(self, event):
        """ timerEvent handler
        Update statuses of RTnet and ATMD server
        """
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

        out = commands.getstatusoutput('ssh root@'+str(self.agent_host.text())+' "cat /proc/xenomai/rtdm/protocol_devices | grep PACKET_DGRAM"')
        if out[0] == 0 or (out[0] == 256 and out[1] == ''):
            self.rtnet_slave = (out[1] != '')
        else:
            print "Error: ["+str(out[0])+"]" + str(out[1])

        out = commands.getstatusoutput('ssh root@'+str(self.agent_host.text())+' "ps -e | grep atmd-agent"')
        if out[0] == 0 or (out[0] == 256 and out[1] == ''):
            self.atmd_agent = (out[1] != '')
        else:
            print "Error: ["+str(out[0])+"]" + str(out[1])

        self.sig_status.emit()

    @QtCore.pyqtSlot()
    def update_indicator(self):
        """ Update indicators.
        """
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

    @QtCore.pyqtSlot()
    def on_rtnet_start_released(self):
        """ RTnet start button callback.
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

    @QtCore.pyqtSlot()
    def on_rtnet_stop_released(self):
        """ RTnet stop button callback.
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

    @QtCore.pyqtSlot()
    def on_atdm_start_released(self):
        """ ATMD server start button callback.
        """
        self.command_output.setPlainText('')
        # Check that server port is not in TIME_WAIT
        out = commands.getstatusoutput('netstat -nt4 | grep :2606 | grep TIME_WAIT')
        if out[1] != '':
           self.warning("ATMD-GPX server port still in TIME_WAIT status.\nPlease wait for the port to close and then try again.")
           return

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

    @QtCore.pyqtSlot()
    def on_atmd_stop_released(self):
        """ ATMD server stop button callback.
        """
        self.command_output.setPlainText('')
        if self.atmd_server:
            out = commands.getstatusoutput('sudo service atmd_server stop')
            self.command_output.appendPlainText(out[1])
        # Check RT handles
        out = commands.getstatusoutput('cat /proc/xenomai/rtdm/open_fildes | grep -v ^Index | awk \'{print $1}\'')
        if out[0] == 0:
            if out[1] != '':
                id = map(int, out[1].split('\n'))
                out = commands.getstatusoutput('sudo term_rtdev '+str(min(id))+' '+str(max(id)))
                if out[0] != 0:
                    self.error("Error closing ATMD server RT handles.")
        else:
            self.error("Cannot access ATMD server RT handle list.")

        if self.atmd_agent:
            out = commands.getstatusoutput('ssh root@'+str(self.agent_host.text())+' "service atmd_agent stop"')
            self.command_output.appendPlainText(out[1])
        # Check RT handles
        out = commands.getstatusoutput('ssh root@'+str(self.agent_host.text())+' "cat /proc/xenomai/rtdm/open_fildes | grep -v ^Index" | awk \'{print $1}\'')
        if out[0] == 0:
            if out[1] != '':
                id = map(int, out[1].split('\n'))
                out = commands.getstatusoutput('ssh root@'+str(self.agent_host.text())+' "term_rtdev '+str(min(id))+' '+str(max(id))+'"')
                if out[0] != 0:
                    self.error("Error closing ATMD agent RT handles.")
        else:
            self.error("Cannot access ATMD agent RT handle list.")




    @QtCore.pyqtSlot()
    def on_exit_button_released(self):
        """ Exit button callback.
        """
        self.close()


if __name__ == "__main__":
    import sys
    app = QtGui.QApplication(sys.argv)
    ui = RTnetControl()
    ui.show()
    sys.exit(app.exec_())