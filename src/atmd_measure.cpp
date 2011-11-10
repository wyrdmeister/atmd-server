/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * atmd_measure.cpp
 * Copyright (C) Michele Devetta 2009 <michele.devetta@unimi.it>
 * 
 * main.cc is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * main.cc is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "atmd_measure.h"


/* @fn StartData::add_stop(const StopData& stop)
 * Add a stop to a StartData object.
 *
 * @param stop A constant reference to a StopData object.
 * @return Return 0 on success, -1 on error.
 */
int StartData::add_stop(StopData& stop) {
	// If the vector is full, we try to allocate more space. We do not use the automatic
	// allocation on insertion because it can be slow to allocate space for every single stop
	if(this->stops.capacity() <= this->stops.size()) {
		try {
			this->stops.reserve(this->stops.capacity() + this->alloc_size);
		} catch (exception& e) {
			syslog(ATMD_ERR, "Measure [add_stop]: memory allocation failed with error %s", e.what());
			return -1;
		}
	}
	this->stops.push_back(stop);
	return 0;
}


/* @fn Measure::add_start(const StartData& start)
 * Add a start to a Measure object.
 *
 * @param start A constant reference to a StartData object.
 * @return Return 0 on success, -1 on error.
 */
int Measure::add_start(StartData& start) {
	try {
		this->starts.push_back(start);
		return 0;
	} catch (exception& e) {
		syslog(ATMD_ERR, "Measure [add_start]: memory allocation failed with error %s", e.what());
		return -1;
	}
}


/* @fn StartData::set_time(struct timeval* begin, struct timeval* end)
 * Compute the window time.
 *
 * @param begin Pointer to the time structure with the begin time.
 * @param end Pointer to the time structure with the end time.
 */
void StartData::set_time(struct timeval& begin, struct timeval& end) {
	this->elapsed_time = (uint64_t)(end.tv_sec - begin.tv_sec) * 1000000 + (end.tv_usec - begin.tv_usec);
};


/* @fn StartData::set_timefrombegin(struct timeval* begin, struct timeval* end)
 * Compute the time of start from the beginngin of the measure.
 *
 * @param begin Pointer to the time structure with the begin time.
 * @param end Pointer to the time structure with the end time.
 */
void StartData::set_timefrombegin(struct timeval& begin, struct timeval& end) {
	this->timefrombegin = (uint64_t)(end.tv_sec - begin.tv_sec) * 1000000 + (end.tv_usec - begin.tv_usec);
};


/* @fn Measure::set_time(struct timeval* begin, struct timeval* end)
 * Compute the measure time.
 *
 * @param begin Pointer to the time structure with the begin time.
 * @param end Pointer to the time structure with the end time.
 */
void Measure::set_time(struct timeval& begin, struct timeval& end) {
	this->elapsed_time = (uint64_t)(end.tv_sec - begin.tv_sec) * 1000000 + (end.tv_usec - begin.tv_usec);
};
