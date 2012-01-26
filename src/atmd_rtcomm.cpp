/*
 * ATMD Server version 3.0
 *
 * ATMD Server - RTcomm class
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

#include "atmd_rtcomm.h"


/* @fn int RTcomm::send(int opcode, void* buffer, size_t buff_size, void * answer, size_t* answer_size)
 * 
 */
int RTcomm::send(int& opcode, const void* buffer, size_t buff_size, void * answer, size_t* answer_size) {

  // Check task descriptor
  if(!_task) {
    rt_syslog(ATMD_ERR, "RTcomm [send]: this object is only a receiver. No task descriptor configured.");
    return -1;
  }

  // Check that buffer is not too large
  if(buff_size > ATMD_PACKET_SIZE) {
    rt_syslog(ATMD_ERR, "RTcomm [send]: submitted buffer is too large.");
    return -1;
  }

  // Clear buffers
  memset(_send_buff, 0, ATMD_PACKET_SIZE);
  memset(_recv_buff, 0, ATMD_PACKET_SIZE);

  // Copy message
  memcpy(_send_buff, buffer, buff_size);
  _send.size = buff_size;
  _send.opcode = opcode;

  // Send message
  int retval = rt_task_send(_task, &_send, (answer != NULL) ? &_recv : NULL, TM_INFINITE);
  if(retval < 0) {
    switch(retval) {
      case -ENOBUFS:
        rt_syslog(ATMD_ERR, "RTcomm [send]: rt_task_send() failed. Buffer not large enough to store reply message.");
        break;

      case -EIDRM:
        rt_syslog(ATMD_ERR, "RTcomm [send]: rt_task_send() failed. Task deleted while waiting for a reply.");
        break;

      case -EINTR:
        rt_syslog(ATMD_ERR, "RTcomm [send]: rt_task_send() failed. Task unblocked before reply.");
        break;

      case -EPERM:
        rt_syslog(ATMD_ERR, "RTcomm [send]: rt_task_send() failed. Invalid context.");
        break;

      case -ESRCH:
        rt_syslog(ATMD_ERR, "RTcomm [send]: rt_task_send() failed. The task cannot be found.");
        break;

      default:
        rt_syslog(ATMD_ERR, "RTcomm [send]: rt_task_send() failed. Unexpected return value (%d).", retval);
        break;
    }
    return -1;
  }

  if(answer == NULL)
    return 0;

  // Save opcode
  opcode = _recv.opcode;

  // If answer received
  if(answer && retval > 0) {
    if((size_t)retval > *answer_size) {
      // Buffer too smalll
      rt_syslog(ATMD_WARN, "RTcomm [send]: answer was expecter but was empty.");
      return -1;

    } else {
      // Copy message
      memcpy(answer, _recv.data, retval);
      *answer_size = retval;
    }

  } else {
    rt_syslog(ATMD_WARN, "RTcomm [send]: answer was expecter but was empty.");
    return -1;
  }

  return 0;
}


/* @fn int RTcomm::recv(int& opcode, void* buffer, size_t& buff_size)
 * 
 */
int RTcomm::recv(int& opcode, void* buffer, size_t& buff_size) {

  // Clear buffer
  memset(_recv_buff, 0, ATMD_PACKET_SIZE);
  _recv.size = ATMD_PACKET_SIZE;

  // Receive message
  int retval = rt_task_receive(&_recv, TM_INFINITE);
  if(retval <= 0) {
    switch(retval) {
      case -ENOBUFS:
        rt_syslog(ATMD_ERR, "RTcomm [recv]: rt_task_receive() failed. Buffer not large enough to store message.");
        break;
  
      case -EINTR:
        rt_syslog(ATMD_ERR, "RTcomm [recv]: rt_task_receive() failed. Task unblocked before any message arrives.");
        break;
  
      case -EPERM:
        rt_syslog(ATMD_ERR, "RTcomm [recv]: rt_task_receive() failed. Invalid context.");
        break;

      default:
        rt_syslog(ATMD_ERR, "RTcomm [recv]: rt_task_receive() failed. Unexpected return value (%d).", retval);
        break;
    }
    return -1;
  }

  // Store flowid
  _flow_id = retval;

  // Get opcode
  opcode = _recv.opcode;

  // Check message size
  if(_recv.size > buff_size) {
    rt_syslog(ATMD_ERR, "RTcomm [recv]: message is too big for supplied buffer.");
    return -1;
  }

  if(_recv.size > 0) {
    // Copy message
    memcpy(buffer, _recv.data, _recv.size);
    buff_size = _recv.size;

  } else {
    // Got empty message
    rt_syslog(ATMD_ERR, "RTcomm [recv]: got empty message.");
    return -1;
  }

  return 0;
}


/* @fn int RTcomm::reply(int opcode, void* buffer, size_t buff_size)
 * 
 */
int RTcomm::reply(int opcode, const void* buffer, size_t buff_size) {
  // Check flowid
  if(_flow_id <= 0) {
    rt_syslog(ATMD_ERR, "RTcomm [reply]: no valid flow id present.");
    return -1;
  }

  // Check message length
  if(buff_size > ATMD_PACKET_SIZE) {
    rt_syslog(ATMD_ERR, "RTcomm [reply]: supplied message is too large.");
    return -1;
  }

  // Clear buffer
  memset(_send_buff, 0, ATMD_PACKET_SIZE);

  // Copy message
  memcpy(_send_buff, buffer, buff_size);
  _send.size = buff_size;
  _send.opcode = opcode;

  // Send reply
  int retval = rt_task_reply(_flow_id, &_send);
  if(retval) {
    switch(retval) {
      case -EINVAL:
        rt_syslog(ATMD_ERR, "RTcomm [reply]: rt_task_reply() failed. Invalid flow id.");
        break;

      case -ENXIO:
        rt_syslog(ATMD_ERR, "RTcomm [reply]: rt_task_reply() failed. Flow id mismatch or remote task was unblocked.");
        break;

      case -EPERM:
        rt_syslog(ATMD_ERR, "RTcomm [reply]: rt_task_reply() failed. Invalid context.");
        break;

      default:
        rt_syslog(ATMD_ERR, "RTcomm [reply]: rt_task_reply() failed.");
        break;
    }
    return -1;
  }

  return 0;
}
