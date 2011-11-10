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

#include "atmd_hardware.h"

// Define struct to handle data in real time measurement task
struct rt_event {
	uint32_t retrig_count; /* Counter of ATMD retrigs */
	uint32_t stoptime;     /* Stop times in unit of Tbin */
	int8_t ch;            /* Stop channel */
};

struct rt_data {
	// Board object
	ATMDboard* board;

	// Event buffer
	struct rt_event * events;

	// Measure data
	uint32_t start01;
	uint32_t num_events;
	bool good_start;
	bool first_start;

	// Measure timings
	RTIME measure_begin;
	RTIME measure_end;

	// Window timings
	RTIME window_begin;
	RTIME window_end;
};
