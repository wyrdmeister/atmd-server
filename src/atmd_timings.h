/*
 * ATMD Server version 3.0
 *
 * ATMD Server - Time string handling class header
 *
 * Copyright (C) Michele Devetta 2011 <michele.devetta@unimi.it>
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

#ifndef ATMD_TIMINGS_H
#define ATMD_TIMINGS_H

// Global
#include <stdio.h>
#include <stdint.h>
#include <string>
#include <pcrecpp.h>
#include <math.h>

// Local
#include "common.h"


/* @class Timings
 *
 */
class Timings {
public:
  // Constructors and destructor
  Timings() : seconds(0), useconds(0) {};
  Timings(std::string timestr) : seconds(0), useconds(0) { set(timestr); };
  ~Timings() {};

  // Take a string with time units and convert it to usable numbers
  // Use "u" for microseconds, "m" for milliseconds, "s" for seconds,
  // "M" for minutes, "h" for hours
  int set(std::string timestr);

  // Return the saved time in string using a convenient time unit
  std::string get()const;

  // Tell if the saved time is zero
  bool is_zero()const { return ( (this->get_sec() + this->get_usec()) == 0); };

  // Return number of microseconds (always less than 1e6)
  uint32_t get_usec()const { return this->useconds; };

  // Return number of seconds
  uint32_t get_sec()const { return this->seconds; };

  // Return as RTIME in ns
  uint64_t get_nsec()const { return ((uint64_t)seconds * 1000000000 + (uint64_t)useconds * 1000); };

  // Return the time in ps
  double get_ps()const { return ((double)this->seconds * 1e12 + (double)this->useconds * 1e6); };

private:
  uint32_t seconds;
  uint32_t useconds;

  // Functions to compute times based on time units
  void compute_times(char unit, uint32_t value);
  void compute_times(char unit, double value);
};

#endif
