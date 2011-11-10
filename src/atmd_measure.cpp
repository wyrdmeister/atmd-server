/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
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
	} catch (exception& e) {
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
int StartData::get_event(uint32_t num, uint32_t& retrig, int32_t& stop, int8_t& ch) {
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
int StartData::get_channel(uint32_t num, int8_t& ch) {
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
int StartData::get_stoptime(uint32_t num, double& stop) {
	if(num < this->retrig_count.size()) {
		if(this->retrig_count[num] > 0) {
			stop = (double)(this->stoptime[num] + this->start01) * this->time_bin + (double)(this->retrig_count[num] - 1) * (ATMD_AUTORETRIG + 1) * ATMD_TREF * 1e12;
		} else {
			stop = (double)this->stoptime[num] * this->time_bin;
		}

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
int StartData::get_rawstoptime(uint32_t num, uint32_t& retrig, double& stop) {
	if(num < this->retrig_count.size()) {
		if(this->retrig_count[num] > 0) {
			stop = (double)(this->stoptime[num] + this->start01) * this->time_bin;
			retrig = this->retrig_count[num] - 1;
		} else {
			stop = (double)this->stoptime[num] * this->time_bin;
			retrig = 0;
		}

	} else {
		// Non-existent stop, so we return -1
		return -1;
	}
	return 0;
}


/* @fn Measure::add_start(const StartData& start)
 * Add a start to a Measure object.
 *
 * @param start A constant reference to a StartData object.
 * @return Return 0 on success, -1 on error.
 */
int Measure::add_start(StartData* start) {
	try {
		this->starts.push_back(start);
		return 0;

	} catch (exception& e) {
		syslog(ATMD_ERR, "Measure [add_start]: memory allocation failed with error %s", e.what());
		return -1;
	}
}


/* @fn Measure::set_time(struct timeval* begin, struct timeval* end)
 * Compute the measure time.
 *
 * @param begin Pointer to the time structure with the begin time.
 * @param end Pointer to the time structure with the end time.
 */
void Measure::set_time(RTIME begin, RTIME end) {
	this->measure_begin = begin;
	this->measure_end = end;
	if(begin > end) {
		syslog(ATMD_ERR, "Measure [set_time]: end time is before begin time.");
		this->elapsed_time = 0;
	} else {
		this->elapsed_time = (uint64_t)(end - begin) / 1000;
	}
};
