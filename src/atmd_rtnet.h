/*
 * ATMD Server version 3.0
 *
 * ATMD Server - RTnet class header
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

#ifndef ATMD_RTNET_H
#define ATMD_RTNET_H

// Default number of RTSKBS
#define ATMD_DEF_RTSKBS     250

// Protocols
#define ATMD_PROTO_NONE     0x0000
#define ATMD_PROTO_CTRL     0x5555
#define ATMD_PROTO_DATA     0x5115

// Global
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netinet/ether.h>
#include <netpacket/packet.h>

// Xenomai
#include <rtdk.h>
#include <rtnet.h>
#include <rtmac.h>

// Local
#include "common.h"
#include "atmd_netagent.h"


/* @class RTnet
 * Define a wrapper around an RTnet socket
 */
class RTnet {
public:
  RTnet() : _sock(-1), _if_id(0), _rtskbs(ATMD_DEF_RTSKBS), _protocol(ATMD_PROTO_NONE) { memset(_ifname, 0, IFNAMSIZ); };
  ~RTnet() { if(_sock >= 0) rt_dev_close(_sock); };

  // Configure RTSKBS
  void rtskbs(unsigned int num) { _rtskbs = num; };
  unsigned int rtskbs()const { return _rtskbs; };

  // Configure interface
  void interface(const char* name) { strncpy(_ifname, name, IFNAMSIZ); _ifname[IFNAMSIZ-1] = '\0'; };
  const char* interface()const { return _ifname; };

  // Configure protocol
  void protocol(uint16_t proto) { _protocol = proto; };
  uint16_t protocol()const { return _protocol; };

  // Init socket
  int init(bool en_bind = false);

  // Send packet
  int send(const GenMsg& packet, const struct ether_addr* addr)const;

  // Receive packet
  int recv(GenMsg& packet, struct ether_addr* addr = NULL)const;

  // Close socket
  void close() { if(_sock >= 0) rt_dev_close(_sock); };

  // Wait for a TDMA cycle
  unsigned long wait_tdma(unsigned long cycle);

  // Wait on TDMA sync
  unsigned long wait_tdma();

private:
  // Socket
  int _sock;

  // Interface ID
  int _if_id;

  // Number of SKBS
  unsigned int _rtskbs;

  // Interface name for bind
  char _ifname[IFNAMSIZ];

  // Protocol for bind
  uint16_t _protocol;
};

#endif
