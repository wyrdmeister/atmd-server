# -*- coding: utf-8 -*-

"""
Module implementing mainWindow.
"""

from PyQt4.QtCore import pyqtSignature, pyqtSlot
from PyQt4 import QtCore
from PyQt4.QtGui import QMainWindow, QMessageBox
import socket, re, threading
from select import select

from .Ui_mainwindow import Ui_mainWindow


class mainWindow(QMainWindow, Ui_mainWindow):
    """ Main window of ATMD-GPX client
    """
    
    # Signal to update the server status
    sig_status = QtCore.pyqtSignal(QtCore.QString)
    
    def __init__(self, parent=None):
        """ Constructor
        """
        QMainWindow.__init__(self, parent)
        self.setupUi(self)
        
        # Init
        self.sock = None
        self.buffer = ''
        self.command_log.setReadOnly(True)
        self.connect_button.setEnabled(True)
        self.disconnect_button.setEnabled(False)
        self.status_label.setText('Unknown')
        
        # Connect signal to update command log
        self.sig_status.connect(self.update_status)

        # Valid commands
        self.send_cmd = ("SET", "GET", "MSR", "EXT")
        self.recv_cmd = ("ACK",  "VAL",  "MSR",  "ERR")

    def error(self, message, title="Error"):
        """ Display an error message
        """
        QMessageBox.critical(self, title, message)
        
    def warning(self, message, title="Warning"):
        """ Display an error message
        """
        QMessageBox.warning(self, title, message)
        
    def get_status(self):
        ans = self.send_command("MSR STATUS", True)
        if ans:
            self.sig_status.emit(QtCore.QString(ans[0]))
        else:
            self.polltimer.cancel()
    
    @pyqtSlot(QtCore.QString)
    def update_status(self, text):
        self.status_label.setText(text)

        
    def write_config(self):
        """ Send configuration to remote server
        """
        if self.measure_time.text() != '0u':
            ans = self.send_command("SET TT "+str(self.measure_time.text()))
            if not ans:
                self.error("Error configuring measure time.")
        if self.window_time.text() != '0u':
            ans = self.send_command("SET ST "+str(self.window_time.text()))
            if not ans:
                self.error("Error configuring window time.")
        if self.deadtime.text() != '0u':
            ans = self.send_command("SET TD "+str(self.deadtime.text()))
            if not ans:
                self.error("Error configuring deadtime.")
        if self.save_prefix.text() != '':
            ans = self.send_command("SET PREFIX "+str(self.save_prefix.text()))
            if not ans:
                self.error("Error configuring save prefix.")
        ans = self.send_command("SET FORMAT 12")
        if not ans:
            self.error("Error configuring save format: '"+ans[0])
        if self.autosave_count.text().toInt()[0] != 0:
            ans = self.send_command("SET AUTOSAVE "+str(self.autosave_count.text().toInt()[0]))
            if not ans:
                self.error("Error configuring autosave count.")
        if self.monitor_count.text().toInt()[0] != 0 and self.monitor_save.text().toInt()[0] != 0 and self.monitor_name.text() != '':
            ans = self.send_command("SET MONITOR "+ str(self.monitor_save.text().toInt()[0]) +" "+ str(self.monitor_count.text().toInt()[0]) +" "+ str(self.monitor_name.text()))
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
            self.error("Sending wrong command '"+command+"'")
            return None
        
        # Send command
        try:
            self.sock.sendall(command + "\n")
            if not nolog:
                self.command_log.appendPlainText("S: "+command)
            #self.emit(QtCore.SIGNAL("msg(QString)"),QtCore.QString("S: "+command))
        except socket.error as msg:
            self.error(str(msg), "Network error")
            return None
            
        # Receive answer
        ans = self.get_command()
        if ans:
            if not nolog:
                self.command_log.appendPlainText("R: "+" ".join(ans))
            #self.emit(QtCore.SIGNAL("msg(QString)"),QtCore.QString("R: "+" ".join(ans)))
            # Interpret answer
            if command[:3] == 'SET':
                if ans[0] == 'ACK':
                    # Command was successful
                    return (1, )
                elif ans[0] == 'ERR':
                    # An error occurred
                    self.error("The command '"+command+"' returned error '"+ans[1]+"'")
                else:
                    self.error("The command '"+command+"' returned an unexpected response '"+" ".join(ans)+"'")
                    
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
                                        ag_lst.append( (int(m.groupdict()['id']), m.groupdict()['mac']) )
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
                    p = re.compile(r""+ans[1][:id]+r" (?P<time>\d+\.?\d*[umsMh]{1})")
                    m = p.match(ans[1])
                    if m:
                        return (m.groupdict()['time'], )
                    else:
                        self.error("Got a malformed timestring '"+ans[id+1:]+"'.")
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
                        self.error("Got a malformed prefix '"+ans[id+1:]+"'.")
                elif ans[1][:id] == 'FORMAT':
                    pass
                elif ans[1][:id] == 'AUTOSAVE':
                    p = re.compile(r"AUTOSAVE (?P<count>\d+)")
                    m = p.match(ans[1])
                    if m:
                        return (m.groupdict()['count'], )
                    else:
                        self.error("Got a malformed autosave count '"+ans[id+1:]+"'.")
                elif ans[1][:id] == 'MONITOR':
                    p = re.compile(r"MONITOR (?P<save>\d+) (?P<count>\d+)\s*(?P<file>[a-zA-Z0-9\.\_\-\/]*)")
                    m = p.match(ans[1])
                    if m:
                        return (int(m.groupdict()['save']), int(m.groupdict()['count']), m.groupdict()['file'])
                    else:
                        self.error("Got a malformed monitor configuration '"+ans[id+1:]+"'.")
                else:
                    pass

            elif command[:3] == 'MSR':
                if ans[0] == 'ACK':
                    # Command was successful
                    return (1, )
                elif ans[0] == 'ERR':
                    # An error occurred
                    self.error("The command '"+command+"' returned error '"+ans[1]+"'")
                elif ans[0] == 'MSR':
                    id = ans[1].find(" ")
                    # Status and list
                    if ans[1][:id] == 'STATUS':
                        return (ans[1][id+1:], )
                    else:
                        pass
                else:
                    self.error("The command '"+command+"' returned an unexpected response '"+" ".join(ans)+"'")
                
            elif command[:3] == 'EXT':
                pass
        return None
        
    def get_command(self):
        """ Read a command from the receive buffer """
        while True:
            id = self.buffer.find("\r\n")
            if id > 0:
                cmd = self.buffer[:id]
                self.buffer = self.buffer[(id+2):]
                for c in self.recv_cmd:
                    if c == cmd[:3]:
                        return (cmd[:3],  cmd[4:])
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
    
    @pyqtSignature("")
    def on_config_button_released(self):
        """
        Slot documentation goes here.
        """
        self.write_config()
    
    @pyqtSignature("")
    def on_connect_button_released(self):
        """ Start connection to remote server
        """
        try:
            self.sock = socket.create_connection( (str(self.server_address.text()), self.server_port.text().toInt()[0]) )
            self.connect_button.setEnabled(False)
            self.disconnect_button.setEnabled(True)
            self.polltimer = threading.Timer(2.0, self.get_status)
            self.polltimer.start()
            self.read_config()
        except socket.error as msg:
            self.error(str(msg), "Network error")
    
    @pyqtSignature("")
    def on_disconnect_button_released(self):
        """ Close connection to remote server
        """
        self.polltimer.cancel()
        self.sock.close()
        self.connect_button.setEnabled(True)
        self.disconnect_button.setEnabled(False)
        self.sig_status.emit(QtCore.QString("Unknown"))
    
    @pyqtSignature("")
    def on_start_button_released(self):
        """
        Slot documentation goes here.
        """
        self.send_command("MSR START")
    
    @pyqtSignature("")
    def on_stop_button_released(self):
        """
        Slot documentation goes here.
        """
        self.send_command("MSR STOP")
