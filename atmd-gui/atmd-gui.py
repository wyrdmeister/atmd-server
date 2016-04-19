#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""

ATMD Server Control GUI

Copyright (C) Michele Devetta 2016 <michele.devetta@gmail.com>

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

import socket
import re
import math
import time
from select import select
from PyQt4 import QtCore
from PyQt4 import QtGui
from ui.Ui_atmd_gui import Ui_AtmdGUI
from ui.Ui_savemeasure import Ui_savemeasure
from ui.Ui_dtcalculator import Ui_dtcalculator


class ATMDGui(QtGui.QMainWindow, Ui_AtmdGUI):
    """ Main window of ATMD-GPX client
    """

    def __init__(self, parent=None):
        """ Constructor
        """
        # Parent constructor
        QtGui.QMainWindow.__init__(self, parent)

        # Build UI
        self.setupUi(self)

        # Init
        self.sock = None
        self.buffer = ''
        self.command_log.setReadOnly(True)
        self.connect_button.setEnabled(True)
        self.disconnect_button.setEnabled(False)
        self.status_label.setText('Disconnected')
        self.start_time = 0
        self.meas_time = 0

        # Valid commands
        self.send_cmd = ("SET", "GET", "MSR", "EXT")
        self.recv_cmd = ("ACK", "VAL", "MSR", "ERR")

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
        """ Timer event handler
        """
        # Get board status
        ans = self.send_command("MSR STATUS", True)
        if not ans:
            return

        # Update indicator
        self.status_label.setText(ans[0])

        # Re-enable start button if IDLE
        if ans[0] == "IDLE" and not self.start_button.isEnabled():
            self.start_button.setDisabled(False)
            self.stop_button.setDisabled(True)
            self.meas_time = 0
            self.start_time = 0

        elif ans[0] == "RUNNING":
            self.stop_button.setDisabled(False)

        if self.start_time:
            tt = float(self.meas_time - (time.time() - self.start_time))

            hh = math.trunc(tt / 3600.0);
            mm = math.trunc((tt - hh*3600.0) / 60.0)
            ss = tt - (hh * 3600) - (mm * 60)
            if tt >= 0:
                self.timeleft.setStyleSheet("color:black; font-weight:normal")
                timestr = "%02d:%02d:%02d" % (hh, mm, ss)
            else:
                self.timeleft.setStyleSheet("color:red; font-weight:bold")
                timestr = "- %02d:%02d:%02d" % (abs(hh), abs(mm), abs(ss))
            self.timeleft.setText(timestr)
        else:
            self.timeleft.setStyleSheet("color:black; font-weight:normal")
            self.timeleft.setText("00:00:00")

    def write_config(self):
        """ Send configuration to remote server
        """
        # Acquisition times
        if self.measure_time.text() != '0u':
            ans = self.send_command("SET TT %s" % (str(self.measure_time.text()), ))
            if not ans:
                self.error("Error configuring measure time.")
        if self.window_time.text() != '0u':
            ans = self.send_command("SET ST %s" % (str(self.window_time.text()), ))
            if not ans:
                self.error("Error configuring window time.")
        if self.deadtime.text() != '0u':
            ans = self.send_command("SET TD %s" % (str(self.deadtime.text()), ))
            if not ans:
                self.error("Error configuring deadtime.")
        # Autosave mode
        if self.automatic_save.isChecked():
            if self.save_prefix.text() != '':
                ans = self.send_command("SET PREFIX %s" % (str(self.save_prefix.text()), ))
                if not ans:
                    self.error("Error configuring save prefix.")
            else:
                self.error("In autosave mode a prefix is required!")
            if self.autosave_count.text().toInt()[0] != 0:
                ans = self.send_command("SET AUTOSAVE %d" % (self.autosave_count.text().toInt()[0], ))
                if not ans:
                    self.error("Error configuring autosave count.")
            else:
                self.error("In autosave mode an autosave count is required!")
        # Manual mode
        else:
            ans = self.send_command("SET AUTOSAVE 0")
            if not ans:
                self.error("Error disabling autosave.")
        # Set save format
        ans = self.send_command("SET FORMAT 12")
        if not ans:
            self.error("Error configuring save format.")
        # Configure monitor
        if self.monitor_count.text().toInt()[0] != 0 and self.monitor_save.text().toInt()[0] != 0 and self.monitor_name.text() != '':
            ans = self.send_command("SET MONITOR %d %d %s" % (self.monitor_save.text().toInt()[0], self.monitor_count.text().toInt()[0], str(self.monitor_name.text())))
            if not ans:
                self.error("Error configuring monitor.")
        else:
            self.send_command("SET NOMONITOR")

    def read_config(self):
        """ Read configuration from remote server
        """
        ans = self.send_command("GET TT")
        if ans:
            self.measure_time.setText(ans[0])
        ans = self.send_command("GET ST")
        if ans:
            self.window_time.setText(ans[0])
        ans = self.send_command("GET TD")
        if ans:
            self.deadtime.setText(ans[0])
        ans = self.send_command("GET PREFIX")
        if ans:
            self.save_prefix.setText(ans[0])
        ans = self.send_command("GET AUTOSAVE")
        if ans:
            self.autosave_count.setText(str(ans[0]))
        ans = self.send_command("GET MONITOR")
        if ans:
            self.monitor_save.setText(str(ans[0]))
            self.monitor_count.setText(str(ans[1]))
            self.monitor_name.setText(ans[2])
        ans = self.send_command("GET AGENTS")
        self.agents_num.setText(str(len(ans)))
        self.agents_addresses.setPlainText("")
        if ans:
            if len(ans) > 0:
                for i in range(len(ans)):
                    self.agents_addresses.appendPlainText(ans[i][1])

    def send_command(self, command, nolog=False):
        """ Send a command to the remote server a return response
        """
        # Check command
        good = False
        for c in self.send_cmd:
            if c == command[:3]:
                good = True
                break
        if not good:
            self.error("Sending wrong command '%s'" % (command, ))
            return None

        # Send command
        try:
            self.sock.sendall(command + "\n")
            if not nolog:
                self.command_log.appendPlainText("S: %s" % (command, ))
            #self.emit(QtCore.SIGNAL("msg(QString)"),QtCore.QString("S: "+command))
        except socket.error as msg:
            self.error(str(msg), "Network error")
            return None

        # Receive answer
        ans = self.get_command()
        if ans:
            if not nolog:
                self.command_log.appendPlainText("R: %s" % (" ".join(ans), ))
            #self.emit(QtCore.SIGNAL("msg(QString)"),QtCore.QString("R: "+" ".join(ans)))
            # Interpret answer
            if command[:3] == 'SET':
                if ans[0] == 'ACK':
                    # Command was successful
                    return (1, )
                elif ans[0] == 'ERR':
                    # An error occurred
                    self.error("The command '%s' returned error '%s'" % (command, ans[1]))
                else:
                    self.error("The command '%s' returned an unexpected response '%s'" % (command, " ".join(ans)))

            elif command[:3] == 'GET':
                id = ans[1].find(" ")
                if ans[1][:id] == 'AGENTS':
                    p = re.compile(r"AGENTS (?P<num>\d+)")
                    m = p.match(ans[1])
                    if m:
                        num_agents = int(m.groupdict()['num'])
                        ag_lst = []
                        for i in range(num_agents):
                            ans = self.get_command()
                            if ans:
                                id = ans[1].find(" ")
                                if ans[1][:id] == 'AGENT':
                                    p = re.compile(r"AGENT (?P<id>\d+) (?P<mac>\w+\:\w+\:\w+\:\w+:\w+:\w+)")
                                    m = p.match(ans[1])
                                    if m:
                                        ag_lst.append((int(m.groupdict()['id']), m.groupdict()['mac']))
                                    else:
                                        self.error("Got malformed agent information.")
                                else:
                                    pass
                            else:
                                pass
                        return tuple(ag_lst)
                    else:
                        self.error("Got malformed angent list.")
                elif ans[1][:id] == 'CHS':
                    pass
                elif ans[1][:id] == 'CH':
                    pass
                elif ans[1][:id] == 'TT' or ans[1][:id] == 'ST' or ans[1][:id] == 'TD':
                    p = re.compile(r"%s (?P<time>\d+\.?\d*[umsMh]{1})" % (ans[1][:id], ))
                    m = p.match(ans[1])
                    if m:
                        return (m.groupdict()['time'], )
                    else:
                        self.error("Got a malformed timestring '%s'." % (ans[id + 1:], ))
                elif ans[1][:id] == 'RS':
                    pass
                elif ans[1][:id] == 'OF':
                    pass
                elif ans[1][:id] == 'HOST':
                    pass
                elif ans[1][:id] == 'USER':
                    pass
                elif ans[1][:id] == 'PSW':
                    pass
                elif ans[1][:id] == 'PREFIX':
                    p = re.compile(r"PREFIX (?P<prefix>[a-zA-Z0-9\.\_\-\/]+)")
                    m = p.match(ans[1])
                    if m:
                        if m.groupdict()['prefix'] == 'NONE':
                            return ('', )
                        else:
                            return (m.groupdict()['prefix'], )
                    else:
                        self.error("Got a malformed prefix '%s'." % (ans[id + 1:], ))
                elif ans[1][:id] == 'FORMAT':
                    pass
                elif ans[1][:id] == 'AUTOSAVE':
                    p = re.compile(r"AUTOSAVE (?P<count>\d+)")
                    m = p.match(ans[1])
                    if m:
                        return (m.groupdict()['count'], )
                    else:
                        self.error("Got a malformed autosave count '%s'." % (ans[id + 1:], ))
                elif ans[1][:id] == 'MONITOR':
                    p = re.compile(r"MONITOR (?P<save>\d+) (?P<count>\d+)\s*(?P<file>[a-zA-Z0-9\.\_\-\/]*)")
                    m = p.match(ans[1])
                    if m:
                        return (int(m.groupdict()['save']), int(m.groupdict()['count']), m.groupdict()['file'])
                    else:
                        self.error("Got a malformed monitor configuration '%s'." % (ans[id + 1:], ))
                else:
                    pass

            elif command[:3] == 'MSR':
                if ans[0] == 'ACK':
                    # Command was successful
                    return (1, )
                elif ans[0] == 'ERR':
                    # An error occurred
                    self.error("The command '%s' returned error '%s'" % (command, ans[1]))
                elif ans[0] == 'MSR':
                    id = ans[1].find(" ")
                    # Status and list
                    if ans[1][:id] == 'STATUS':
                        return (ans[1][id + 1:], )
                    elif ans[1][:id] == 'LST':
                        pt = re.compile(r"LST NUM (\d+)")
                        mt = pt.match(ans[1])
                        if mt:
                            num = int(mt.group(1))
                            meas = []
                            for i in range(num):
                                ans = self.get_command()
                                pt = re.compile(r"LST (\d+) (\d+)")
                                mt = pt.match(ans[1])
                                if mt:
                                    meas.append((int(mt.group(1)), int(mt.group(2))))
                                else:
                                    meas.append((0, 0))
                            return (num, meas)
                        else:
                            return (0, [])
                    elif ans[1][:id] == 'STAT':
                        pt = re.compile(r"STAT (\d+) (\d+) (\d+) (\d+) (\d+) (\d+) (\d+) (\d+) (\d+) (\d+)")
                        mt = pt.match(ans[1])
                        if mt:
                            return (mt.group(1), mt.group(2), mt.group(3), mt.group(4), mt.group(5), mt.group(6), mt.group(7), mt.group(8), mt.group(9), mt.group(10))
                        else:
                            return (0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
                    else:
                        pass
                else:
                    self.error("The command '%s' returned an unexpected response '%s'" % (command, " ".join(ans)))

            elif command[:3] == 'EXT':
                pass
        return None

    def get_command(self):
        """ Read a command from the receive buffer
        """
        while True:
            id = self.buffer.find("\r\n")
            if id > 0:
                cmd = self.buffer[:id]
                self.buffer = self.buffer[(id + 2):]
                for c in self.recv_cmd:
                    if c == cmd[:3]:
                        return (cmd[:3], cmd[4:])
            else:
                if id == 0:
                    self.buffer = self.buffer[2:]
                try:
                    ready = select([self.sock], [], [], 5)
                    if len(ready[0]) > 0:
                        self.buffer += self.sock.recv(4096)
                    else:
                        # No answer
                        self.error("Timed out waiting for a command.")
                        return None
                except socket.error as msg:
                    self.error(str(msg), "Network error")
                    return None

    @QtCore.pyqtSlot()
    def on_config_button_released(self):
        """ Send configuration to server
        """
        self.write_config()

    @QtCore.pyqtSlot()
    def on_read_button_released(self):
        """ Send configuration to server
        """
        self.read_config()

    @QtCore.pyqtSlot()
    def on_connect_button_released(self):
        """ Start connection to remote server
        """
        try:
            # Create socket
            self.sock = socket.create_connection((str(self.server_address.text()), self.server_port.text().toInt()[0]))
            self.connect_button.setEnabled(False)
            self.disconnect_button.setEnabled(True)

            # Enable buttons
            if self.manual_save.isChecked():
                self.save_button.setEnabled(True)
                self.clear_button.setEnabled(True)
            self.config_button.setEnabled(True)
            self.read_button.setEnabled(True)
            self.start_button.setEnabled(True)

            # Start update timer
            self.timer = self.startTimer(2000)
            self.read_config()

        except socket.error as msg:
            self.error(str(msg), "Network error")

    @QtCore.pyqtSlot()
    def on_disconnect_button_released(self):
        """ Close connection to remote server
        """
        # Stop timer
        self.killTimer(self.timer)
        # Close socket
        self.sock.close()
        self.sock = None
        self.connect_button.setEnabled(True)
        self.disconnect_button.setEnabled(False)
        self.status_label.setText("Disconnected")
        # Disable buttons
        self.save_button.setDisabled(True)
        self.clear_button.setDisabled(True)
        self.config_button.setDisabled(True)
        self.read_button.setDisabled(True)
        self.start_button.setDisabled(True)
        self.stop_button.setDisabled(True)

    @QtCore.pyqtSlot()
    def on_start_button_released(self):
        """ Start a new measure
        """
        pt = re.compile("(\d+\.?\d*)([umsMh]?)")
        mt = pt.match(str(self.measure_time.text()))
        if mt:
            unit = mt.group(2)
            if unit == 's':
                self.meas_time = float(mt.group(1))
            elif unit == 'M':
                self.meas_time = float(mt.group(1))*60
            elif unit == 'h':
                self.meas_time = float(mt.group(1))*3600
            else:
                self.meas_time = 0
        else:
            self.meas_time = 0
        self.send_command("MSR START")
        self.start_button.setDisabled(True)
        self.start_time = time.time()

    @QtCore.pyqtSlot()
    def on_stop_button_released(self):
        """ Stop the current measure
        """
        self.send_command("MSR STOP")

    @QtCore.pyqtSlot()
    def on_exit_button_released(self):
        """ Exit button callback
        """
        self.close()

    @QtCore.pyqtSlot(bool)
    def on_manual_save_toggled(self, checked):
        """ Manual mode callback
        """
        if checked and self.sock:
            self.save_button.setDisabled(False)
            self.clear_button.setDisabled(False)
        else:
            self.save_button.setDisabled(True)
            self.clear_button.setDisabled(True)

    @QtCore.pyqtSlot(bool)
    def on_automatic_save_toggled(self, checked):
        """ Automatic mode callback
        """
        if checked:
            self.autosave_count.setDisabled(False)
            self.save_prefix.setDisabled(False)
        else:
            self.autosave_count.setDisabled(True)
            self.save_prefix.setDisabled(True)

    @QtCore.pyqtSlot()
    def on_save_button_released(self):
        """ Save button callback
        """
        # Create dialog
        dl = SaveMeasure(self.sock, self)
        dl.show()

    @QtCore.pyqtSlot()
    def on_clear_button_released(self):
        """ Clear button callback
        """
        ans = QtGui.QMessageBox.question(
                        self,
                        "Clear measures",
                        "Are you sure to clear all measures?",
                        QtGui.QMessageBox.Yes | QtGui.QMessageBox.Yes,
                        QtGui.QMessageBox.No);
        if ans == QtGui.QMessageBox.Yes:
            self.send_command("MSR CLR")

    QtCore.pyqtSlot()
    def on_dt_calculate_released(self):
        """ Open deadtime calculator.
        """
        dl = DTcalculator(self)
        dl.show()


class SaveMeasure(QtGui.QDialog, Ui_savemeasure):

    """ Save measure dialog
    """

    def __init__(self, sock, parent=None):
        """ Constructor
        """
        # Parent constructor
        QtGui.QDialog.__init__(self, parent)

        # Build Ui
        self.setupUi(self)

        # Save socket
        self.parent = parent
        self.sock = sock

        # Get measure list
        mlist = self.parent.send_command("MSR LST")
        for i in range(mlist[0]):
            ans = self.parent.send_command("MSR STAT -%d" % (i, ))
            mlist[1][i] = mlist[1][i] + ans[1:]

        # Setup QTableView
        model = MeasureModel(mlist[1], self)
        self.meas_list.setModel(model)
        self.meas_list.verticalHeader().hide()
        self.meas_list.horizontalHeader().setResizeMode(QtGui.QHeaderView.ResizeToContents)
        self.meas_list.setSelectionBehavior(QtGui.QAbstractItemView.SelectRows)
        self.meas_list.setSelectionMode(QtGui.QAbstractItemView.SingleSelection)

    QtCore.pyqtSlot()
    def on_pb_browse_released(self):
        """ Browse button callback
        """
        filename = QtGui.QFileDialog.getSaveFileName(
                            self,
                            "Save measure to...",
                            "/home/data",
                            "*.mat")
        filename = self.check_filename(filename)
        if filename:
            self.filename.setText(filename)

    def check_filename(self, filename):
        """ Check save filename
        """
        # Check that the filename is in /home/data
        pt = re.compile(r"^/home/data/(.+)")
        mt = pt.match(filename)
        if not mt:
            QtGui.QMessageBox.critical(
                        self,
                        "Filename error",
                        "The selected save file must be under /home/data.")
            return False
        filename = mt.group(1)

        # Check extension
        pt = re.compile(r".*\.mat$")
        if not pt.match(filename):
            ans = QtGui.QMessageBox.question(
                            self,
                            "Missing extension",
                            "Data files should have the .mat extension.\nDo you want to add it?",
                            QtGui.QMessageBox.Yes | QtGui.QMessageBox.No,
                            QtGui.QMessageBox.Yes)
            if ans == QtGui.QMessageBox.Yes:
                filename += ".mat"

        return filename

    QtCore.pyqtSlot()
    def on_pb_save_released(self):
        """ Save button callback
        """
        ind = self.meas_list.selectedIndexes()
        if len(ind) > 0:
            r = -1
            for i in range(len(ind)):
                if r == -1:
                    r = ind[i].row()
                elif r != ind[i].row():
                    QtGui.QMessageBox.critical(
                            self,
                            "Selction error",
                            "There was an error in the measure selection.\nPlease try to select it again.")
                    return
        else:
            QtGui.QMessageBox.warning(
                    self,
                    "Select measure",
                    "You must select a measurement.")
            return

        print "Saving measure %d..." % (r, )
        filename = self.check_filename(str("/home/data/"+self.filename.text()))
        if filename:
            ans = self.parent.send_command("MSR SAV %d %s" % (r, filename))
            if ans:
                QtGui.QMessageBox.information(
                                self,
                                "Save successfull",
                                "The measure was saved successfully.")
                self.close()

    QtCore.pyqtSlot()
    def on_pb_cancel_released(self):
        """ Cancel button callback
        """
        self.close()


class MeasureModel(QtCore.QAbstractTableModel):

    """ Table model for the list of measures. """

    def __init__(self, measures, parent=None):
        """ Constructor
        """
        # Parent constructor
        QtCore.QAbstractTableModel.__init__(self, parent)

        # Measure list
        self.measures = measures

    def rowCount(self, parent=QtCore.QModelIndex()):
        """ Return row count
        """
        return len(self.measures)

    def columnCount(self, parent=QtCore.QModelIndex()):
        """ Return column count
        """
        return 11

    def data(self, index, role=QtCore.Qt.DisplayRole):
        """ Return model data
        """
        if index.isValid() and role == QtCore.Qt.DisplayRole and index.row() < self.rowCount() and index.column() < self.columnCount():
            return QtCore.QVariant(self.measures[index.row()][index.column()])
        return QtCore.QVariant()

    def headerData(self, section, orientation, role=QtCore.Qt.DisplayRole):
        """ Return table headers
        """
        if orientation == QtCore.Qt.Horizontal and role == QtCore.Qt.DisplayRole:
            if section == 0:
                return QtCore.QVariant("Measure ID")
            elif section == 1:
                return QtCore.QVariant("Num. of starts")
            elif section == 2:
                return QtCore.QVariant("Window time")
            elif section == 3:
                return QtCore.QVariant("Ch. 1")
            elif section == 4:
                return QtCore.QVariant("Ch. 2")
            elif section == 5:
                return QtCore.QVariant("Ch. 3")
            elif section == 6:
                return QtCore.QVariant("Ch. 4")
            elif section == 7:
                return QtCore.QVariant("Ch. 5")
            elif section == 8:
                return QtCore.QVariant("Ch. 6")
            elif section == 9:
                return QtCore.QVariant("Ch. 7")
            elif section == 10:
                return QtCore.QVariant("Ch. 8")
        return QtCore.QVariant()


class DTcalculator(QtGui.QDialog, Ui_dtcalculator):

    def __init__(self, parent=None):
        """ Constructor
        """
        # Parent constructor
        QtGui.QDialog.__init__(self, parent)

        # Build UI
        self.setupUi(self)

    def compute_dt(self):
        """ Compute deadtime
        """
        try:
            num_pk = math.ceil((float(self.counts.text())*float(self.win_time.text()) - 163) / 165)
            cycles = num_pk / float(self.tdmaslots.text())
            self.deadtime.setText("%.1f" % (cycles * float(self.cycle.text()), ))
        except:
            self.deadtime.setText("n.a.")

    @QtCore.pyqtSlot(QtCore.QString)
    def on_counts_textChanged(self, text):
        """ ... """
        self.compute_dt()

    @QtCore.pyqtSlot(QtCore.QString)
    def on_tdmaslots_textChanged(self, text):
        """ ... """
        self.compute_dt()

    @QtCore.pyqtSlot(QtCore.QString)
    def on_cycle_textChanged(self, text):
        """ ... """
        self.compute_dt()

    @QtCore.pyqtSlot(QtCore.QString)
    def on_win_time_textChanged(self, text):
        """ ... """
        self.compute_dt()

    @QtCore.pyqtSlot()
    def on_pb_close_released(self):
        """ Close dialog
        """
        self.close()


if __name__ == "__main__":
    import sys
    app = QtGui.QApplication(sys.argv)
    ui = ATMDGui()
    ui.show()
    sys.exit(app.exec_())