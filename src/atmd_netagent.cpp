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


/* @fn
 * Serialize template
 */
template<typename T>
size_t serialize(char* buffer, size_t offset, T var) {
  memcpy(buffer+offset, (void*)&var, sizeof(T));
  return (offset+sizeof(T));
}


/*
 * Deserialize template
 */
template<typename T>
size_t deserialize(const char* buffer, size_t offset, T& var) {
  memcpy((void*)&var, buffer+offset, sizeof(T));
  return (offset+sizeof(T));
}


/* @fn int AgentMsg::decode()
 * Decode a control message
 */
int AgentMsg::decode() {

  // Service variables
  size_t offset = 0;
  uint8_t val_type = 0;

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
      offset = deserialize<uint8_t>(_buffer, offset, val_type);
      if(val_type != ATMD_TYPE_STR) {
        rt_syslog(ATMD_ERR, "NetAgent [AgentMsg::decode]: %s argument has wrong type.", (_type == ATMD_CMD_BRD) ? "ATMD_CMD_BRD" : "ATMD_CMD_HELLO");
        return -1;
      }
      strncpy(_version, (char*)(_buffer+offset), ATMD_VER_LEN);
      _version[ATMD_VER_LEN-1] = '\0';
      offset += strlen(_buffer+offset)+1;
      break;

    case ATMD_CMD_MEAS_SET:
      // 0) agent_id -> UINT32
      offset = deserialize<uint8_t>(_buffer, offset, val_type);
      if(val_type != ATMD_TYPE_UINT32) {
        rt_syslog(ATMD_ERR, "NetAgent [AgentMsg::decode]: ATMD_CMD_MEAS_SET agent_id argument has wrong type.");
        return -1;
      }
      offset = deserialize<uint32_t>(_buffer, offset, _agent_id);

      // 1) start_rising -> UINT8
      offset = deserialize<uint8_t>(_buffer, offset, val_type);
      if(val_type != ATMD_TYPE_UINT8) {
        rt_syslog(ATMD_ERR, "NetAgent [AgentMsg::decode]: ATMD_CMD_MEAS_SET start_rising argument has wrong type.");
        return -1;
      }
      offset = deserialize<uint8_t>(_buffer, offset, _start_rising);

      // 2) start_falling -> UINT8
      offset = deserialize<uint8_t>(_buffer, offset, val_type);
      if(val_type != ATMD_TYPE_UINT8) {
        rt_syslog(ATMD_ERR, "NetAgent [AgentMsg::decode]: ATMD_CMD_MEAS_SET start_falling argument has wrong type.");
        return -1;
      }
      offset = deserialize<uint8_t>(_buffer, offset, _start_falling);

      // 3) rising_mask -> UINT8
      offset = deserialize<uint8_t>(_buffer, offset, val_type);
      if(val_type != ATMD_TYPE_UINT8) {
        rt_syslog(ATMD_ERR, "NetAgent [AgentMsg::decode]: ATMD_CMD_MEAS_SET rising_mask argument has wrong type.");
        return -1;
      }
      offset = deserialize<uint8_t>(_buffer, offset, _rising_mask);

      // 4) falling_mask -> UINT8
      offset = deserialize<uint8_t>(_buffer, offset, val_type);
      if(val_type != ATMD_TYPE_UINT8) {
        rt_syslog(ATMD_ERR, "NetAgent [AgentMsg::decode]: ATMD_CMD_MEAS_SET falling_mask argument has wrong type.");
        return -1;
      }
      offset = deserialize<uint8_t>(_buffer, offset, _falling_mask);

      // 5) measure_time -> UINT64
      offset = deserialize<uint8_t>(_buffer, offset, val_type);
      if(val_type != ATMD_TYPE_UINT64) {
        rt_syslog(ATMD_ERR, "NetAgent [AgentMsg::decode]: ATMD_CMD_MEAS_SET measure_time argument has wrong type.");
        return -1;
      }
      offset = deserialize<uint64_t>(_buffer, offset, _measure_time);

      // 6) window_time -> UINT64
      offset = deserialize<uint8_t>(_buffer, offset, val_type);
      if(val_type != ATMD_TYPE_UINT64) {
        rt_syslog(ATMD_ERR, "NetAgent [AgentMsg::decode]: ATMD_CMD_MEAS_SET window_time argument has wrong type.");
        return -1;
      }
      offset = deserialize<uint64_t>(_buffer, offset, _window_time);

      // 7) timeout -> UINT64
      offset = deserialize<uint8_t>(_buffer, offset, val_type);
      if(val_type != ATMD_TYPE_UINT64) {
        rt_syslog(ATMD_ERR, "NetAgent [AgentMsg::decode]: ATMD_CMD_MEAS_SET deadtime argument has wrong type.");
        return -1;
      }
      offset = deserialize<uint64_t>(_buffer, offset, _deadtime);

      // 8) start_offset -> UINT32
      offset = deserialize<uint8_t>(_buffer, offset, val_type);
      if(val_type != ATMD_TYPE_UINT32) {
        rt_syslog(ATMD_ERR, "NetAgent [AgentMsg::decode]: ATMD_CMD_MEAS_SET start_offset argument has wrong type.");
        return -1;
      }
      offset = deserialize<uint32_t>(_buffer, offset, _start_offset);

      // 9) refclk -> UINT16
      offset = deserialize<uint8_t>(_buffer, offset, val_type);
      if(val_type != ATMD_TYPE_UINT16) {
        rt_syslog(ATMD_ERR, "NetAgent [AgentMsg::decode]: ATMD_CMD_MEAS_SET refclk argument has wrong type.");
        return -1;
      }
      offset = deserialize<uint16_t>(_buffer, offset, _refclk);

      // 10) hsdiv -> UINT16
      offset = deserialize<uint8_t>(_buffer, offset, val_type);
      if(val_type != ATMD_TYPE_UINT16) {
        rt_syslog(ATMD_ERR, "NetAgent [AgentMsg::decode]: ATMD_CMD_MEAS_SET hsdiv argument has wrong type.");
        return -1;
      }
      offset = deserialize<uint16_t>(_buffer, offset, _hsdiv);
      break;

    case ATMD_CMD_MEAS_CTR:
      // 1) Action
      offset = deserialize<uint8_t>(_buffer, offset, val_type);
      if(val_type != ATMD_TYPE_UINT16) {
        rt_syslog(ATMD_ERR, "NetAgent [AgentMsg::decode]: ATMD_CMD_MEAS_CTR action argument has wrong type.");
        return -1;
      }
      offset = deserialize<uint16_t>(_buffer, offset, _action);

      // 2) TDMA cycle
      offset = deserialize<uint8_t>(_buffer, offset, val_type);
      if(val_type != ATMD_TYPE_UINT64) {
        rt_syslog(ATMD_ERR, "NetAgent [AgentMsg::decode]: ATMD_CMD_MEAS_CTR tdma_cycle argument has wrong type.");
        return -1;
      }
      offset = deserialize<uint64_t>(_buffer, offset, _tdma_cycle);
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

  // Skip size
  offset += sizeof(uint16_t);

  switch(_type) {
    case ATMD_CMD_BADTYPE:
      rt_syslog(ATMD_ERR, "NetAgent [AgentMsg::encode]: message type is ATMD_CMD_BADTYPE.");
      return -1;

    case ATMD_CMD_BRD:
    case ATMD_CMD_HELLO:
      offset = serialize<uint8_t>(_buffer, offset, ATMD_TYPE_STR);
      strncpy((char*)(_buffer+offset), _version, ATMD_VER_LEN);
      offset += strlen(_version)+1;
      break;

    case ATMD_CMD_MEAS_SET:
      // 0) agent_id
      offset = serialize<uint8_t>(_buffer, offset, ATMD_TYPE_UINT32);

      *( reinterpret_cast<uint32_t*>(_buffer+offset) ) = _agent_id;
      offset += sizeof(uint32_t);

      // 1) start_rising -> UINT8
      offset = serialize<uint8_t>(_buffer, offset, ATMD_TYPE_UINT8);
      offset = serialize<uint8_t>(_buffer, offset, _start_rising);

      // 2) start_falling -> UINT8
      offset = serialize<uint8_t>(_buffer, offset, ATMD_TYPE_UINT8);
      offset = serialize<uint8_t>(_buffer, offset, _start_falling);

      // 3) rising_mask -> UINT8
      offset = serialize<uint8_t>(_buffer, offset, ATMD_TYPE_UINT8);
      offset = serialize<uint8_t>(_buffer, offset, _rising_mask);

      // 4) falling_mask -> UINT8
      offset = serialize<uint8_t>(_buffer, offset, ATMD_TYPE_UINT8);
      offset = serialize<uint8_t>(_buffer, offset, _falling_mask);

      // 5) measure_time -> UINT64
      offset = serialize<uint8_t>(_buffer, offset, ATMD_TYPE_UINT64);
      offset = serialize<uint64_t>(_buffer, offset, _measure_time);

      // 6) window_time -> UINT64
      offset = serialize<uint8_t>(_buffer, offset, ATMD_TYPE_UINT64);
      offset = serialize<uint64_t>(_buffer, offset, _window_time);

      // 7) deadtime -> UINT64
      offset = serialize<uint8_t>(_buffer, offset, ATMD_TYPE_UINT64);
      offset = serialize<uint64_t>(_buffer, offset, _deadtime);

      // 8) start_offset -> UINT32
      offset = serialize<uint8_t>(_buffer, offset, ATMD_TYPE_UINT32);
      offset = serialize<uint32_t>(_buffer, offset, _start_offset);

      // 9) refclk -> UINT16
      offset = serialize<uint8_t>(_buffer, offset, ATMD_TYPE_UINT16);
      offset = serialize<uint16_t>(_buffer, offset, _refclk);

      // 10) hsdiv -> UINT16
      offset = serialize<uint8_t>(_buffer, offset, ATMD_TYPE_UINT16);
      offset = serialize<uint16_t>(_buffer, offset, _hsdiv);
      break;

    case ATMD_CMD_MEAS_CTR:
      // 1) Action
      offset = serialize<uint8_t>(_buffer, offset, ATMD_TYPE_UINT16);
      offset = serialize<uint16_t>(_buffer, offset, _action);

      // 2) TDMA cycle
      offset = serialize<uint8_t>(_buffer, offset, ATMD_TYPE_UINT64);
      offset = serialize<uint64_t>(_buffer, offset, _tdma_cycle);
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
  offset = serialize<uint16_t>(_buffer, sizeof(uint16_t), _size);

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

  // Leave type and number of events as last job
  size_t offset = 2 * sizeof(uint16_t);

  // Set start ID
  offset = serialize<uint32_t>(_buffer, offset, _id);

  // Check if this is the first packet
  if(start == 0) {
    _type = ATMD_DT_FIRST;

    // Total number of events
    _total_events = ch.size();
    offset = serialize<uint32_t>(_buffer, offset, _total_events);

    // Window start time
    offset = serialize<uint64_t>(_buffer, offset, _window_start);

    // Window duration
    offset = serialize<uint64_t>(_buffer, offset, _window_time);

  } else {
    _type = ATMD_DT_DATA;
  }

  // Add events
  uint16_t count = 0;
  while(true) {

    // Add one event
    offset = serialize<int8_t>(_buffer, offset, ch[start]);
    offset = serialize<int32_t>(_buffer, offset, stoptime[start]);
    offset = serialize<uint32_t>(_buffer, offset, retrig[start]);

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
      if(offset+ATMD_EV_SIZE >= ATMD_PACKET_SIZE)
        break;
    }
  }

  // Set packet size
  _size = offset;

  // Write type and number of events
  offset = serialize<uint16_t>(_buffer, 0, _type);
  offset = serialize<uint16_t>(_buffer, offset, count);

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
    offset = serialize<uint16_t>(_buffer, offset, _type);

    // Skip numev and start_id
    offset += sizeof(uint16_t) + sizeof(uint32_t);

    // Measure start time
    offset = serialize<uint64_t>(_buffer, offset, _window_start);

    // Measure duration
    offset = serialize<uint64_t>(_buffer, offset, _window_time);

    // Set size
    _size = offset;
    return 0;


  } else if(_type == ATMD_DT_ONLY) {
    // We got an empty start and we should anyway send a message to the master

    // Clear buffer
    memset(_buffer, 0, ATMD_PACKET_SIZE);

    size_t offset = 0;

    // Set type
    offset = serialize<uint16_t>(_buffer, offset, _type);

    // Set number of events to zero
    _numev = 0;
    offset = serialize<uint16_t>(_buffer, offset, _numev);

    // Set start ID
    offset = serialize<uint32_t>(_buffer, offset, _id);

    // Total number of events
    _total_events = 0;
    offset = serialize<uint32_t>(_buffer, offset, _total_events);

    // Window start time
    offset = serialize<uint64_t>(_buffer, offset, _window_start);

    // Window duration
    offset = serialize<uint64_t>(_buffer, offset, _window_time);

    // Set size
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
  offset = deserialize<uint16_t>(_buffer, offset, _type);

  // Read number of events
  offset = deserialize<uint16_t>((const char*)_buffer, (size_t)offset, _numev);

  // Read ID
  offset = deserialize<uint32_t>(_buffer, offset, _id);

  switch(_type) {
    case ATMD_DT_FIRST:
    case ATMD_DT_ONLY:
      // Total events
      offset = deserialize<uint32_t>(_buffer, offset, _total_events);

      // Window start time
      offset = deserialize<uint64_t>(_buffer, offset, _window_start);

      // Window duration
      offset = deserialize<uint64_t>(_buffer, offset, _window_time);

    case ATMD_DT_DATA:
    case ATMD_DT_LAST:
      // Set event offset
      _ev_offset = offset;
      break;

    case ATMD_DT_TERM:
      // Measure start time
      offset = deserialize<uint64_t>(_buffer, offset, _window_start);

      // Measure duration
      offset = deserialize<uint64_t>(_buffer, offset, _window_time);
      break;

    default:
      rt_syslog(ATMD_ERR, "NetAgent [DataMsg::decode]: trying to decode an unknown message type.");
      return -1;
  }

  // Setup size
  _size = offset + _numev * ATMD_EV_SIZE;

  return 0;
}


/* @fn int DataMsg::decode()
 * Decode a data message
 */
int DataMsg::getevent(size_t i, int8_t& ch, int32_t& stoptime, uint32_t& retrig)const {
  // Check if we reached the last event
  if(i >= _numev)
    return -1;

  // Compute start offset
  size_t offset = _ev_offset + ATMD_EV_SIZE*i;

  // Check buffer limits
  if(offset + ATMD_EV_SIZE > ATMD_PACKET_SIZE)
    return -1;

  // Extract event
  offset = deserialize<int8_t>(_buffer, offset, ch);
  offset = deserialize<int32_t>(_buffer, offset, stoptime);
  offset = deserialize<uint32_t>(_buffer, offset, retrig);

  return 0;
}
