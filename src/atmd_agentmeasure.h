/*
 * ATMD Server version 3.0
 *
 * ATMD Agent - Measurement function header
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

#ifndef ATMD_AGENT_MEAS_H
#define ATMD_AGENT_MEAS_H

// General
#include <stdint.h>
#include <string.h>
#include <netinet/ether.h>

// Xenomai
#include <rtdk.h>
#include <native/task.h>
#include <native/heap.h>
#include <native/timer.h>

// Local
#include "common.h"
#include "xenovec.h"
#include "atmd_rtnet.h"
#include "atmd_hardware.h"
#include "atmd_netagent.h"
#include "atmd_rtcomm.h"

// Defines
#define ATMD_BLOCK 512 // Size of a data block


/* @class InitData
 * This class contains basic parameters passed to the real-time thread that controls the acquisition.
 */
class InitData {
public:

  // Heap name
  const char* heap_name;

  // Board
  ATMDboard *board;

  // Data socket
  RTnet* sock;

  // Master address
  struct ether_addr* addr;

};


/* @class MeasureDef
 * Pass the measure configuration to the measurement thread.
 */
class MeasureDef {
public:
  // Manage timings
  void measure_time(RTIME time) { _measure_time = time; };
  RTIME measure_time() { return _measure_time; };
  void window_time(RTIME time) { _window_time = time; };
  RTIME window_time() { return _window_time; };
  void timeout(RTIME time) { _timeout = time; };
  RTIME timeout() { return _timeout; };
  void deadtime(RTIME time) { _deadtime = time; };
  RTIME deadtime() { return _deadtime; };

  // TDMA cycle
  void tdma_cycle(uint32_t val) { _tdma_cycle = val; };
  uint32_t tdma_cycle()const { return _tdma_cycle; };

private:
  // Timings
  RTIME _measure_time;
  RTIME _window_time;
  RTIME _timeout;
  RTIME _deadtime;

  // TDMA cycle
  uint32_t _tdma_cycle;
};


/* @class EventData
 * This class holds the data acquired during a start event.
 */
class EventData {
public:
  // Contructors
  EventData(RT_HEAP *heap) : _ch(heap), _stop(heap), _retrig(heap), _window_begin(0), _window_end(0) {};

  // Destructor
  ~EventData() {};

  // Add one stop event
  void add(int8_t ch, int32_t stop, uint32_t retrig) {
    if(_ch.size() >= _ch.capacity()) {
      _ch.reserve(_ch.size() + ATMD_BLOCK);
      _stop.reserve(_stop.size() + ATMD_BLOCK);
      _retrig.reserve(_retrig.size() + ATMD_BLOCK);
    }
    _ch.push_back(ch);
    _stop.push_back(stop);
    _retrig.push_back(retrig);
  };

  // Size operator
  size_t size()const { return _ch.size(); };

  // Reserve storage space
  void reserve(size_t sz) {
    _ch.reserve(sz);
    _stop.reserve(sz);
    _retrig.reserve(sz);
  };

  // Clear operator
  void clear() { _ch.clear(); _stop.clear(); _retrig.clear(); };

  // Read operators
  const xenovec<int8_t>& ch()const { return _ch; };
  const xenovec<int32_t>& stoptime()const { return _stop; };
  const xenovec<uint32_t>& retrig()const { return _retrig; };

  // Time operators
  void begin(RTIME time) { _window_begin = time; };
  RTIME begin()const { return _window_begin; };
  void end(RTIME time) { _window_end = time; };
  RTIME end()const { return _window_end; };

  // Add start01 where needed
  void compute_start01(uint32_t start01) {
    for(size_t i = 0; i < this->size(); i++) {
      if(_retrig[i] > 0) {
        _stop[i] = _stop[i] + start01;
        _retrig[i] = _retrig[i] - 1;
      }
    }
  };

private:
  // HEAP obj for dynamic allocation
  RT_HEAP * _heap;

  // Vectors to handle data
  xenovec<int8_t> _ch;
  xenovec<int32_t> _stop;
  xenovec<uint32_t> _retrig;

  // Timings
  RTIME _window_begin;
  RTIME _window_end;
};


// Function prototypes
void atmd_measure(void *arg);
int atmd_get_start(ATMDboard* board, RTIME window, RTIME timeout, EventData& events);
int atmd_send_start(uint32_t id, EventData& events, RTnet* sock, const struct ether_addr* addr);

#endif
