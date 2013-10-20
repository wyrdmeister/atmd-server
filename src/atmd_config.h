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
#include <pwd.h>
#include <grp.h>

#include <pcrecpp.h>

// Local
#include "common.h"


/* @class AtmdConfig
 * This class reads the ATMD configuration from a text files and stores it.
 */
class AtmdConfig {
public:
  AtmdConfig() : _rtskbs(0) {
#ifdef EN_TANG0
    _tango_ch = 0;
#endif
#ifdef ATMD_SERVER
    _uid = 0;
    _gid = 0;
#endif
    memset(_rtif, 0, IFNAMSIZ);
    memset(_tdma_dev, 0, IFNAMSIZ);
  };
  ~AtmdConfig() {};

  // Open the give file an read configuration from it
  int read(const std::string& filename);

#ifdef ATMD_SERVER
  // Get an agent address
  const struct ether_addr* get_agent(size_t id)const { return &(agent_addr[id]); };

  // Get the number of agent addresses
  size_t agents()const { return agent_addr.size(); };

  // UID to save files
  uid_t uid()const { return _uid; }
  void uid(uid_t num) { _uid = num; }

  // GID to save files
  gid_t gid()const { return _gid; }
  void gid(gid_t num) { _gid = num; }
#endif
  
  // Return a pointer to RTSKBS
  unsigned int rtskbs()const { return _rtskbs; };

  // Return the interface name
  const char * rtif()const { return _rtif; };

  // Return the TDMA device name
  const char * tdma_dev()const { return _tdma_dev; };

#ifdef EN_TANGO
  // Return TANGO trigger channel
  int8_t tango_ch()const { return _tango_ch; };
#endif


  // Clear config
  void clear() {
#ifdef ATMD_SERVER
    agent_addr.clear();
#endif
  };


private:
#ifdef ATMD_SERVER
  // Agent addresses
  std::vector<struct ether_addr> agent_addr;

  // UID
  uid_t _uid;

  // GID
  gid_t _gid;
#endif
  
  // RTSKBS
  unsigned int _rtskbs;

  // RT ethernet interface
  char _rtif[IFNAMSIZ];

  // TDMA device
  char _tdma_dev[IFNAMSIZ];

#ifdef EN_TANGO
  // TANGO trigger channel
  unsigned int _tango_ch;
#endif

};

#endif
