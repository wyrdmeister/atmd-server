/*
 * ATMD Server version 3.0
 *
 * ATMD Server - Virtual board
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

// Global debug flag
#ifdef DEBUG
extern bool enable_debug;
#endif

#include "atmd_rtnet.h"


/* @fn int RTnet::init(bool en_bind = false)
 *
 */
int RTnet::init(bool en_bind) {

  int retval = 0;

  // Check that the interface name is set
  if(en_bind && strlen(_ifname) == 0) {
    rt_syslog(ATMD_ERR, "RTnet [init]: cannot bind without specifing the interface name.");
    return -1;
  }

  // Check that the protocol type is specified
  if(_protocol == ATMD_PROTO_NONE) {
    rt_syslog(ATMD_ERR, "RTnet [init]: cannot proceed without specifing the protocol ID.");
    return -1;
  }

  // Create real-time network control socket
  _sock = rt_dev_socket(AF_PACKET, SOCK_DGRAM, htons(_protocol));
  if (_sock < 0) {
    rt_syslog(ATMD_ERR, "RTnet [init]: cannot create RT socket. Error: '%s'.\n", strerror(-_sock));
    return -1;
  }

#ifdef DEBUG
  if(enable_debug)
    rt_syslog(ATMD_DEBUG, "RTnet [init]: successfully created RT socket with descriptor '%d'.\n", _sock);
#endif

  // Extend the socket pool
  retval = rt_dev_ioctl(_sock, RTNET_RTIOC_EXTPOOL, &_rtskbs);
  if (retval != (int)_rtskbs) {
    rt_syslog(ATMD_ERR, "RTnet [init]: rt_dev_ioctl(RT_IOC_SO_EXTPOOL) failed. Error: '%s'\n", strerror(-retval));
    rt_dev_close(_sock);
    _sock = -1;
    return -1;
  }

#ifdef DEBUG
  if(enable_debug)
    rt_syslog(ATMD_DEBUG, "RTnet [init]: successfully extended RT socket pool to %d SKBS.\n", _rtskbs);
#endif

  // If bind is enabled...
  if(en_bind) {

    // Find interface ID
    struct ifreq ifr;
    strncpy(ifr.ifr_name, _ifname, IFNAMSIZ);
    retval = rt_dev_ioctl(_sock, SIOCGIFINDEX, &ifr);
    if(retval < 0) {
      rt_syslog(ATMD_ERR, "RTnet [init]: cannot identify interface '%s'. Error: '%s'.", _ifname, strerror(-retval));
      rt_dev_close(_sock);
      _sock = -1;
      return -1;
    }

#ifdef DEBUG
  if(enable_debug)
    rt_syslog(ATMD_DEBUG, "RTnet [init]: interface '%s' was found to have ID %d.\n", _ifname, ifr.ifr_ifindex);
#endif

    // Local address
    struct sockaddr_ll local_addr;
    memset(&local_addr, 0, sizeof(struct sockaddr_ll));
    local_addr.sll_family = AF_PACKET;
    local_addr.sll_protocol = htons(_protocol);
    local_addr.sll_ifindex = ifr.ifr_ifindex;

    // Bind socket
    retval = rt_dev_bind(_sock, (struct sockaddr *)&local_addr, sizeof(struct sockaddr_ll));
    if(retval < 0) {
      rt_syslog(ATMD_ERR, "RTnet [init]: cannot bind socket to local interface '%s'. Error: '%s'.", _ifname, strerror(-retval));
      rt_dev_close(_sock);
      _sock = -1;
      return -1;
    }

#ifdef DEBUG
    if(enable_debug)
      rt_syslog(ATMD_DEBUG, "RTnet [init]: successfully binded RT socket '%d' to interface '%s' with protocol '0x%04X'.\n", _sock, _ifname, _protocol);
#endif
  }

  return 0;
}


/* @fn int RTnet::send(const GenMsg& packet, struct ether_addr* address)
 *
 */
int RTnet::send(const GenMsg& packet, const struct ether_addr* addr)const {
  // Check address
  if(!addr) {
    rt_syslog(ATMD_ERR, "RTnet [send]: passed a null address.");
    return -1;
  }

  // Compose address
  struct sockaddr_ll remote_addr;
  memset(&remote_addr, 0, sizeof(struct sockaddr_ll));
  remote_addr.sll_family = AF_PACKET;
  remote_addr.sll_protocol = _protocol;
  memcpy(&remote_addr.sll_addr, addr, sizeof(struct ether_addr));

  // Send message
  int retval = rt_dev_sendto(_sock, packet.get_buffer(), packet.size(), 0, (struct sockaddr*)&remote_addr, sizeof(struct sockaddr_ll));
  if(retval < 0) {
    rt_syslog(ATMD_ERR, "RTnet [send]: failed to send packet. Error: '%s'.", strerror(-retval));
    return -1;
  }

  return 0;
}


/* @fn int RTnet::recv(GenMsg& packet, struct ether_addr* remote_addr = NULL)
 *
 */
int RTnet::recv(GenMsg& packet, struct ether_addr* addr)const {
  // Clear packet
  packet.clear();

  // Init remote addr structure
  struct sockaddr_ll remote_addr;
  socklen_t remote_len = sizeof(struct sockaddr_ll);
  memset(&remote_addr, 0, sizeof(struct sockaddr_ll));

  // Receive packet
  int retval = rt_dev_recvfrom(_sock, packet.get_buffer(), packet.maxsize(), 0, (struct sockaddr*)&remote_addr, &remote_len);
  if(retval < 0) {
    rt_syslog(ATMD_ERR, "RTnet [recv]: failed to receive packet. Error: '%s'.", strerror(-retval));
    return -1;
  }

  // Copy remote addr
  if(addr) {
    memcpy(&addr, &remote_addr.sll_addr, sizeof(struct ether_addr));
  }

  return 0;
}


/* @fn unsigned long RTnet::wait_tdma(unsigned long cycle)
 * Wait for a specific TDMA cycle
 */
unsigned long RTnet::wait_tdma(unsigned long cycle) {
  unsigned long curr = 0;
  while(true) {
    curr = wait_tdma();
    if(curr == 0)
      // Error in TDMA
      break;
    if(curr < cycle)
      continue;
    else if(curr == cycle)
      break;
    else {
      rt_syslog(ATMD_ERR, "RTnet [recv]: tried to sync on a TDMA cycle in the past.");
      break;
    }
  }
  return curr;
}


/* @fn unsigned long RTnet::wait_tdma()
 * Wait for next TDMA cycle
 */
unsigned long RTnet::wait_tdma() {
  struct rtmac_waitinfo waitinfo;
  waitinfo.type = TDMA_WAIT_ON_SYNC;
  waitinfo.size = sizeof(waitinfo);

  int retval = rt_dev_ioctl(_sock, RTMAC_RTIOC_WAITONCYCLE_EX, &waitinfo);
  if(retval) {
    rt_syslog(ATMD_ERR, "RTnet [wait_tdma]: error in IOCTL waiting for TMDA sync. Error: %s.", strerror(-retval));
    return 0;

  } else {
    return waitinfo.cycle_no;
  }
}
