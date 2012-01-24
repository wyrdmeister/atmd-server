/*
 * ATMD Server version 3.0
 *
 * ATMD Server - Client communication class header
 *
 * Copyright (C) Michele Devetta 2012 <michele.devetta@unimi.it>
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ATMD_NETWORK_H
#define ATMD_NETWORK_H

// Network constants
#define ATMD_DEF_PORT       2606
#define ATMD_DEF_LISTEN     "0.0.0.0"

// Global
#include <vector>
#include <string>
#include <stdarg.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <pcrecpp.h>


// Local
#include "common.h"
#include "atmd_virtualboard.h"


/* @class Network
 * Class to manage client connections
 */
class Network {
  public:
  Network();
  ~Network() {};

  // Network initialization
  int init();
  int accept_client();
  void close_client() { if(this->client_socket) close(this->client_socket); this->client_socket = 0; };

  // Command managing
  int get_command(string& command);
  int exec_command(string command, VirtualBoard& board);
  int send_command(string command);
  std::string format_command(std::string format, ...);

  // Interfaces for connection parameters
  void set_address(std::string address) { this->address = address; };
  string get_address() { return this->address; };
  void set_port(uint16_t port) { this->port = port; };
  uint16_t get_port() { return this->port; };

  private:
  // Connection parameters
  std::string address;
  uint16_t port;

  // Connection sockets
  int listen_socket;
  int client_socket;

  // Valid command beginnings
  std::vector<std::string> valid_commands;

  // Receive buffer
  std::string recv_buffer;

  // Utility function to fill the buffer
  int fill_buffer();

  // Utility function that check the buffer to find a valid command
  int check_buffer(std::string& command);
};

#endif
