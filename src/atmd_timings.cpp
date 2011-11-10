/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * atmd_timings.cpp
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

#include "atmd_timings.h"

/* @fn Timings::Timings(string timestr)
 * Timings class constructor
 *
 * @param timestr String containing the time to be saved in the object.
 */
Timings::Timings (string timestr) {
	this->raw = 0;
	this->useconds = 0;
	this->seconds = 0;
	set(timestr);
}


/* @fn Timings::set(string timestr)
 * Takes a time string, convert it and save it in the object.
 *
 * @param timestr String containing the time to be saved in the object.
 * @return Return 0 on success, -1 on error, i.e. when an invalid string is supplied.
 */
int Timings::set(string timestr) {
	char unit = 0;

	// Regular expression to match an integer value
	pcrecpp::RE re_integer("(\\d+)([umsMh]?)");
	// Regular expression to match a float value
	pcrecpp::RE re_float("((?:\\d+)(?:[\\.]{1})(?:\\d+))([umsMh]?)");

	if(re_integer.FullMatch(timestr)) {
		uint32_t value = 0;
		re_integer.FullMatch(timestr, &value, &unit);
		if(unit != 0) {
			compute_times(unit, value);
			this->raw = 0;
		} else {
			this->raw = value;
			this->seconds = 0;
			this->useconds = 0;
		}

	} else if(re_float.FullMatch(timestr)) {
		double value;
		re_float.FullMatch(timestr, &value, &unit);
		if(unit != 0) {
			compute_times(unit, value);
			this->raw = 0;
		} else {
			this->raw = (int) value;
			this->seconds = 0;
			this->useconds = 0;
		}

	} else {
		syslog(ATMD_ERR, "Time string error. String \"%s\" is not a valid time string.", timestr.c_str());
		return -1;
	}

	return 0;
}


/* @fn Timings::get()
 * Convert the saved time to a string using the more convenient time unit.
 *
 * @return Return a string with the saved time.
 */
string Timings::get() {
	char buffer[32];
	string output = "";

	if(this->raw != 0) {
		snprintf(buffer, 32, "%d", this->raw);

	} else {
		if(this->seconds == 0) {
			if(this->useconds > 1000) {
				double millisec = this->useconds / 1e3;
				snprintf(buffer, 32, "%fm", millisec);

			} else {
				snprintf(buffer, 32, "%du", this->useconds);
			}

		} else {
			if(this->seconds > 3600) {
				double hours = this->seconds / 3.6e3 + this->useconds / 3.6e9;
				snprintf(buffer, 32, "%fh", hours);

			} else if(this->seconds > 60) {
				double minutes = this->seconds / 60 + this->useconds / 6e7;
				snprintf(buffer, 32, "%fM", minutes);

			} else {
				double sec = this->seconds + this->useconds / 1e6;
				snprintf(buffer, 32, "%fs", sec);
			}
		}
	}
	output = buffer;
	return output;
}

/* @fn Timings::compute_times(char unit, int value)
 * Private utility function to convert a time in 2 values (seconds and microseconds).
 *
 * @param unit Time unit.
 * @param value Value to be converted.
 */
void Timings::compute_times(char unit, uint32_t value) {
	switch(unit) {
		case 'u': // Microseconds case
			this->useconds = value % 1000000;
			this->seconds = value / 1000000;
			break;

		case 'm': // Milliseconds case
			this->useconds = (value % 1000) * 1000;
			this->seconds = value / 1000;
			break;

		case 's': // Seconds case
			this->seconds = value;
			this->useconds = 0;
			break;

		case 'M': // Minutes case
			this->seconds = value * 60;
			this->useconds = 0;
			break;

		case 'h': // Hours case
			this->seconds = value * 3600;
			this->useconds = 0;
			break;

		default:
			// This case should never be reached because unit is validated by regex.
			break;
	}
}

/* @fn Timings::compute_times(char unit, double value)
 * Private utility function to convert a time in 2 values (seconds and microseconds).
 *
 * @param unit Time unit.
 * @param value Value to be converted.
 */
void Timings::compute_times(char unit, double value) {
	switch(unit) {
		case 'u': // Microseconds case
			this->useconds = (uint32_t) (value - floor(value / 1e6) * 1e6);
			this->seconds = (uint32_t) floor(value / 1e6);
			break;

		case 'm': // Milliseconds case
			this->useconds = (uint32_t) ((value - floor(value / 1e3) * 1e3) * 1e3);
			this->seconds = (uint32_t) floor(value / 1e3);
			break;

		case 's': // Seconds case
			this->seconds = (uint32_t) floor(value);
			this->useconds = (uint32_t) ((value - floor(value)) * 1e6);
			break;

		case 'M': // Minutes case
			this->seconds = (uint32_t) floor(value * 60);
			this->useconds = (uint32_t) (((value * 60) - floor(value * 60)) * 1e6);
			break;

		case 'h': // Hours case
			this->seconds = (uint32_t) floor(value * 3600);
			this->useconds = (uint32_t) (((value * 3600) - floor(value * 3600)) * 1e6);
			break;

		default:
			// This case should never be reached because unit is validated by regex.
			break;
	}
}
