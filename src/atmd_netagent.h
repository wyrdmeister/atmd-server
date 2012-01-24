/*
 * ATMD Server version 3.0
 *
 * RTnet communication protocol class header
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

#ifndef ATMD_AGENT_NETWORK_H
#define ATMD_AGENT_NETWORK_H

// Message sizes
#define ATMD_PACKET_SIZE    1500
#define ATMD_EV_SIZE        ( sizeof(int8_t) + sizeof(uint32_t) * 2 )

// Message types
#define ATMD_CMD_BADTYPE    0   // Bad type. Returned on unknown command
#define ATMD_CMD_BRD        1   // Broadcast message
#define ATMD_CMD_HELLO      2   // Hello message (answer to broadcast)
#define ATMD_CMD_PROTO      3   // Set data protocol ID
#define ATMD_CMD_MEAS_SET   4   // Configure measure parameters
#define ATMD_CMD_MEAS_CTR   5   // Control measure
#define ATMD_CMD_ACK        6   // Acknowledge
#define ATMD_DT_FIRST       7   // First data packet
#define ATMD_DT_ONLY        8   // Single data packet for this start
#define ATMD_DT_DATA        9   // General data packet
#define ATMD_DT_LAST       10   // Final data packet
#define ATMD_DT_TERM       11   // Measure termination packet

// Actions
#define ATMD_ACTION_NOACTION 0
#define ATMD_ACTION_START    1
#define ATMD_ACTION_STOP     2

// Type for message passing
#define ATMD_TYPE_UINT8     1
#define ATMD_TYPE_INT8      2
#define ATMD_TYPE_UINT16    3
#define ATMD_TYPE_INT16     4
#define ATMD_TYPE_UINT32    5
#define ATMD_TYPE_INT32     6
#define ATMD_TYPE_UINT64    7
#define ATMD_TYPE_INT64     8
#define ATMD_TYPE_STR       9

// Max len of version string
#define ATMD_VER_LEN        32


// Global
#include <stdint.h>
#include <string.h>

// Xenomai
#include <rtdk.h>

// Local
#include "common.h"
#include "xenovec.h"


/* @class GenMsg
 * General base class for messages
 */
class GenMsg {
public:
  GenMsg() : _type(0) {};
  ~GenMsg() {};

  // Handle type
  void type(uint16_t val) { _type = val; };
  uint16_t type()const { return _type; };

  // Handle buffer
  const uint8_t* get_buffer()const { return _buffer; };
  uint8_t* get_buffer() { return _buffer; };
  size_t size()const { return _size; };
  size_t maxsize()const { return ATMD_PACKET_SIZE; };

  // Clear method
  void clear() {
    _type = ATMD_CMD_BADTYPE;
    _size = 0;
    memset(_buffer, 0, ATMD_PACKET_SIZE);
  }

protected:
  // Type and size
  uint16_t _type;
  uint16_t _size;

  // Buffer
  uint8_t _buffer[ATMD_PACKET_SIZE];
};


/* @class AgentMsg
 * This class describes a control message sent between master and agent.
 */
class AgentMsg : public GenMsg {
public:
  AgentMsg() { clear(); };
  ~AgentMsg() {};

  // Parse a message buffer, deserializing the data
  int decode();

  // Encode the message into the give buffer, serializing the data
  int encode();

  // Manage version
  void version(const char * val) { strncpy(_version, val, ATMD_VER_LEN); _version[ATMD_VER_LEN-1] = '\0'; };
  const char * version()const { return _version; };

  // Manage protocol ID
  void protocol(uint16_t val) { _proto_id = val; };
  uint16_t protocol()const { return _proto_id; };

  // Manage measure info
  void start_rising(uint8_t val) { _start_rising = val; };
  uint8_t start_rising()const { return _start_rising; };
  void start_falling(uint8_t val) { _start_falling = val; };
  uint8_t start_falling()const { return _start_falling; };
  void rising_mask(uint8_t val) { _rising_mask = val; };
  uint8_t rising_mask()const { return _rising_mask; };
  void falling_mask(uint8_t val) { _falling_mask = val; };
  uint8_t falling_mask()const { return _falling_mask; };
  void measure_time(uint64_t val) { _measure_time = val; };
  uint64_t measure_time()const { return _measure_time; };
  void window_time(uint64_t val) { _window_time = val; };
  uint64_t window_time()const { return _window_time; };
  void timeout(uint64_t val) { _timeout = val; };
  uint64_t timeout()const { return _timeout; };
  void deadtime(uint64_t val) { _deadtime = val; };
  uint64_t deadtime()const { return _deadtime; };

  // Manage measure action
  void action(uint16_t val) { _action = val; };
  uint16_t action()const { return _action; };

  // TDMA cycle for synchronization
  void tdma_cycle(uint32_t val) { _tdma_cycle = val; };
  uint32_t tdma_cycle()const { return _tdma_cycle; };

  // Clear operator
  void clear() {
    GenMsg::clear();
    memset(_version, 0, ATMD_VER_LEN);
    _proto_id = 0;
    _start_rising = 0;
    _start_falling = 0;
    _rising_mask = 0;
    _falling_mask = 0;
    _measure_time = 0;
    _window_time = 0;
    _timeout = 0;
    _deadtime = 0;
    _action = ATMD_ACTION_NOACTION;
    _tdma_cycle = 0;
  };

private:
  // Version string for broadcast / hello messages
  char _version[ATMD_VER_LEN];

  // Protocol ID
  uint16_t _proto_id;

  // Measure info
  uint8_t _start_rising;
  uint8_t _start_falling;
  uint8_t _rising_mask;
  uint8_t _falling_mask;
  uint64_t _measure_time;
  uint64_t _window_time;
  uint64_t _timeout;
  uint64_t _deadtime;

  // Measure action
  uint16_t _action;

  // TDMA cycle
  uint32_t _tdma_cycle;
};


/* @class DataMsg
 * This class descibes a data message sent from the agent to the master.
 */
class DataMsg : public GenMsg {
public:
  DataMsg() { clear(); };
  ~DataMsg() {};

  // Manage start ID
  void id(uint32_t val) { _id = val; };
  uint32_t id()const { return _id; };

  // Encode data packet
  int encode(size_t start, const xenovec<int8_t>& ch,
                           const xenovec<int32_t>& stoptime,
                           const xenovec<uint32_t>& retrig);
  int encode();

  // Decode data packet
  int decode();

  // Get number of events decoded
  size_t numev()const { return _numev; };

  // Get event
  int getevent(size_t i, int8_t &ch, int32_t &stoptime, uint32_t &retrig)const;

  // Manage window_start
  void window_start(uint64_t val) { _window_start = val; };
  uint64_t window_start()const { return _window_start; };

  // Manage window_time
  void window_time(uint64_t val) { _window_time = val; };
  uint64_t window_time()const { return _window_time; };

  // Clear
  void clear() {
    GenMsg::clear();
    _id = 0;
    _window_start = 0;
    _window_time = 0;
    _numev = 0;
    _total_events = 0;
    _evcount = 0;
    _ev_offset = 0;
  };

private:
  // Start ID
  uint32_t _id;

  // Window times
  uint64_t _window_start;
  uint64_t _window_time;

  // Number of events added
  uint32_t _numev;
  uint32_t _total_events;
  size_t _evcount;

  // Buffer offset
  size_t _ev_offset;
};

#endif
