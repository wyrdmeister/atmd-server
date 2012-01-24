/*
 * ATMD Server version 3.0
 *
 * ATMD Server - Configuration options header
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

#ifndef ATMD_CONFIG
#define ATMD_CONFIG

// Global
#include <stdio.h>
#include <string>
#include <vector>
#include <stdint.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <netinet/ether.h>

#include <pcrecpp.h>

// Local
#include "common.h"


/* @class AtmdConfig
 * This class reads the ATMD configuration from a text files and stores it.
 */
class AtmdConfig {
public:
  AtmdConfig() {};
  ~AtmdConfig() {};

  // Open the give file an read configuration from it
  int read(const std::string& filename);

  // Get an agent address
  const struct ether_addr* get_agent(size_t id)const { return &(agent_addr[id]); };

  // Get the number of agent addresses
  size_t agents()const { return agent_addr.size(); };

  // Return a pointer to RTSKBS
  unsigned int rtskbs()const { return _rtskbs; };

  // Return the interface name
  const char * rtif()const { return _rtif; };

  // Clear config
  void clear() {
    agent_addr.clear();
  };

private:
  // Agent addresses
  std::vector<struct ether_addr> agent_addr;

  // RTSKBS
  unsigned int _rtskbs;

  // RT ethernet interface
  char _rtif[IFNAMSIZ];

};

#endif
