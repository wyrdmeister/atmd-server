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


/* @fn int AgentMsg::decode()
 * Decode a control message
 */
int AgentMsg::decode() {

  // Service variables
  size_t offset = 0;
  uint8_t val_type = 0;

  // First 4 bytes of message are type and size, both uint16_t
  _type = *( reinterpret_cast<uint16_t*>(_buffer) );
  offset += sizeof(uint16_t);
  _size = *( reinterpret_cast<uint16_t*>(_buffer + offset) );
  offset += sizeof(uint16_t);

  switch(_type) {
    case ATMD_CMD_BADTYPE:
      rt_syslog(ATMD_ERR, "NetAgent [AgentMsg::decode]: message type is ATMD_CMD_BADTYPE.");
      return -1;

    case ATMD_CMD_BRD:
    case ATMD_CMD_HELLO:
      // Here we expect a string
      val_type = *(_buffer+offset);
      offset++;
      if(val_type != ATMD_TYPE_STR) {
        rt_syslog(ATMD_ERR, "NetAgent [AgentMsg::decode]: %s argument has wrong type.", (_type == ATMD_CMD_BRD) ? "ATMD_CMD_BRD" : "ATMD_CMD_HELLO");
        return -1;
      }
      strncpy((char*)(_buffer+offset), _version, ATMD_VER_LEN);
      _version[ATMD_VER_LEN-1] = '\0';
      break;

    case ATMD_CMD_MEAS_SET:
      // 0) agent_id -> UINT32
      val_type = *(_buffer+offset);
      offset++;
      if(val_type != ATMD_TYPE_UINT32) {
        rt_syslog(ATMD_ERR, "NetAgent [AgentMsg::decode]: ATMD_CMD_MEAS_SET argument has wrong type.");
        return -1;
      }
      _agent_id = *(_buffer+offset);
      offset += sizeof(uint32_t);

      // 1) start_rising -> UINT8
      val_type = *(_buffer+offset);
      offset++;
      if(val_type != ATMD_TYPE_UINT8) {
        rt_syslog(ATMD_ERR, "NetAgent [AgentMsg::decode]: ATMD_CMD_MEAS_SET argument has wrong type.");
        return -1;
      }
      _start_rising = *(_buffer+offset);
      offset++;

      // 2) start_falling -> UINT8
      val_type = *(_buffer+offset);
      offset++;
      if(val_type != ATMD_TYPE_UINT8) {
        rt_syslog(ATMD_ERR, "NetAgent [AgentMsg::decode]: ATMD_CMD_MEAS_SET argument has wrong type.");
        return -1;
      }
      _start_falling = *(_buffer+offset);
      offset++;

      // 3) rising_mask -> UINT8
      val_type = *(_buffer+offset);
      offset++;
      if(val_type != ATMD_TYPE_UINT8) {
        rt_syslog(ATMD_ERR, "NetAgent [AgentMsg::decode]: ATMD_CMD_MEAS_SET argument has wrong type.");
        return -1;
      }
      _rising_mask = *(_buffer+offset);
      offset++;

      // 4) falling_mask -> UINT8
      val_type = *(_buffer+offset);
      offset++;
      if(val_type != ATMD_TYPE_UINT8) {
        rt_syslog(ATMD_ERR, "NetAgent [AgentMsg::decode]: ATMD_CMD_MEAS_SET argument has wrong type.");
        return -1;
      }
      _falling_mask = *(_buffer+offset);
      offset++;

      // 5) measure_time -> UINT64
      val_type = *(_buffer+offset);
      offset++;
      if(val_type != ATMD_TYPE_UINT64) {
        rt_syslog(ATMD_ERR, "NetAgent [AgentMsg::decode]: ATMD_CMD_MEAS_SET argument has wrong type.");
        return -1;
      }
      _measure_time = *( reinterpret_cast<uint64_t*>(_buffer+offset) );
      offset += sizeof(uint64_t);

      // 6) window_time -> UINT64
      val_type = *(_buffer+offset);
      offset++;
      if(val_type != ATMD_TYPE_UINT64) {
        rt_syslog(ATMD_ERR, "NetAgent [AgentMsg::decode]: ATMD_CMD_MEAS_SET argument has wrong type.");
        return -1;
      }
      _window_time = *( reinterpret_cast<uint64_t*>(_buffer+offset) );
      offset += sizeof(uint64_t);

      // 7) timeout -> UINT64
      val_type = *(_buffer+offset);
      offset++;
      if(val_type != ATMD_TYPE_UINT64) {
        rt_syslog(ATMD_ERR, "NetAgent [AgentMsg::decode]: ATMD_CMD_MEAS_SET argument has wrong type.");
        return -1;
      }
      _timeout = *( reinterpret_cast<uint64_t*>(_buffer+offset) );
      offset += sizeof(uint64_t);

      // 8) timeout -> UINT64
      val_type = *(_buffer+offset);
      offset++;
      if(val_type != ATMD_TYPE_UINT64) {
        rt_syslog(ATMD_ERR, "NetAgent [AgentMsg::decode]: ATMD_CMD_MEAS_SET argument has wrong type.");
        return -1;
      }
      _deadtime = *( reinterpret_cast<uint64_t*>(_buffer+offset) );
      offset += sizeof(uint64_t);

      // 9) start_offset -> UINT32
      val_type = *(_buffer+offset);
      offset++;
      if(val_type != ATMD_TYPE_UINT32) {
        rt_syslog(ATMD_ERR, "NetAgent [AgentMsg::decode]: ATMD_CMD_MEAS_SET argument has wrong type.");
        return -1;
      }
      _start_offset = *( reinterpret_cast<uint32_t*>(_buffer+offset) );
      offset += sizeof(uint32_t);

      // 10) refclk -> UINT16
      val_type = *(_buffer+offset);
      offset++;
      if(val_type != ATMD_TYPE_UINT16) {
        rt_syslog(ATMD_ERR, "NetAgent [AgentMsg::decode]: ATMD_CMD_MEAS_SET argument has wrong type.");
        return -1;
      }
      _refclk = *( reinterpret_cast<uint16_t*>(_buffer+offset) );
      offset += sizeof(uint16_t);

      // 11) hsdiv -> UINT16
      val_type = *(_buffer+offset);
      offset++;
      if(val_type != ATMD_TYPE_UINT16) {
        rt_syslog(ATMD_ERR, "NetAgent [AgentMsg::decode]: ATMD_CMD_MEAS_SET argument has wrong type.");
        return -1;
      }
      _hsdiv = *( reinterpret_cast<uint16_t*>(_buffer+offset) );
      offset += sizeof(uint16_t);

      break;

    case ATMD_CMD_MEAS_CTR:
      // Here we expect a sigle uint16_t value
      val_type = *(_buffer+offset);
      offset++;
      if(val_type != ATMD_TYPE_UINT16) {
        rt_syslog(ATMD_ERR, "NetAgent [AgentMsg::decode]: ATMD_CMD_MEAS_CTR argument has wrong type.");
        return -1;
      }
      _action = *( reinterpret_cast<uint16_t*>(_buffer+offset) );
      offset += sizeof(uint16_t);
      val_type = *(_buffer+offset);
      if(val_type != ATMD_TYPE_UINT32) {
        rt_syslog(ATMD_ERR, "NetAgent [AgentMsg::decode]: ATMD_CMD_MEAS_CTR argument has wrong type.");
        return -1;
      }
      _tdma_cycle = *( reinterpret_cast<uint32_t*>(_buffer+offset) );
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
  *( reinterpret_cast<uint16_t*>(_buffer) ) = _type;
  offset += sizeof(uint16_t) * 2; // Skip size

  switch(_type) {
    case ATMD_CMD_BADTYPE:
      rt_syslog(ATMD_ERR, "NetAgent [AgentMsg::encode]: message type is ATMD_CMD_BADTYPE.");
      return -1;

    case ATMD_CMD_BRD:
    case ATMD_CMD_HELLO:
      *(_buffer+offset) = ATMD_TYPE_STR;
      offset++;
      strncpy((char*)(_buffer+offset), _version, ATMD_VER_LEN);
      offset += strlen(_version);
      break;

    case ATMD_CMD_MEAS_SET:
      // 0) agent_id
      *(_buffer+offset) = ATMD_TYPE_UINT32;
      offset++;
      *( reinterpret_cast<uint32_t*>(_buffer+offset) ) = _agent_id;
      offset += sizeof(uint32_t);

      // 1) start_rising -> UINT8
      *(_buffer+offset) = ATMD_TYPE_UINT8;
      offset++;
      *( reinterpret_cast<uint8_t*>(_buffer+offset) ) = _start_rising;
      offset += sizeof(uint8_t);

      // 2) start_falling -> UINT8
      *(_buffer+offset) = ATMD_TYPE_UINT8;
      offset++;
      *( reinterpret_cast<uint8_t*>(_buffer+offset) ) = _start_falling;
      offset += sizeof(uint8_t);

      // 3) rising_mask -> UINT8
      *(_buffer+offset) = ATMD_TYPE_UINT8;
      offset++;
      *( reinterpret_cast<uint8_t*>(_buffer+offset) ) = _rising_mask;
      offset += sizeof(uint8_t);

      // 4) falling_mask -> UINT8
      *(_buffer+offset) = ATMD_TYPE_UINT8;
      offset++;
      *( reinterpret_cast<uint8_t*>(_buffer+offset) ) = _falling_mask;
      offset += sizeof(uint8_t);

      // 5) measure_time -> UINT64
      *(_buffer+offset) = ATMD_TYPE_UINT64;
      offset++;
      *( reinterpret_cast<uint64_t*>(_buffer+offset) ) = _measure_time;
      offset += sizeof(uint64_t);

      // 6) window_time -> UINT64
      *(_buffer+offset) = ATMD_TYPE_UINT64;
      offset++;
      *( reinterpret_cast<uint64_t*>(_buffer+offset) ) = _window_time;
      offset += sizeof(uint64_t);

      // 7) timeout -> UINT64
      *(_buffer+offset) = ATMD_TYPE_UINT64;
      offset++;
      *( reinterpret_cast<uint64_t*>(_buffer+offset) ) = _timeout;
      offset += sizeof(uint64_t);

      // 8) deadtime -> UINT64
      *(_buffer+offset) = ATMD_TYPE_UINT64;
      offset++;
      *( reinterpret_cast<uint64_t*>(_buffer+offset) ) = _deadtime;
      offset += sizeof(uint64_t);
      break;

      // 9) start_offset -> UINT32
      *(_buffer+offset) = ATMD_TYPE_UINT32;
      offset++;
      *( reinterpret_cast<uint32_t*>(_buffer+offset) ) = _start_offset;
      offset += sizeof(uint32_t);
      break;

      // 10) refclk -> UINT16
      *(_buffer+offset) = ATMD_TYPE_UINT16;
      offset++;
      *( reinterpret_cast<uint16_t*>(_buffer+offset) ) = _refclk;
      offset += sizeof(uint16_t);
      break;

      // 11) hsdiv -> UINT16
      *(_buffer+offset) = ATMD_TYPE_UINT16;
      offset++;
      *( reinterpret_cast<uint16_t*>(_buffer+offset) ) = _hsdiv;
      offset += sizeof(uint16_t);
      break;

    case ATMD_CMD_MEAS_CTR:
      *(_buffer+offset) = ATMD_TYPE_UINT16;
      offset++;
      *( reinterpret_cast<uint16_t*>(_buffer+offset) ) = _action;
      offset += sizeof(uint16_t);
      *(_buffer+offset) = ATMD_TYPE_UINT32;
      offset++;
      *( reinterpret_cast<uint32_t*>(_buffer+offset) ) = _tdma_cycle;
      offset += sizeof(uint32_t);
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
  *( reinterpret_cast<uint16_t*>(_buffer+sizeof(uint16_t)) ) = _size;

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
