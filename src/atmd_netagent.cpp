/*
 * ATMD Server version 3.0
 *
 * RTnet communication protocol class
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

#ifdef DEBUG
extern bool enable_debug;
#endif

#include "atmd_netagent.h"


/* @fn template<typename T> size_t serialize(char * buffer, size_t offset, T var)
 * Template function to serialize type T to a buffer.
 */
template<typename T> size_t AgentMsg::serialize(char * buffer, size_t offset, T var) {
  if(offset+sizeof(T) > maxsize())
    throw(0);
  T* value = reinterpret_cast<T*>( buffer+offset );
  *value = var;
  return (offset+sizeof(T));
}


/* @fn size_t serialize(char * buffer, size_t offset, const char* var)
 * Specialization of serialize template for char arrays, aka strings.
 */
size_t AgentMsg::serialize(char * buffer, size_t offset, const char* var) {
  if(offset+strlen(var)+1 > maxsize())
    throw(0);
  strncpy(buffer+offset, var, maxsize()-offset);
  return (offset+strlen(var)+1);
}


/* @fn template<typename T> size_t deserialize(char * buffer, size_t offset, T var)
 * Template function to deserialize type T to a buffer.
 */
template<typename T> size_t AgentMsg::deserialize(char * buffer, size_t offset, T& var) {
  T* value = reinterpret_cast<T*>( buffer+offset );
  var = *value;
  return (offset+sizeof(T));
}


/* @fn size_t deserialize(char * buffer, size_t offset, const char* var)
 * Specialization of deserialize template for char arrays, aka strings.
 */
size_t AgentMsg::deserialize(char * buffer, size_t offset, char* var, size_t var_len) {
  strncpy(var, buffer+offset, var_len-1);
  var[var_len-1] = '\0';
  return (offset+strlen(buffer+offset)+1);
}


/* @fn int AgentMsg::decode()
 * Decode a control message
 */
int AgentMsg::decode() {

  // Service variables
  size_t offset = 0;

  // First 4 bytes of message are type and size, both uint16_t
  offset = deserialize<uint16_t>(_buffer, offset, _type);
  offset = deserialize<uint16_t>(_buffer, offset, _size);

  switch(_type) {
    case ATMD_CMD_BADTYPE:
      rt_syslog(ATMD_ERR, "NetAgent [AgentMsg::decode]: message type is ATMD_CMD_BADTYPE.");
      return -1;

    case ATMD_CMD_BRD:
    case ATMD_CMD_HELLO:
      // Here we expect a string
      offset = deserialize(_buffer, offset, _version, ATMD_VER_LEN);
      break;

    case ATMD_CMD_MEAS_SET:
      // 0) agent_id -> UINT32
      offset = deserialize<uint32_t>(_buffer, offset, _agent_id);

      // 1) start_rising -> UINT8
      offset = deserialize<uint8_t>(_buffer, offset, _start_rising);

      // 2) start_falling -> UINT8
      offset = deserialize<uint8_t>(_buffer, offset, _start_falling);

      // 3) rising_mask -> UINT8
      offset = deserialize<uint8_t>(_buffer, offset, _rising_mask);

      // 4) falling_mask -> UINT8
      offset = deserialize<uint8_t>(_buffer, offset, _falling_mask);

      // 5) measure_time -> UINT64
      offset = deserialize<uint64_t>(_buffer, offset, _measure_time);

      // 6) window_time -> UINT64
      offset = deserialize<uint64_t>(_buffer, offset, _window_time);

      // 7) timeout -> UINT64
      offset = deserialize<uint64_t>(_buffer, offset, _timeout);

      // 8) deadtime -> UINT64
      offset = deserialize<uint64_t>(_buffer, offset, _deadtime);

      // 9) start_offset -> UINT32
      offset = deserialize<uint32_t>(_buffer, offset, _start_offset);

      // 10) refclk -> UINT16
      offset = deserialize<uint16_t>(_buffer, offset, _refclk);

      // 11) hsdiv -> UINT16
      offset = deserialize<uint16_t>(_buffer, offset, _hsdiv);

      break;

    case ATMD_CMD_MEAS_CTR:
      // Here we expect a uint16_t value plus a uint32_t value
      offset = deserialize<uint16_t>(_buffer, offset, _action);
      offset = deserialize<uint32_t>(_buffer, offset, _tdma_cycle);
      break;

    case ATMD_CMD_ACK:
    case ATMD_CMD_BUSY:
    case ATMD_CMD_ERROR:
      // No arguments
      break;

    default:
      // Unknown type
      rt_syslog(ATMD_ERR, "NetAgent [AgentMsg::decode]: trying to decode an unknown message type.");
      return -1;
  }

  return 0;
}

