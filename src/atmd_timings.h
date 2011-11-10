/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * atmd_timings.h
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

#ifndef ATMD_TIMINGS_H
#define ATMD_TIMINGS_H

#include "common.h"

class Timings {
	public:
		// Constructors and destructor
		Timings() : seconds(0), useconds(0), raw(0) {};
		Timings(string timestr);
		~Timings() {};

		// Take a string with time units and convert it to usable numbers
		// Use "u" for microseconds, "m" for milliseconds, "s" for seconds,
		// "M" for minutes, "h" for hours
		int set(string timestr);

		// Return the saved time in string using a convenient time unit
		string get();

		// Tell if the supplied data was given without specifing time units
		bool is_raw() { return (this->raw != 0); };

		// Tell if the saved time is zero
		bool is_zero() { return ( (this->get_sec() + this->get_usec() + this->get_raw()) == 0); };

		// Return number of microseconds (always less than 1e6)
		uint32_t get_usec() { return this->useconds; };

		// Return number of seconds
		uint32_t get_sec() { return this->seconds; };

		// Return the raw number
		uint32_t get_raw() { return this->raw; };

		// Return the time in ps
		double get_ps() { return ((double)this->seconds * 1e12 + (double)this->useconds * 1e6); };

	private:
		uint32_t seconds;
		uint32_t useconds;
		uint32_t raw;

		// Functions to compute times based on time units
		void compute_times(char unit, uint32_t value);
		void compute_times(char unit, double value);
};

#endif
