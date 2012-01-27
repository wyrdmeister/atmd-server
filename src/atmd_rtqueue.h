/*
 * ATMD Server version 3.0
 *
 * ATMD Server - RTqueue class header
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

#ifndef ATMD_RTQUEUE_H
#define ATMD_RTQUEUE_H

// Global
#include <string.h>

// Xenomai
#include <rtdk.h>
#include <native/queue.h>

// Local
#include "common.h"


/* @class RTqueue
 *
 */
class RTqueue {
public:
  RTqueue() {};
  ~RTqueue() { rt_queue_delete(&_descriptor); };

  // Create queue
  int init(const char * name, size_t pool = 1000000);

  // Send message to queue
  int send(const char * buffer, size_t buff_size);

  // Receive message from queue
  int recv(char * buffer, size_t &buff_size, int64_t timeout = TM_INFINITE);

private:
  // Queue descriptor
  RT_QUEUE _descriptor;
};

#endif
