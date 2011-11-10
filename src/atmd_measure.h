/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * atmd_measure.h
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

#ifndef ATMD_MEASURE_H
#define ATMD_MEASURE_H

#include "common.h"


class StopData {
	public:
	StopData(): stop_time(0), start_count(0), channel(0), slope(0) {};
	~StopData() {};

	// Interfaces for managing stop time
	void set_bins(int32_t time) { this->stop_time = time; };
	int32_t get_bins() { return this->stop_time; };

	// Interfaces for managing start count
	void set_startcount(uint32_t count) { this->start_count = count; };
	uint32_t get_startcount() { return this->start_count; };
	
	// Interfaces for managing the channel
	void set_channel(uint8_t channel) { this->channel = channel; };
	uint8_t get_channel() { return this->channel; };
	
	// Interfaces for managing slope
	void set_slope(bool slope) { this->slope = slope; };
	bool get_slope() { return this->slope; };

	private:
	// Hit time in units of tbin (depends on resolution)
	int32_t stop_time;

	// Start count in units of ATMD_TREF
	uint32_t start_count;

	// Stop channel
	uint8_t channel;

	// Slope
	bool slope;
};

class StartData {
	public:
	StartData(uint16_t def_size = 256): start01(0), elapsed_time(0), timefrombegin(0) { this->alloc_size = def_size; this->stops.reserve(this->alloc_size); };
	~StartData() { this->stops.clear(); }; // The destructor should clear the stops vector to release the memory allocated

	int add_stop(StopData& stop);
	uint32_t count_stops() { return this->stops.size(); };
	StopData get_stop(uint32_t num_stop) { return this->stops[num_stop]; } // TODO: this should include some error checking!
	void clear() { this->stops.clear(); this->start01 = 0; this->elapsed_time = 0; this->timefrombegin = 0; };

	// Interface to set start01
	void set_start01(uint32_t value) { this->start01 = value; };
	uint32_t get_start01() { return this->start01; };

	// Interface for managing effective window time
	void set_time(struct timeval& begin, struct timeval& end);
	uint64_t get_time() { return this->elapsed_time; };

	// Interface for managing time from the begin of measure
	void set_timefrombegin(struct timeval& begin, struct timeval& end);
	uint64_t get_timefrombegin() { return this->timefrombegin; };

	private:
	// Total window time in microseconds
	uint64_t elapsed_time;

	// Time from the begin of the measure in microseconds
	uint64_t timefrombegin;

	// Default allocation size for start
	uint16_t alloc_size;

	// Value of start01 in tbin
	uint32_t start01;

	vector<StopData> stops;
};

class Measure {
	public:
	Measure(): incomplete(false), time_bin(0.0) {this->elapsed_time = 0; };
	~Measure() { this->starts.clear(); }; // The destructor should clear the starts vector to release the memory allocated

	// Interfaces for managing start objects
	int add_start(StartData& start);
	void clear() { this->starts.clear(); this->incomplete = false; this->time_bin = 0; this->elapsed_time = 0; };
	uint32_t count_starts() { return this->starts.size(); };
	StartData get_start(uint32_t num_start) { return this->starts[num_start]; } // TODO: this should include some error checking!

	// Interface for managing effective measure time
	void set_time(struct timeval& begin, struct timeval& end);
	uint64_t get_time() { return this->elapsed_time; };

	void set_incomplete(bool value) { this->incomplete = value; };
	bool is_incomplete() { return this->incomplete; };

	// Interface for time_bin
	void set_tbin(double tbin) { this->time_bin = tbin; };
	double get_tbin() { return this->time_bin; };

	private:
	// Total measure time
	uint64_t elapsed_time;

	// Incomplete flags
	bool incomplete;

	// Time bin e start count
	double time_bin;

	// Vector of start objects relative to this measure
	vector<StartData> starts;
};

#endif
