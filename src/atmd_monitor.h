/*
 * ATMD Server version 3.0
 *
 * ATMD Server - Monitor class header
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

#ifndef ATMD_MONITOR_H
#define ATMD_MONITOR_H

// Global
#include <stdint.h>
#include <string>
#include <vector>

// Local
#include "atmd_measure.h"

// Declare class VirtualBoard for friendship
class VirtualBoard;


/* @class Monitor
 *
 */
class Monitor {
public:
  Monitor() : _n(0), _m(0), _count(0) {};
  ~Monitor() { clear(); };

  // Make class VirtualBoard a friend
  friend class VirtualBoard;

  // Setup
  void setup(uint32_t n, uint32_t m) { _n = n; _m = m; };

  // Enabled?
  bool enabled()const { return (_m && _n); };

  // Add start
  void add_start(const std::vector<StartData*>& svec) {
    // Add new start object
    _data.push_back(StartData::merge(svec));

    // Check if circular buffer is full
    if(_data.size() > _m) {
      // Remove first element of vector
      delete _data.front();
      _data.erase(_data.begin());
    }
  };

  // Clear
  void clear() {
    for(size_t i = 0; i < _data.size(); i++)
      delete _data[i];
    _data.clear();
    _count = 0;
  };

  // Start count
  size_t start_count()const { return _data.size(); };

  // Get starts
  const StartData* get_start(size_t i)const { return _data[i]; };

private:

  // Vector of start objects
  std::vector<StartData*> _data;

  // Monitor parameters
  uint32_t _n; // Save _n starts
  uint32_t _m; // Save every _m starts

  // Counter
  size_t _count;
};

#endif
