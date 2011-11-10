/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * atmd_measure_routines.cpp
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

#include "atmd_hardware.h"

// External global debug flag
extern bool enable_debug;


/* @fn burst_measure(void *arg)
 * Pefrorm a burst measure. This should be run inside a thread.
 *
 * @param arg The pointer to the board object casted to a void pointer.
 * @return Return a void pointer to the return value (should be freed after the end of the thread).
 */
void* burst_measure(void* arg) {
	// Reconstruct the board object
	ATMDboard *board = (ATMDboard *)arg;

	// Get the measure timings
	Timings measure_window(board->get_window());
	Timings measure_time(board->get_tottime());

	// Pointer for the return value
	int *retval = new int(ATMD_ERR_NONE);

	// Set the measure_running flag
	board->set_status(ATMD_STATUS_RUNNING);

	// Just to be sure we reset the stop flag
	board->set_stop(false);

	// We sleep for some time to let the main thread answer to the client.
	struct timespec sleeptime;
	sleeptime.tv_sec = 0;
	sleeptime.tv_nsec = 1000000;
	nanosleep(&sleeptime, NULL);
	
	// Set the 10us sleeptime
	sleeptime.tv_sec = 0;
	sleeptime.tv_nsec = 10000;

	// Measurement utility variables
	uint16_t mbs = 0x0000; // Motherboard status register
	bool finish_measure = false; // Measure finished flag
	bool finish_window = false; // Window finished flag
	bool start_received = false; // True when a start is recieved
	uint32_t measure_start_counter = 0; // Counter for start events. Useful if measure time is a raw value
	uint32_t data = 0; // Data read from fifos
	uint32_t start01 = 0; // Start01

	// Allocate objects to hold measurement data
	Measure current_measure;
	current_measure.set_tbin(board->get_timebin());
	StartData current_start;
	StopData current_stop;

	// We get measure start time
	bool first_start = true;
	struct timeval measure_begin, measure_end, window_start, window_end;
	if(gettimeofday(&measure_begin, NULL))
		syslog(ATMD_ERR, "Measure [Burst mode]: gettimeofday failed with error \"%m\"");
	if(gettimeofday(&measure_end, NULL))
		syslog(ATMD_ERR, "Measure [Burst mode]: gettimeofday failed with error \"%m\"");
	if(gettimeofday(&window_start, NULL))
		syslog(ATMD_ERR, "Measure [Burst mode]: gettimeofday failed with error \"%m\"");
	if(gettimeofday(&window_end, NULL))
		syslog(ATMD_ERR, "Measure [Burst mode]: gettimeofday failed with error \"%m\"");

	// We need to decide after how many starts we check the window time
	int check_step;
	int check_index = 0;
	if(measure_window.get_usec() > 200)
		check_step = 5;
	else
		check_step = 1;
	if(enable_debug) 
		check_step = 1;

	// Begin measurement. We use exceptions to catch errors
	try {

		do {
			// Start the measuring window

			// Make a TDC-GPX master reset
			board->master_reset();

			// We enable the inputs and burst mode
			board->mb_config(0x0202);

			// We reset the window finish flag and the start recieved flag 
			finish_window = false;
			start_received = false;

			// Clear data from previous start
			current_start.clear();

			// We wait for a start pulse (through the intflag and mtimer)
			do {

				// We update the measure_end structure
				if(!measure_time.is_raw())
					if(gettimeofday(&measure_end, NULL))
						syslog(ATMD_ERR, "Measure [Burst mode]: gettimeofday failed with error \"%m\"");

				// Check if the measure time (or number of requested start events) is exceeded
				if(measure_time.is_raw()) {
					if(measure_start_counter >= measure_time.get_raw()) {
						finish_measure = true;
						break;
					}
				} else {
					if(board->check_measuretime_exceeded(measure_begin, measure_end)) {
						finish_measure = true;
						break;
					}
				}

				// Read motherboard status register
				mbs = board->mb_status();

				if(mbs & 0x0020) {
					// Start received. We set the flag
					start_received = true;

					// If this is the first start we resynchronize the begin with this start
					if(first_start) {
						first_start = false;
						if(gettimeofday(&measure_begin, NULL))
							syslog(ATMD_ERR, "Measure [Burst mode]: gettimeofday failed with error \"%m\"");
					}

					// We save the begin time of the measure window
					if(gettimeofday(&window_start, NULL))
						syslog(ATMD_ERR, "Measure [Burst mode]: gettimeofday failed with error \"%m\"");

					// We increment the start counter
					measure_start_counter++;

					if(enable_debug)
						syslog(ATMD_DEBUG, "Measure [Burst mode]: received start %d.", measure_start_counter);
				}

			} while(!start_received && !board->get_stop());

			// If we exited the previous cycle because measure is terminating or time exceed we stop here.
			if(finish_measure || board->get_stop())
				break;

			// Reset counter of stop acquisitions
			check_index = 0;

			// Start data acquisition
			do {
				// Read motherboard status
				mbs = board->mb_status();

				check_index++;

				// When we recieve the stop command, we set the finish_window flag
				if(board->get_stop())
					finish_window = true;

				// If the finish window flag is set we disable the inputs
				if(finish_window) {
					board->mb_config(0x020D);
					current_start.set_time(window_start, window_end);
				}

				// Check if the fifos contains some data
				if(mbs & 0x0101) {
					data = board->mb_readfifo();

					// We check if the value we are reading is start01
					if(!(data & 0x80000000)) {
						// Start01
						start01 = (data & 0x0001FFFF);

					} else {
						// We get the start count
						current_stop.set_startcount(((data & 0x03FC0000) >> 18));

						// We get the number of bins
						current_stop.set_bins((data & 0x0001FFFF) - board->get_start_offset());

						// We get the channel number
						if(data & 0x40000000)
							current_stop.set_channel(((data & 0x0A000000) >> 26) + 5);
						else
							current_stop.set_channel(((data & 0x0A000000) >> 26) + 1);

						// We get the slope
						current_stop.set_slope((data & 0x00020000) >> 17);

						// Add stop to current start
						if(current_start.add_stop(current_stop)) {
							syslog(ATMD_ERR, "Measure [Burst mode]: error saving stops of start %d.", measure_start_counter);
							throw(ATMD_ERR_ALLOC);
						}
					}

				} else {
					if(finish_window)
						break;
				}

				// If it's the time to check the time we do it!
				if((check_index % check_step) == 0) {
					if(gettimeofday(&window_end, NULL))
						syslog(ATMD_ERR, "Measure [Burst mode]: gettimeofday failed with error \"%m\"");
					if(board->check_windowtime_exceeded(window_start, window_end))
						finish_window = true;
				}
			} while (true);

			// At this point start01 should be set. If not, we throw an exception (something went wrong in the measure)
			if(start01 == 0) {
				syslog(ATMD_ERR, "Measure [burst mode]: start01 was not set in start %d. Aborting.", measure_start_counter);
				//throw(ATMD_ERR_START01);
			}

			// We save the start01
			current_start.set_start01(start01);

			// Add current start to current measure_finished
			if(current_measure.add_start(current_start)) {
				syslog(ATMD_ERR, "Measure [Burst mode]: error saving start %d", measure_start_counter);
				throw(ATMD_ERR_ALLOC);
			};

			if(enable_debug)
				syslog(ATMD_DEBUG, "Measure [Burst mode]: added start %d with %d stops.", measure_start_counter, current_start.count_stops());

			// When we finish the window we disable inputs and burst mode
			board->mb_config(0x0008);

			if(board->get_stop() || finish_measure)
				break;

			// We sleep for a while to let the main thread process some network command.
			nanosleep(&sleeptime, NULL);

		} while(true);

	} catch(int e) {
		current_measure.set_incomplete(true);
		board->set_status(ATMD_STATUS_ERROR);
		board->reset();
		*retval = e;
	} catch(exception& e) {
		syslog(ATMD_ERR, "Measure [Burst mode]: caught an unexpected exception (\"%s\").", e.what());
		current_measure.set_incomplete(true);
		board->set_status(ATMD_STATUS_ERROR);
		board->reset();
		*retval = ATMD_ERR_UNKNOWN_EX;
	} catch(...) {
		syslog(ATMD_ERR, "Measure [Burst mode]: caught an unknown exception.");
		current_measure.set_incomplete(true);
		board->set_status(ATMD_STATUS_ERROR);
		board->reset();
		*retval = ATMD_ERR_UNKNOWN_EX;
	}

	// We get measure end time and save total elapsed time
	if(gettimeofday(&measure_end, NULL))
		syslog(ATMD_ERR, "Measure [Burst mode]: gettimeofday failed with error \"%m\"");
	current_measure.set_time(measure_begin, measure_end);
	if(board->get_stop())
		syslog(ATMD_INFO, "Measure [Burst mode]: measure stopped. Got %d start events. Elapsed time is %lds and %ldus.", current_measure.count_starts(), measure_end.tv_sec-measure_begin.tv_sec, measure_end.tv_usec-measure_begin.tv_usec);
	else
		syslog(ATMD_INFO, "Measure [Burst mode]: measure finished. Got %d start events. Elapsed time is %lds and %ldus.", current_measure.count_starts(), measure_end.tv_sec-measure_begin.tv_sec, measure_end.tv_usec-measure_begin.tv_usec);

	// Save the current measure
	if(current_measure.count_starts() > 0) {
		if(board->add_measure(current_measure)) {
			syslog(ATMD_ERR, "Measure [Burst mode]: error adding measure to measure vector.");
			board->set_status(ATMD_STATUS_ERROR);
			*retval = ATMD_ERR_EMPTY; // Here we return ATMD_ERR_EMPTY to know that there's no measure saved
		}
	} else {
		syslog(ATMD_WARN, "Measure [Burst mode]: discarding an empty measurement.");
		board->set_status(ATMD_STATUS_ERROR);
		*retval = ATMD_ERR_EMPTY;
	}

	if(board->get_status() != ATMD_STATUS_ERROR)
		// Setting board status to ATMD_STATUS_FINISHED
		board->set_status(ATMD_STATUS_FINISHED);

	pthread_exit((void*) retval);
}