/* @fn int AgentMsg::encode()
 * Encode a control message
 */
int AgentMsg::encode() {

  size_t offset = 0;

  // Clear buffer
  memset(_buffer, 0, ATMD_PACKET_SIZE);

  // Set type
  offset = serialize<uint16_t>(_buffer, offset, _type);
  offset = serialize<uint16_t>(_buffer, offset, 0);

  switch(_type) {
    case ATMD_CMD_BADTYPE:
      rt_syslog(ATMD_ERR, "NetAgent [AgentMsg::encode]: message type is ATMD_CMD_BADTYPE.");
      return -1;

    case ATMD_CMD_BRD:
    case ATMD_CMD_HELLO:
      offset = serialize(_buffer, offset, _version);
      break;

    case ATMD_CMD_MEAS_SET:
      // 0) agent_id -> UINT32
      offset = serialize<uint32_t>(_buffer, offset, _agent_id);

      // 1) start_rising -> UINT8
      offset = serialize<uint8_t>(_buffer, offset, _start_rising);

      // 2) start_falling -> UINT8
      offset = serialize<uint8_t>(_buffer, offset, _start_falling);

      // 3) rising_mask -> UINT8
      offset = serialize<uint8_t>(_buffer, offset, _rising_mask);

      // 4) falling_mask -> UINT8
      offset = serialize<uint8_t>(_buffer, offset, _falling_mask);

      // 5) measure_time -> UINT64
      offset = serialize<uint64_t>(_buffer, offset, _measure_time);

      // 6) window_time -> UINT64
      offset = serialize<uint64_t>(_buffer, offset, _window_time);

      // 7) timeout -> UINT64
      offset = serialize<uint64_t>(_buffer, offset, _timeout);

      // 8) deadtime -> UINT64
      offset = serialize<uint64_t>(_buffer, offset, _deadtime);

      // 9) start_offset -> UINT32
      offset = serialize<uint32_t>(_buffer, offset, _start_offset);

      // 10) refclk -> UINT16
      offset = serialize<uint16_t>(_buffer, offset, _refclk);

      // 11) hsdiv -> UINT16
      offset = serialize<uint16_t>(_buffer, offset, _hsdiv);

      break;

    case ATMD_CMD_MEAS_CTR:
      offset = serialize<uint16_t>(_buffer, offset, _action);
      offset = serialize<uint32_t>(_buffer, offset, _tdma_cycle);
      break;

    case ATMD_CMD_ACK:
    case ATMD_CMD_BUSY:
    case ATMD_CMD_ERROR:
      break;

    default:
      // Unknown type
      rt_syslog(ATMD_ERR, "NetAgent [AgentMsg::encode]: trying to encode an unknown message type.");
      return -1;
  }

  // Set size
  _size = offset;
  offset = sizeof(uint16_t);
  offset = serialize<uint16_t>(_buffer, offset, _size);

  return 0;
}


/* @fn int DataMsg::encode()
 * Encode a data message
 */
int DataMsg::encode(size_t start, const xenovec<int8_t>& ch,
                                  const xenovec<int32_t>& stoptime,
                                  const xenovec<uint32_t>& retrig) {

  // Clean buffer
  memset(_buffer, 0, ATMD_PACKET_SIZE);
  size_t offset = 2 * sizeof(uint16_t); // Leave type and number of events as last job

  // Set start ID
  *( reinterpret_cast<uint32_t*>(_buffer+offset) ) = _id;
  offset += sizeof(uint32_t);

  // Check if this is the first packet
  if(start == 0) {
    _type = ATMD_DT_FIRST;

    // Total number of events
    *( reinterpret_cast<uint32_t*>(_buffer+offset) ) = ch.size();
    offset += sizeof(uint32_t);

    // Window start time
    *( reinterpret_cast<uint64_t*>(_buffer+offset) ) = _window_start;
    offset += sizeof(uint64_t);

    // Window duration
    *( reinterpret_cast<uint64_t*>(_buffer+offset) ) = _window_time;
    offset += sizeof(uint64_t);

  } else {
    _type = ATMD_DT_DATA;
  }

  // Add events
  uint16_t count = 0;
  while(true) {

    // Add one event
    *( reinterpret_cast<int8_t*>(_buffer+offset) ) = ch[start];
    offset += sizeof(int8_t);
    *( reinterpret_cast<int32_t*>(_buffer+offset) ) = stoptime[start];
    offset += sizeof(int32_t);
    *( reinterpret_cast<uint32_t*>(_buffer+offset) ) = retrig[start];
    offset += sizeof(uint32_t);

    // Update counters
    start++;
    count++;

    if(start >= ch.size()) {
      if(_type == ATMD_DT_FIRST)
        _type = ATMD_DT_ONLY;
      else
        _type = ATMD_DT_LAST;
      break;
    } else {
      if(offset >= ATMD_PACKET_SIZE)
        break;
    }
  }

  // Set packet size
  _size = offset;

  // Write type and number of events
  *( reinterpret_cast<uint16_t*>(_buffer) ) = _type;
  *( reinterpret_cast<uint16_t*>(_buffer+sizeof(uint16_t)) ) = count;

  return start;
}


