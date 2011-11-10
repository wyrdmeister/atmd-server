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

#ifndef ATMD_MEASURE_H
#define ATMD_MEASURE_H

#include "common.h"

class StartData {
	public:
		StartData(): elapsed_time(0), timefrombegin(0), start01(0), time_bin(0.0), _bnumber(0) {};
		~StartData() {};

		int add_event(uint32_t retrig, int32_t stop, int8_t ch);
		int get_event(uint32_t num, uint32_t& retrig, int32_t& stop, int8_t& ch);
		int get_channel(uint32_t num, int8_t& ch);
		int get_stoptime(uint32_t num, double& stop);
		int get_rawstoptime(uint32_t num, uint32_t& retrig, double& stop);
		uint32_t count_stops() { return this->retrig_count.size(); };

		void clear() {
			this->retrig_count.clear();
			this->stoptime.clear();
			this->channel.clear();
			this->start01 = 0;
			this->elapsed_time = 0;
			this->timefrombegin = 0;
			this->time_bin = 0.0;
		};

		// Interface to set start01
		void set_start01(uint32_t value) { this->start01 = value; };
		uint32_t get_start01() { return this->start01; };

		// Interface for managing effective window time
		void set_time(RTIME begin, RTIME end) { this->elapsed_time =  (uint64_t)(end - begin) / 1000;};
		uint64_t get_time() { return this->elapsed_time; };

		// Interface for managing time from the begin of measure
		void set_timefrombegin(RTIME begin, RTIME end) { this->timefrombegin = (uint64_t)(end - begin) / 1000; };
		uint64_t get_timefrombegin() { return this->timefrombegin; };

		// Interface to manage time_bin
		void set_tbin(double tbin) { this->time_bin = tbin; };
		double get_tbin() { return this->time_bin; };

		// Preallocate
		void reserve(size_t sz);

		// Bunch number
		void set_bnum(uint32_t val, uint64_t ts) { _bnumber = val; _bnumber_ts = ts; };
		uint32_t get_bnum() { return _bnumber; };
		uint64_t get_bnum_ts() { return _bnumber_ts; };

	private:
		uint64_t elapsed_time;         /* Effective window time in microseconds */
		uint64_t timefrombegin;        /* Total time from the begin of the measure in microseconds */

		uint32_t start01;              /* Start01 in unit of Tbin for current start */

		vector<uint32_t> retrig_count; /* Vector of ATMD retrig counters */
		vector<int32_t> stoptime;      /* Vector of stoptimes in unit of Tbin */
		vector<int8_t> channel;        /* Vector of channel numbers */

		double time_bin;               /* Time bin in ps */

		uint32_t _bnumber;             /* Bunch number */
		uint64_t _bnumber_ts;          /* Bunch number timestamp in us */
};

class Measure {
	public:
		Measure(): elapsed_time(0), measure_begin(0), measure_end(0), incomplete(false), bnum_enabled(false) {};
		~Measure() {
			for(uint32_t i = 0; i < this->starts.size(); i++)
				delete starts[i];
			this->starts.clear();
		}; // The destructor should cycle over starts vector to free all the start objects

		// Interface to add a start object
		int add_start(StartData* start);

		// Interface to clear all stored start objects
		void clear() {
			for(uint32_t i = 0; i < this->starts.size(); i++)
				delete (starts[i]);
			this->starts.clear();
			this->incomplete = false;
			this->elapsed_time = 0;
			this->measure_begin = 0;
			this->measure_end = 0;
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
		void set_time(RTIME begin, RTIME end);
		uint64_t get_time() { return this->elapsed_time; };
		RTIME get_begin() { return this->measure_begin; };
		RTIME get_end() { return this->measure_end; };

		// Interface to manage incomplete flag
		void set_incomplete(bool value) { this->incomplete = value; };
		bool is_incomplete() { return this->incomplete; };

		// Interface for bunch number flag
		bool bnum_en(bool en) { return (this->bnum_enabled = en); };
		bool bnum_en() { return this->bnum_enabled; };

	private:
		uint64_t elapsed_time;        /* Total measure time */
		RTIME measure_begin;          /* Timestamp of measure start */
		RTIME measure_end;            /* Timestamp of measure end */
		bool incomplete;              /* Incomplete flags */
		vector<StartData*> starts;    /* Vector of pointers to start objects relative to this measure */
		bool bnum_enabled;            /* Flag for bunch number */
};

#endif
