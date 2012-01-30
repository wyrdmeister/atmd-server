/*
 * ATMD Server version 3.0
 *
 * ATMD Server - Measure handling class header
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

#ifndef ATMD_MEASURE_H
#define ATMD_MEASURE_H

// Global
#include <stdlib.h>
#include <stdint.h>
#include <vector>
#include <exception>

// Local
#include "common.h"

// Declare class VirtualBoard for friendship
class VirtualBoard;


/* @class StartData
 * This class hold the data of on start event
 */
class StartData {
public:
  StartData(): time_bin(0.0) {};
  ~StartData() {};

  int add_event(uint32_t retrig, int32_t stop, int8_t ch);
  int get_event(uint32_t num, uint32_t& retrig, int32_t& stop, int8_t& ch)const;
  int get_channel(uint32_t num, int8_t& ch)const;
  int get_stoptime(uint32_t num, double& stop)const;
  int get_rawstoptime(uint32_t num, uint32_t& retrig, double& stop)const;
  uint32_t count_stops()const { return this->retrig_count.size(); };

  void clear() {
    this->retrig_count.clear();
    this->stoptime.clear();
    this->channel.clear();
    this->window_time.clear();
    this->window_begin.clear();
    this->time_bin = 0.0;
  };

  // Interface for managing effective window time
  void add_time(uint64_t begin, uint64_t duration) { window_begin.push_back(begin); window_time.push_back(duration); };
  size_t times()const { return window_begin.size(); };
  uint64_t get_window_time(size_t i)const { return window_time[i]; };
  uint64_t get_window_begin(size_t i)const { return window_begin[i]; };

  // Interface to manage time_bin
  void set_tbin(double tbin) { this->time_bin = tbin; };
  double get_tbin()const { return this->time_bin; };

  // Preallocate
  void reserve(size_t sz);

  // Merge multiple StartData struct into one
  static StartData* merge(const std::vector<StartData*>& svec);

private:
  std::vector<uint64_t> window_time;    // Effective window time in nanoseconds
  std::vector<uint64_t> window_begin;   // Total time from the begin of the measure in nanoseconds

  std::vector<uint32_t> retrig_count;   // Vector of ATMD retrig counters
  std::vector<int32_t> stoptime;        // Vector of stoptimes in unit of Tbin
  std::vector<int8_t> channel;          // Vector of channel numbers

  double time_bin;                      // Time bin in ps
};


/* @class Measure
 * This class manages the starts of one measure
 */
class Measure {
public:
  Measure() {};
  ~Measure() {
    for(uint32_t i = 0; i < this->starts.size(); i++)
      delete starts[i];
    this->starts.clear();
  }; // The destructor should cycle over starts vector to free all the start objects

  // Make class VirtualBoard a friend
  friend class VirtualBoard;

  // Interface to add a start object
  int add_start(std::vector<StartData*>& starts);

  // Interface to clear all stored start objects
  void clear() {
    for(uint32_t i = 0; i < this->starts.size(); i++)
      delete starts[i];
    this->starts.clear();
    this->measure_begin.clear();
    this->measure_time.clear();
  };

  // Interface to count start events
  uint32_t count_starts() { return this->starts.size(); };

  // Interface to retrieve a start object
  StartData* get_start(uint32_t num_start) {
    if(num_start < this->starts.size())
      return this->starts[num_start];
    else
      return NULL;
  };

  // Interface for managing effective measure time
  void add_time(uint64_t begin, uint64_t duration) { measure_begin.push_back(begin); measure_time.push_back(duration); };
  size_t times()const { return measure_begin.size(); };
  uint64_t get_time(size_t i) { return measure_time[i]; };
  uint64_t get_begin(size_t i) { return measure_begin[i]; };

private:
  std::vector<uint64_t> measure_begin;  // Timestamp of measure start
  std::vector<uint64_t> measure_time;   // Duration of measure
  std::vector<StartData*> starts;       // Vector of pointers to start objects relative to this measure
};

#endif