/* @fn int DataMsg::encode()
 *
 */
int DataMsg::encode() {
  // This function only support ATMD_DT_TERM type
  if(_type == ATMD_DT_TERM) {

    // Clear buffer
    memset(_buffer, 0, ATMD_PACKET_SIZE);

    size_t offset = 0;

    // Set type
    *( reinterpret_cast<uint16_t*>(_buffer+offset) ) = _type;
    offset += sizeof(uint16_t) * 2; // Skip numev

    // Measure start time
    *( reinterpret_cast<uint64_t*>(_buffer+offset) ) = _window_start;
    offset += sizeof(uint64_t);

    // Measure duration
    *( reinterpret_cast<uint64_t*>(_buffer+offset) ) = _window_time;
    offset += sizeof(uint64_t);

    _size = offset;
    return 0;

  } else {
    return -1;
  }
}


/* @fn int DataMsg::decode()
 * Decode a data message
 */
int DataMsg::decode() {

  size_t offset = 0;

  // Get packet type
  _type = *( reinterpret_cast<uint16_t*>(_buffer) );
  offset += sizeof(uint16_t);

  // ATMD_DT_TERM needs different handling
  if(_type == ATMD_DT_TERM) {
    // Measure start time
    _window_start = *( reinterpret_cast<uint64_t*>(_buffer+offset) );
    offset += sizeof(uint64_t);

    // Measure duration
    _window_time = *( reinterpret_cast<uint64_t*>(_buffer+offset) );
    offset += sizeof(uint64_t);

    return 0;
  }

  // Read number of events
  _numev = *( reinterpret_cast<uint16_t*>(_buffer) );
  offset += sizeof(uint16_t);

  // Read ID
  _type = *( reinterpret_cast<uint32_t*>(_buffer) );
  offset += sizeof(uint32_t);

  switch(_type) {
    case ATMD_DT_FIRST:
    case ATMD_DT_ONLY:
      // Total events
      _total_events = *( reinterpret_cast<uint32_t*>(_buffer+offset) );
      offset += sizeof(uint32_t);

      // Window start time
      _window_start = *( reinterpret_cast<uint64_t*>(_buffer+offset) );
      offset += sizeof(uint64_t);

      // Window duration
      _window_time = *( reinterpret_cast<uint64_t*>(_buffer+offset) );
      offset += sizeof(uint64_t);

    case ATMD_DT_DATA:
    case ATMD_DT_LAST:
      // Set event offset
      _ev_offset = offset;
      break;

    default:
      rt_syslog(ATMD_ERR, "NetAgent [DataMsg::decode]: trying to decode an unknown message type.");
      return -1;
  }

  return 0;
}


/* @fn int DataMsg::decode()
 * Decode a data message
 */
int DataMsg::getevent(size_t i, int8_t &ch, int32_t &stoptime, uint32_t &retrig)const {
  // Check if we reached the last event
  if(i >= _numev)
    return -1;

  // Check buffer limits
  if(_ev_offset + ATMD_EV_SIZE * (i+1) > ATMD_PACKET_SIZE)
    return -1;

  // Extract event
  ch = *( reinterpret_cast<const int8_t*>(_buffer + _ev_offset + ATMD_EV_SIZE * i) );
  stoptime = *( reinterpret_cast<const int32_t*>(_buffer + _ev_offset + ATMD_EV_SIZE * i + sizeof(int8_t)) );
  retrig = *( reinterpret_cast<const uint32_t*>(_buffer + _ev_offset + ATMD_EV_SIZE * i + sizeof(int8_t) + sizeof(uint32_t)) );

  return 0;
}
