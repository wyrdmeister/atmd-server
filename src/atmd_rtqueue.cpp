/*
 * ATMD Server version 3.0
 *
 * ATMD Server - RTqueue class
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

#include "atmd_rtqueue.h"


/* @fn int RTqueue::init(const char * name, size_t pool = 1000000)
 *
 */
int RTqueue::init(const char * name, size_t pool) {

  int retval = rt_queue_create(&_descriptor, name, pool, Q_UNLIMITED, Q_SHARED);
  if(retval) {
    switch(retval) {
      case -ENOMEM:
        rt_syslog(ATMD_CRIT, "RTqueue [init]: rt_queue_create() failed. Not enough memory available.");
        break;

      case -EEXIST:
        rt_syslog(ATMD_CRIT, "RTqueue [init]: rt_queue_create() failed. The given name is already in use.");
        break;

      case -EPERM:
        rt_syslog(ATMD_CRIT, "RTqueue [init]: rt_queue_create() failed. Called from invalid context.");
        break;

      case -EINVAL:
        rt_syslog(ATMD_CRIT, "RTqueue [init]: rt_queue_create() failed. Invalid parameter.");
        break;

      case -ENOENT:
        rt_syslog(ATMD_CRIT, "RTqueue [init]: rt_queue_create() failed. Cannot open /dev/rtheap.");
        break;

      default:
        rt_syslog(ATMD_CRIT, "RTqueue [init]: rt_queue_create() failed with an unexpected error (%d).", retval);
        break;
    }
    return -1;
  }

  return 0;
}


/* @fn int RTqueue::send(const void *buffer, size_t buff_size)
 *
 */
int RTqueue::send(const char *buffer, size_t buff_size) {

  // Allocate message
  void *msg = rt_queue_alloc(&_descriptor, buff_size);
  if(msg == NULL) {
    rt_syslog(ATMD_ERR, "RTqueue [send]: rt_queue_alloc() failed.");
    return -1;
  }

  // Copy buffer
  memcpy(msg, buffer, buff_size);

  // Send message
  int retval = rt_queue_send(&_descriptor, msg, buff_size, Q_NORMAL);
  if(retval < 0) {
    switch(retval) {
      case -EINVAL:
        rt_syslog(ATMD_CRIT, "RTqueue [send]: rt_queue_send() failed. Invalid queue descriptor.");
        break;

      case -EIDRM:
        rt_syslog(ATMD_CRIT, "RTqueue [send]: rt_queue_send() failed. Close queue descriptor.");
        break;

      case -ENOMEM:
        rt_syslog(ATMD_CRIT, "RTqueue [send]: rt_queue_send() failed. Not enough memory for the message.");
        break;

      default:
        rt_syslog(ATMD_CRIT, "RTqueue [send]: rt_queue_send() failed with an unexpected error (%d)", retval);
        break;
    }
    return -1;
  }

#ifdef DEBUG
  if(enable_debug)
    rt_syslog(ATMD_DEBUG, "RTqueue [send]: successfully sent packet to queue with size '%d'", buff_size);
#endif

  return 0;
}


/* @fn int RTqueue::recv(void * buffer, size_t& buff_size)
 *
 */
int RTqueue::recv(char * buffer, size_t& buff_size, int64_t timeout) {

  void * msg = NULL;

  // Receive message
  int retval = rt_queue_receive(&_descriptor, &msg, timeout);
  if(retval < 0) {
    if(retval == -EWOULDBLOCK || retval == -ETIMEDOUT)
      return -EWOULDBLOCK;

    // Receive failed
    switch(retval) {
      case -EINVAL:
       rt_syslog(ATMD_CRIT, "RTqueue [recv]: rt_queue_receive() failed. Invalid queue descriptor.");
         break;

      case -EIDRM:
        rt_syslog(ATMD_CRIT, "RTqueue [recv]: rt_queue_receive() failed. Deleted queue descriptor.");
        break;

      case -EINTR:
        rt_syslog(ATMD_CRIT, "RTqueue [recv]: rt_queue_receive() failed. Unblocked by rt_task_unblock().");
        break;

      case -EPERM:
        rt_syslog(ATMD_CRIT, "RTqueue [recv]: rt_queue_receive() failed. Called from an invalid context.");
        break;

      default:
        rt_syslog(ATMD_CRIT, "RTqueue [recv]: rt_queue_receive() failed with unexpected return code (%d).", retval);
        break;
    }
    return -1;
  }

#ifdef DEBUG
  if(enable_debug)
    rt_syslog(ATMD_DEBUG, "RTqueue [recv]: successfully received packet from queue with size '%d'", retval);
#endif

  // Check length of message
  if(retval > 0 && (size_t)retval > buff_size) {
    rt_syslog(ATMD_ERR, "RTqueue [recv]: given buffer is too small to handle message (size was %d and buffer was %u).", retval, buff_size);
    return -1;
  }

  // Copy message to buffer
  memcpy(buffer, msg, retval);
  buff_size = retval;

  // Free message
  rt_queue_free(&_descriptor, msg);

  return 0;
}
