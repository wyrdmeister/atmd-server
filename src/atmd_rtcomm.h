/*
 * ATMD Server version 3.0
 *
 * ATMD Server - RTcomm class header
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

#ifndef ATMD_RTCOMM_H
#define ATMD_RTCOMM_H

// Global
#include <string.h>

// Xenomai
#include <rtdk.h>
#include <native/task.h>

// Local
#include "common.h"
#include "atmd_netagent.h"


/* @class RTcomm
 *
 */
class RTcomm {
public:
  RTcomm(): _task(NULL), _flow_id(-1) {
    memset(_send_buff, 0, ATMD_PACKET_SIZE);
    memset(_recv_buff, 0, ATMD_PACKET_SIZE);
    memset(&_send, 0, sizeof(RT_TASK_MCB));
    memset(&_recv, 0, sizeof(RT_TASK_MCB));
    _send.data = _send_buff;
    _send.size = ATMD_PACKET_SIZE;
    _recv.data = _recv_buff;
    _recv.size = ATMD_PACKET_SIZE;
  };
  ~RTcomm() {};

  // Init
  int init(RT_TASK* descriptor) {
    if(!descriptor)
      return -1;
    _task = descriptor;
    return 0;
  };

  // Send
  int send(int& opcode, const void* buffer, size_t buff_size, void* answer, size_t* answer_size);

  // Receive
  int recv(int& opcode, void* buffer, size_t& buff_size, int64_t timeout = TM_INFINITE);

  // Reply
  int reply(int opcode, const void* buffer, size_t buff_size);

private:
  // Task descriptor
  RT_TASK* _task;

  // Flow id of last receive
  int _flow_id;

  // Message blocks
  RT_TASK_MCB _send;
  RT_TASK_MCB _recv;

  // Message buffers
  char _send_buff[ATMD_PACKET_SIZE];
  char _recv_buff[ATMD_PACKET_SIZE];
};

#endif
