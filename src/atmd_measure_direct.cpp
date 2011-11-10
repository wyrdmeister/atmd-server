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


/* @fn direct_measure(void *arg)
 * Pefrorm a direct read measure. This should be run inside a thread.
 *
 * @param arg The pointer to the board object casted to a void pointer.
 * @return Return a void pointer to the return value (should be freed after the end of the thread).
 */
void* direct_measure(void* arg) {
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

	// Check if we need to read both FIFOs or only one
	uint8_t en_channel = board->get_rising_mask() | board->get_falling_mask();
	bool en_fifo0 = (en_channel & 0x0F);
	bool en_fifo1 = (en_channel & 0xF0);

	// Measurement utility variables
	bool finish_measure = false; // Measure finished flag
	bool finish_window = false;  // True when the window is finished
	bool start_received = false; // True when we recieve a start
	uint32_t measure_start_counter = 0; // Counter for start events. Useful if measure time is a raw value

	uint16_t mbs = 0x0000; // Motherboard status register
	uint32_t dra_data = 0x00000000; // Reg12 -> Flag for the end of mtimer

	// Allocate objects to hold measurement data
	Measure* current_measure = new Measure();
	current_measure->clear();
	StartData* current_start;

	// Time measuring structures
	struct timeval measure_begin, measure_end, window_start, window_end;
	bool first_start = true;
	if(gettimeofday(&measure_begin, NULL))
		syslog(ATMD_ERR, "Measure [Direct read mode]: gettimeofday failed with error \"%m\"");

	// We initialize the other timeval structures with memcpy that is faster!
	memcpy(&measure_end, &measure_begin, sizeof(struct timeval));
	memcpy(&window_start, &measure_begin, sizeof(struct timeval));
	memcpy(&window_end, &measure_begin, sizeof(struct timeval));

	// We need to decide after how many starts we check the measure time
	// TODO: this should be checked carefully!
	int check_step;
	int check_index = 0;
	if(measure_window.get_sec() > 0)
		check_step = 10;
	else if(measure_window.get_usec() > 1000)
		check_step = 5;
	else
		check_step = 1;

	// Begin measurement. We use exceptions to catch errors
	try {

		do {
			// Start the measuring window

			// Make a TDC-GPX master reset
			board->master_reset();

			// We enable the inputs
			board->mb_config(0x0000);

			// We reset the window finish flag and the start recieved flag
			finish_window = false;
			start_received = false;

			// Allocate a new start object
			current_start = new StartData;
			current_start->set_tbin(board->get_timebin());

			// We set dra address to 0x000C to read reg12 and detect end of mtimer
			board->set_dra(0x000C);

			// We wait for a start pulse (through mtimer flag in reg12 of TDC-GPX)
			do {

				// We update the measure_end structure
				if(!measure_time.is_raw())
					if(gettimeofday(&measure_end, NULL))
						syslog(ATMD_ERR, "Measure [Direct read mode]: gettimeofday failed with error \"%m\"");

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

				// Read the reg12 of TDC-GPX
				dra_data = board->read_dra();

				if(dra_data & 0x00001000) {
					// Start received. We set the flag
					start_received = true;

					// If this is the first start we resynchronize the begin with this start
					if(first_start) {
						first_start = false;
						if(gettimeofday(&measure_begin, NULL))
							syslog(ATMD_ERR, "Measure [Direct read mode]: gettimeofday failed with error \"%m\"");
					}

					// We save the begin time of the measure window
					if(gettimeofday(&window_start, NULL))
						syslog(ATMD_ERR, "Measure [Direct read mode]: gettimeofday failed with error \"%m\"");

					// We increment the start counter
					measure_start_counter++;

					if(enable_debug)
						syslog(ATMD_DEBUG, "Measure [Direct read mode]: received start %d.", measure_start_counter);
				}

			} while(!start_received && !board->get_stop());

			// If we exited the previous cycle for a stop or time exceed we stop here.
			if(finish_measure || board->get_stop())
				break;

			// Flags to detect overflow of internal start counter
			bool act_intflag = false;
			bool prv_intflag = false;

			// Flags for retriggering the start counter for FIFO0 and FIFO1
			bool main_retrig0 = false;
			bool main_retrig1 = false;

			// Counters for external start retriggering
			uint32_t main_startcounter0 = 0;
			uint32_t main_startcounter1 = 0;

			// Stop flags for the two fifos
			bool stop_fifo0 = false;
			bool stop_fifo1 = false;

			// Start count
			uint16_t start_count = 0;

			// Reset counter of stop acquisition
			check_index = 0;

			// Start data acquisition
			do {
				// Read motherboard status
				mbs = board->mb_status();

				// Get interrupt flag
				act_intflag = (bool)(mbs & 0x0020);

				if(prv_intflag && !act_intflag) {
					// Intflag changed from 1 to 0 -> Overflow!
					main_retrig0 = true;
					main_retrig1 = true;
				}
				prv_intflag = act_intflag;

				check_index++;

				// When we recieve the stop command, we set the finish_window flag
				if(board->get_stop())
					finish_window = true;

				// If the finish window flag is set we disable the inputs
				if(finish_window) {
					board->mb_config(0x0008);
					current_start->set_time(window_start, window_end);
					current_start->set_timefrombegin(measure_begin, window_start);
					if(enable_debug)
						syslog(ATMD_DEBUG, "Measure [Direct read mode]: start received %lu microseconds after the begin of the measure.", current_start->get_timefrombegin());
				}

				if(en_fifo0) {
					// Read TDC-GPX FIFO0
					if( !(mbs & 0x0800) ) { // FIFO0 not empty
						board->set_dra(0x0008);
						dra_data = board->read_dra();

						// Get the start count from data
						start_count = (uint16_t)((dra_data & 0x03FC0000) >> 18);

						// If main_retrig0 flag is set and start_count < 128, the counter has resetted. So we need
						// to update main_startcounter0.
						if(start_count < 128 && main_retrig0) {
							main_startcounter0++;
							main_retrig0 = false;
						}

						// Saving stop data
						int8_t ch = (int8_t)( ((dra_data & 0x0C000000) >> 26) + 1);
						if(((dra_data & 0x00020000) >> 17) == 0) {
							ch = -ch;
						}
						if(current_start->add_event( (uint32_t)( start_count + (main_startcounter0 * 256) ), (uint32_t)( (dra_data & 0x0001FFFF) - board->get_start_offset() ), ch)) {
							syslog(ATMD_ERR, "Measure [Direct read mode]: error saving stops of start %d.", measure_start_counter);
							throw(ATMD_ERR_ALLOC);
						}

					} else {
						if(main_retrig0) {
							main_startcounter0++;
							main_retrig0 = false;
						}
						if(finish_window)
							stop_fifo0 = true;
					}
				}

				if(en_fifo1) {
					// Read TDC-GPX FIFO1
					if( !(mbs & 0x1000) ) { // FIFO1 not empty
						board->set_dra(0x0009);
						dra_data = board->read_dra();

						// Get the start count from data
						start_count = (uint16_t)((dra_data & 0x03FC0000) >> 18);

						// If main_retrig1 flag is set and start_count < 128, the counter has resetted. So we need
						// to update main_startcounter1.
						if(start_count < 128 && main_retrig1) {
							main_startcounter1++;
							main_retrig1 = false;
						}

						// Saving stop data
						int8_t ch = (int8_t)( ((dra_data & 0x0C000000) >> 26) + 5);
						if(((dra_data & 0x00020000) >> 17) == 0) {
							ch = -ch;
						}
						if(current_start->add_event( (uint32_t)( start_count + (main_startcounter1 * 256) ), (uint32_t)( (dra_data & 0x0001FFFF) - board->get_start_offset() ), ch)) {
							syslog(ATMD_ERR, "Measure [Direct read mode]: error saving stops of start %d.", measure_start_counter);
							throw(ATMD_ERR_ALLOC);
						}

					} else {
						if(main_retrig1) {
							main_startcounter1++;
							main_retrig1 = false;
						}
						if(finish_window)
							stop_fifo1 = true;
					}
				}

				// If it's the time to check the time we do it!
				if((check_index % check_step) == 0) {
					if(gettimeofday(&window_end, NULL))
						syslog(ATMD_ERR, "Measure [Direct read mode]: gettimeofday failed with error \"%m\"");
					if(board->check_windowtime_exceeded(window_start, window_end))
						finish_window = true;
				}

				// Stop condition
				if( (!en_fifo0 || stop_fifo0) && (!en_fifo1 || stop_fifo1) )
					break;

			} while(true);

			// We save the start01
			board->set_dra(0x000A);
			current_start->set_start01(board->read_dra() & 0x0001FFFF);

			// Add current start to current measure_finished
			if(current_measure->add_start(current_start)) {
				syslog(ATMD_ERR, "Measure [Direct read mode]: error saving start %d", measure_start_counter);
				throw(ATMD_ERR_ALLOC);
			}

			if(enable_debug)
				syslog(ATMD_DEBUG, "Measure [Direct read mode]: added start %d with %d stops.", measure_start_counter, current_start->count_stops());

			if(board->get_stop() || finish_measure)
				break;

			// We sleep for a while to let the main thread process some network command.
			if(measure_window.get_sec() > 0 || measure_window.get_usec() > 100)
				nanosleep(&sleeptime, NULL);

		} while(true);

	} catch(int e) {
		current_measure->set_incomplete(true);
		board->set_status(ATMD_STATUS_ERROR);
		board->reset();
		*retval = e;
	} catch(exception& e) {
		syslog(ATMD_ERR, "Measure [Direct read mode]: caught an unexpected exception (\"%s\").", e.what());
		current_measure->set_incomplete(true);
		board->set_status(ATMD_STATUS_ERROR);
		board->reset();
		*retval = ATMD_ERR_UNKNOWN_EX;
	} catch(...) {
		syslog(ATMD_ERR, "Measure [Direct read mode]: caught an unknown exception.");
		current_measure->set_incomplete(true);
		board->set_status(ATMD_STATUS_ERROR);
		board->reset();
		*retval = ATMD_ERR_UNKNOWN_EX;
	}

	// We get measure end time and save total elapsed time
	if(gettimeofday(&measure_end, NULL))
		syslog(ATMD_ERR, "Measure [Direct read mode]: gettimeofday failed with error \"%m\"");
	current_measure->set_time(measure_begin, measure_end);
	if(board->get_stop())
		syslog(ATMD_INFO, "Measure [Direct read mode]: measure stopped. Got %d start events. Elapsed time is %lu us.", current_measure->count_starts(), current_measure->get_time());
	else
		syslog(ATMD_INFO, "Measure [Direct read mode]: measure finished. Got %d start events. Elapsed time is %lu us.", current_measure->count_starts(), current_measure->get_time());

	// Save the current measure
	if(current_measure->count_starts() > 0) {
		if(board->add_measure(current_measure)) {
			syslog(ATMD_ERR, "Measure [Direct read mode]: error adding measure to measure vector.");
			board->set_status(ATMD_STATUS_ERROR);
			*retval = ATMD_ERR_EMPTY; // Here we return ATMD_ERR_EMPTY to know that there's no measure saved
		}
	} else {
		syslog(ATMD_WARN, "Measure [Direct read mode]: discarding an empty measurement.");
		board->set_status(ATMD_STATUS_ERROR);
		*retval = ATMD_ERR_EMPTY;
	}

	if(board->get_status() != ATMD_STATUS_ERROR)
		// Setting board status to ATMD_STATUS_FINISHED
		board->set_status(ATMD_STATUS_FINISHED);

	pthread_exit((void*) retval);
}
