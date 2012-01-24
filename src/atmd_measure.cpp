/*
 * ATMD Server version 3.0
 *
 * ATMD Server - Measure handling class
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

#include "atmd_measure.h"


/* @fn StartData::add_event(uint32_t retrig, uint32_t stop, int8_t ch)
 * Add a stop to a StartData object.
 *
 * @param retrig The retring count of the event to add
 * @param stop The stop count of the event to add
 * @param ch The channel of the event to add
 * @return Return 0 on success, -1 on error.
 */
int StartData::add_event(uint32_t retrig, int32_t stop, int8_t ch) {
  this->retrig_count.push_back(retrig);
  this->stoptime.push_back(stop);
  this->channel.push_back(ch);
  return 0;
};


/* @fn StartData::reserve(size_t sz)
 * Reserve memory for 'sz' events
 *
 * @param sz The number of objects to preallocate
 */
void StartData::reserve(size_t sz) {
  try {
    this->retrig_count.reserve(sz);
    this->stoptime.reserve(sz);
    this->channel.reserve(sz);
  } catch(std::exception& e) {
    syslog(ATMD_ERR, "Measure [add_event]: memory allocation failed with error %s", e.what());
  }
}


/* @fn StartData::get_event(uint32_t num, uint32_t& retrig, uint32_t& stop, int8_t& ch)
 * Add a stop to a StartData object.
 *
 * @param num The number of the stop we want to retrieve (starting from zero)
 * @param retring A reference to an output var for the retring count
 * @param stop A reference to an output var for the stoptime
 * @param ch A reference to an output var for the channel
 * @return Return 0 on success, -1 on error.
 */
int StartData::get_event(uint32_t num, uint32_t& retrig, int32_t& stop, int8_t& ch)const {
  if(num < this->retrig_count.size()) {
    retrig = this->retrig_count[num];
    stop = this->stoptime[num];
    ch = this->channel[num];
  } else {
    return -1;
  }
  return 0;
}


/* @fn StartData::get_channel(uint32_t num, int8_t& ch)
 * Get the channel number of the numth stop of the start.
 *
 * @param num The number of the stop we want to retrieve (starting from zero)
 * @param ch A reference to an output var for the channel
 * @return Return 0 on success, -1 on error.
 */
int StartData::get_channel(uint32_t num, int8_t& ch)const {
  if(num < this->channel.size())
    ch = this->channel[num];
  else
    return -1;
  return 0;
}


/* @fn StartData::get_stoptime(uint32_t num, double& stop)
 * Get the stoptime in ps of the numth stop of the start.
 *
 * @param num The number of the stop we want to retrieve (starting from zero)
 * @param stop A reference to an output var for the stoptime in ps (double precision)
 * @return Return 0 on success, -1 on error.
 */
int StartData::get_stoptime(uint32_t num, double& stop)const {
  if(num < this->retrig_count.size()) {
    stop = (double)(this->stoptime[num]) * this->time_bin + (double)(this->retrig_count[num]) * (ATMD_AUTORETRIG + 1) * ATMD_TREF * 1e12;

  } else {
    // Non-existent stop, so we return -1
    return -1;
  }

  return 0;
}


/* @fn StartData::get_rawstoptime(uint32_t num, uint32_t& retrig, double& stop)
 * Get the raw stoptime in retrig plus ps of the numth stop of the start.
 *
 * @param num The number of the stop we want to retrieve (starting from zero)
 * @param retrig A reference to an output var for the retrig count
 * @param stop A reference to an output var for the stoptime in ps (double precision)
 * @return Return 0 on success, -1 on error.
 */
int StartData::get_rawstoptime(uint32_t num, uint32_t& retrig, double& stop)const {
  if(num < this->retrig_count.size()) {
    stop = (double)this->stoptime[num] * this->time_bin;
    retrig = this->retrig_count[num];

  } else {
    // Non-existent stop, so we return -1
    return -1;
  }

  return 0;
}


/* @fn Measure::add_start(const StartData& start)
 * Get a vector of starts from the agents, merges them into a single start object
 * and adds it to the Measure object.
 *
 * @param start A constant reference to a StartData object.
 * @return Return 0 on success, -1 on error.
 */
int Measure::add_start(std::vector<StartData*>& svec) {
  // Create new StartData object
  StartData * merged = new StartData;

  // Sum up all events
  size_t numev = 0;
  for(size_t i = 0; i < svec.size(); i++)
    numev += svec[i]->count_stops();

  // Allocate memory
  merged->reserve(numev);

  // Merge events
  for(size_t i = 0; i < svec.size(); i++) {
    for(size_t j = 0; j < svec[i]->count_stops(); j++) {
      int8_t ch = 0;
      int32_t stop = 0;
      uint32_t retrig = 0;
      svec[i]->get_event(j, retrig, stop, ch);
      merged->add_event(retrig, stop, ch);
    }

    // Window times
    if(svec[i]->times())
      merged->add_time(svec[i]->get_window_begin(0), svec[i]->get_window_time(0));
    else
      syslog(ATMD_ERR, "Measure [add_start]: StartData was missing the window start and duration.");
  }

  // Add tbin
  merged->set_tbin(svec[0]->get_tbin());

  // Add start to measure
  try {
    this->starts.push_back(merged);
    return 0;

  } catch (std::exception& e) {
    syslog(ATMD_ERR, "Measure [add_start]: memory allocation failed with error %s", e.what());
    return -1;
  }
}
