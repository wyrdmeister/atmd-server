#!/usr/bin/env python3
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

import time
import shlex

try:
    from subprocess import check_output
    from subprocess import CalledProcessError
except ImportError as e:
    # Old python
    import commands

    class CalledProcessError(Exception):
        def __init__(self, code, output):
            Exception.__init__(self, "Error {0:d}".format(code))
            self.returncode = code
            self.output = output

    def check_output(command, shell=False):
        (code, out) = commands.getstatusoutput(command)
        if code == 0:
            return out
        else:
            raise CalledProcessError(code, out)


# Import PyQt
from PyQt4 import QtCore
from PyQt4 import QtGui

# Import UI
from ui.Ui_rtnet_control import Ui_rtnet_control


class RTNetControl(QtGui.QMainWindow, Ui_rtnet_control):
    """
    Main window class
    """
    def __init__(self, parent=None):
        """ Constructor
        """
        # Parent constructor
        QtGui.QMainWindow.__init__(self, parent)

        # Build UI
        self.setupUi(self)

        # Statuses
        self.rtnet_master = False
        self.rtnet_slave1 = False
        self.rtnet_slave2 = False
        self.atmd_server = False
        self.atmd_agent1 = False
        self.atmd_agent2 = False

        # Start timer
        self.startTimer(2000)

    def error(self, message, title="Error"):
        """ Display an error message
        """
        QtGui.QMessageBox.critical(self, title, message)

    def warning(self, message, title="Warning"):
        """ Display an error message
        """
        QtGui.QMessageBox.warning(self, title, message)

    @QtCore.pyqtSlot(QtCore.QTimerEvent)
    def timerEvent(self, event):
        """ Timer event handler
        """
        try:
            out = check_output('cat /proc/xenomai/rtdm/protocol_devices | grep PACKET_DGRAM', shell=True)
            self.rtnet_master = (out != '')
        except CalledProcessError as e:
            self.rtnet_master = False
            print ("Error: [{0:}] {1:}".format(e.returncode, e.output))

        try:
            out = check_output('ps -e | grep atmd_server', shell=True)
            self.atmd_server = (out != '')
        except CalledProcessError as e:
            self.atmd_server = False
            print ("Error: [{0:}] {1:}".format(e.returncode, e.output))

        try:
            out = check_output('ssh root@{0:} "cat /proc/xenomai/rtdm/protocol_devices | grep PACKET_DGRAM"'.format(shlex.quote(self.agent_host1.text())), shell=True)
            self.rtnet_slave1 = (out != '')
        except CalledProcessError as e:
            self.rtnet_slave1 = False
            print ("Error: [{0:}] {1:}".format(e.returncode, e.output))

        try:
            out = check_output('ssh root@{0:} "ps -e | grep atmd-agent"'.format(shlex.quote(self.agent_host1.text())), shell=True)
            self.atmd_agent1 = (out != '')
        except CalledProcessError as e:
            self.atmd_agent1 = False
            print ("Error: [{0:}] {1:}".format(e.returncode, e.output))

        if self.en_2nd.isChecked():
            try:
                out = check_output('ssh root@{0:} "cat /proc/xenomai/rtdm/protocol_devices | grep PACKET_DGRAM"'.format(shlex.quote(self.agent_host2.text())), shell=True)
                self.rtnet_slave2 = (out != '')
            except CalledProcessError as e:
                self.rtnet_slave2 = False
                print ("Error: [{0:}] {1:}".format(e.returncode, e.output))

            try:
                out = check_output('ssh root@{0:} "ps -e | grep atmd-agent"'.format(shlex.quote(self.agent_host2.text())), shell=True)
                self.atmd_agent2 = (out != '')
            except CalledProcessError as e:
                self.atmd_agent2 = False
                print ("Error: [{0:}] {1:}".format(e.returncode, e.output))
        else:
            self.rtnet_slave2 = True
            self.atmd_agent2 = True

        # Update GUI
        self.update_indicator()

    def update_indicator(self):
        if self.rtnet_master:
            self.status_rtnet_master.setText('Running')
            self.status_rtnet_master.setStyleSheet('QLabel { color: #006600; }')
        else:
            self.status_rtnet_master.setText('Stopped')
            self.status_rtnet_master.setStyleSheet('QLabel { color: #C80000; }')

        if self.rtnet_slave1 and self.rtnet_slave2:
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

        if self.atmd_agent1 and self.atmd_agent2:
            self.status_atmd_agent.setText('Running')
            self.status_atmd_agent.setStyleSheet('QLabel { color: #006600; }')
        else:
            self.status_atmd_agent.setText('Stopped')
            self.status_atmd_agent.setStyleSheet('QLabel { color: #C80000; }')

        val = self.status_toggle.value()+25;
        if val > 100:
            val = 0
        self.status_toggle.setValue(val)


    def start_rtnet_master(self, agents):
        """ Start RTNet master
        """
        self.command_output.appendPlainText(" => Starting RTnet master:")
        try:
            out = check_output('sudo rtnet.master start {0:d}'.format(agents), shell=True)
            self.command_output.appendPlainText(out)
            return 0
        except CalledProcessError as e:
            self.command_output.appendPlainText(e.output)
            return e.returncode

    def start_rtnet_slave(self, host, agents):
        """ Start RTNet slave
        """
        try:
            self.command_output.appendPlainText(" => Starting RTnet slave on {0:}:".format(host))
            out = check_output('ssh root@{0:} "rtnet.slave start {0:}"'.format(shlex.quote(host), agents), shell=True)
            self.command_output.appendPlainText(out)
            self.command_output.appendPlainText(" => Checking packet delay:")
            out = check_output('ssh root@{0:} "dmesg | grep \'TDMA: calibrated\' | tail -n 1"'.format(shlex.quote(host)), shell=True)
            self.command_output.appendPlainText(out)
            return 0
        except CalledProcessError as e:
            self.command_output.appendPlainText(e.output)
            return e.returncode

    def stop_rtnet_master(self):
        """ Stop RTNet master
        """
        try:
            self.command_output.appendPlainText(" => Stopping RTnet master:")
            out = check_output('sudo rtnet.master stop', shell=True)
            self.command_output.appendPlainText(out)
            return 0
        except CalledProcessError as e:
            self.command_output.appendPlainText(e.output)
            return e.returncode

    def stop_rtnet_slave(self, host):
        """ Stop RTNet slave
        """
        try:
            self.command_output.appendPlainText(" => Stopping RTnet slave on {0:}:".format(host))
            out = check_output('ssh root@{0:} "rtnet.slave stop"'.format(shlex.quote(host)), shell=True)
            self.command_output.appendPlainText(out)
            return 0
        except CalledProcessError as e:
            self.command_output.appendPlainText(e.output)
            return e.returncode

    @QtCore.pyqtSlot()
    def on_rtnet_start_released(self):
        """
        Start RTNet
        """
        # Number of agents
        agents = 1
        if self.en_2nd.isChecked():
            agents = 2

        # Clean log
        self.command_output.setPlainText('')

        if not self.rtnet_master and not self.rtnet_slave1 and not (self.en2nd.isChecked() and self.rtnet_slave2):
            # Start rtnet master
            if self.start_rtnet_master(agents) == 0:
                # Wait for the master to settle...
                time.sleep(5.0)

                # Start first slave
                if self.start_rtnet_slave(self.agent_host1.text(), agents) == 0:
                    # Success. Start second slave if needed
                    if self.en_2nd.isChecked():
                        if self.start_rtnet_slave(self.agent_host2.text(), agents) == 0:
                            # Success
                            return
                        else:
                            self.error("Failed to start RTNet slave on {0:}".format(self.agent_host2.text()))
                            self.stop_rtnet_slave(self.agent_host2.text())
                            self.stop_rtnet_master()
                else:
                    self.error("Failed to start RTNet slave on {0:}".format(self.agent_host1.text()))
                    self.stop_rtnet_master()
            else:
                self.error("Failed to start RTNet master")
        else:
            self.error("RTnet already running!")

    @QtCore.pyqtSlot()
    def on_rtnet_stop_released(self):
        """
        Stop RTNet
        """
        self.command_output.setPlainText('')
        if self.atmd_agent or self.atmd_server:
            self.error("Cannot stop RTnet with the ATMD-GPX server running.")
        else:
            # Stop rtnet
            if self.rtnet_slave1:
                if self.stop_rtnet_slave(self.agent_host1.text()):
                    self.error("Faild to stop RTNet slave on {0:}".format(self.agent_host1.text()))
            if self.en_2nd.isChecked() and self.rtnet_slave2:
                if self.stop_rtnet_slave(self.agent_host2.text()):
                    self.error("Faild to stop RTNet slave on {0:}".format(self.agent_host2.text()))
            if self.rtnet_master:
                if self.stop_rtnet_master():
                    self.error("Error stopping RTnet master")

    def start_amtd_agent(self, host):
        """ Start ATMD agent
        """
        try:
            self.command_output.appendPlainText(" => Starting ATMD-GPX agent on {0:}:".format(host))
            out = check_output('ssh root@{0:} "service atmd_agent start"'.format(shlex.quote(host)), shell=True)
            self.command_output.appendPlainText(out)
            return 0
        except CalledProcessError as e:
            self.command_output.appendPlainText(e.output)
            return e.returncode

    def start_atmd_server(self):
        """ Start ATMD server
        """
        try:
            self.command_output.appendPlainText(" => Starting ATMD-GPX server:")
            out = check_output('sudo service atmd_server start', shell=True)
            self.command_output.appendPlainText(out)
            return 0
        except CalledProcessError as e:
            self.command_output.appendPlainText(e.output)
            return e.returncode

    def stop_atmd_agent(self, host):
        """ Stop ATMD agent
        """
        try:
            self.command_output.appendPlainText(" => Stopping ATMD-GPX agent on {0:}:".format(host))
            out = check_output('ssh root@{0:} "service atmd_agent stop"'.format(shlex.quote(host)), shell=True)
            self.command_output.appendPlainText(out)
            # Check RT file descriptors
            out = check_output('ssh root@{0:} "cat /proc/xenomai/rtdm/open_fildes | grep -v ^Index" | awk \'{print $1}\''.format(shlex.quote(host)), shell=True)
            if out != '':
                id = map(int, out.split('\n'))
                out = check_output('ssh root@{0:} "term_rtdev {1:d} {2:d}"'.format(host, min(id), max(id)), shell=True)
            return 0

        except CalledProcessError as e:
            self.command_output.appendPlainText(e.output)
            return e.returncode

    def stop_atmd_server(self):
        """ Stop ATMD server
        """
        try:
            self.command_output.appendPlainText(" => Stopping ATMD-GPX server:")
            out = check_output('sudo service atmd_server stop', shell=True)
            self.command_output.appendPlainText(out)
            # Check RT file descriptors
            out = check_output('cat /proc/xenomai/rtdm/open_fildes | grep -v ^Index | awk \'{print $1}\'', shell=True)
            if out != '':
                id = map(int, out.split('\n'))
                out= check_output('term_rtdev {0:d} {1:d}'.format(min(id), max(id)), shell=True)
            return 0
        except CalledProcessError as e:
            self.command_output.appendPlainText(e.output)
            return e.returncode

    @QtCore.pyqtSlot()
    def on_atdm_start_released(self):
        """ Start ATMD-GPX system
        """
        self.command_output.setPlainText('')
        if self.rtnet_master and self.rtnet_slave:
            if not self.atmd_agent1 and not (self.en2nd.isChecked() and self.atmd_agent2) and not self.atmd_server:
                # Start first agent
                if self.start_atmd_agent(self.agent_host1.text()) == 0:
                    # Success. Start second agent if needed
                    if self.en_2nd.isChecked():
                        if self.start_atmd_agent(self.agent_host2.text()):
                            # Failed
                            self.stop_atmd_agent(self.agent_host1.text())
                            self.error("Error starting ATMD-GPX agent on {0:}".format(self.agent_host2.text()))
                            return
                    # Start server
                    if self.start_atmd_server():
                        # Failed... stop agents
                        self.stop_atmd_agent(self.agent_host1.text())
                        if self.en_2nd.isChecked():
                            self.stop_atmd_agent(self.agent_host2.text())
                        self.error("Error starting ATMD-GPX server")
                else:
                    # Failed...
                    self.error("Error starting ATMD-GPX agent on {0:}".format(self.agent_host1.text()))
            else:
                self.error("ATMD-GPX system already running.")
        else:
            self.error("Cannot start ATMD-GPX system without RTnet running.")

    @QtCore.pyqtSlot()
    def on_atmd_stop_released(self):
        """ Stop ATMD-GPX system
        """
        self.command_output.setPlainText('')
        if self.atmd_server:
            if self.stop_atmd_server():
              self.error("Error stoppng ATMD-GPX server")
        if self.atmd_agent1:
            if self.stop_atmd_agent(self.agent_host1.text()):
                self.error("Error stopping ATMD-GPX agent on {0:}".format(self.agent_host1.text()))
        if self.en_2nd.isChecked() and self.atmd_agent2:
            if self.stop_atmd_agent(self.agent_host2.text()):
                self.error("Error stopping ATMD-GPX agent on {0:}".format(self.agent_host2.text()))

    @QtCore.pyqtSlot()
    def on_exit_button_released(self):
        """ Exit button callback
        """
        self.close()

    @QtCore.pyqtSlot(bool)
    def on_en_2nd_toggled(self, checked):
        if checked:
            self.agent_host2.setDisabled(False)
        else:
            self.agent_host2.setDisabled(True)


if __name__ == "__main__":
    import sys
    app = QtGui.QApplication(sys.argv)
    ui = RTNetControl()
    ui.show()
    sys.exit(app.exec_())