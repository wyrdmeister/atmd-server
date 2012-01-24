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

#include "atmd_measure_direct.h"

// External global debug flag
extern bool enable_debug;

// Global RT_TASK object
extern RT_TASK main_task;
extern RT_TASK meas_task;

// Watchdog
extern RT_ALARM watchdog;


// Real time task prototype
void rt_measure(void *arg);


/* @fn direct_measure(void *arg)
 * Pefrorm a direct read measure. This should be run inside a thread.
 *
 * @param arg The pointer to the board object casted to a void pointer.
 * @return Return a void pointer to the return value (should be freed after the end of the thread).
 */
void direct_measure(void* arg) {

	int rval = rt_task_set_mode(T_WARNSW | T_RPIOFF, T_NOSIG, NULL);
	if(rval)
		syslog(ATMD_ERR, "Measure [Direct read RT]: task set mode failed with error \"%s\" [%d].", (rval == -EINVAL) ? "invalid bitmask" : ((rval == -EPERM) ? "wrong context" : "unknown error"), rval);

	// Reconstruct the board object
	ATMDboard *board = (ATMDboard *)arg;

	// Create alarm
	rval = rt_alarm_create(&watchdog, "meas_watchdog");
	if(rval) {
		switch(rval) {
			case -ENOMEM:
				syslog(ATMD_ERR, "Measure [Direct read RT]: error creating real-time alarm (Reason: not enough memory).");
				break;

			case -EEXIST:
				syslog(ATMD_ERR, "Measure [Direct read RT]: error creating real-time alarm (Reason: name already exist).");
				break;

			case -EPERM:
				syslog(ATMD_ERR, "Measure [Direct read RT]: error creating real-time alarm (Reason: permission denied).");
				break;

			default:
				syslog(ATMD_ERR, "Measure [Direct read RT]: error creating real-time alarm (Reason: unknown reason %d).", rval);
				break;
		}
		board->set_status(ATMD_STATUS_ERROR);
		board->retval(ATMD_ERR_ALARM);
		return;
	}

	// Initialization of return value
	board->retval(ATMD_ERR_NONE);

	// Set the measure_running flag
	board->set_status(ATMD_STATUS_RUNNING);

	// Just to be sure we reset the stop flag
	board->set_stop(false);

	// We sleep for 1ms to let the main thread answer to the client.
	rt_task_sleep(1000000);

	// Counter for start events. Useful if measure time is a raw value
	uint32_t measure_start_counter = 0;

	// Allocate objects to hold measurement data
	Measure* current_measure = new Measure(); // NOTE memory allocation switches to secondary mode
	StartData* current_start = NULL;

	// Data exchange structure
	struct rt_data data_exch;
	memset(&data_exch, 0x00, sizeof(struct rt_data));

	// Board
	data_exch.board = board;

	// Events buffer
	data_exch.events = new struct rt_event[sizeof(struct rt_event) * board->get_max_ev()]; // NOTE memory allocation switches to secondary mode

	// Enforce again primary mode
	rval = rt_task_set_mode(T_WARNSW | T_RPIOFF, T_NOSIG, NULL);
	if(rval)
		syslog(ATMD_ERR, "Measure [Direct read RT]: task set mode failed with error \"%s\" [%d].", (rval == -EINVAL) ? "invalid bitmask" : ((rval == -EPERM) ? "wrong context" : "unknown error"), rval);

	// Time measuring structures
	data_exch.measure_begin = rt_timer_read();
	data_exch.measure_end = data_exch.measure_begin;
	data_exch.window_begin = data_exch.measure_begin;
	data_exch.window_end = data_exch.measure_begin;

	// Flag for first start
	data_exch.first_start = true;

	// Initialize bunch number
	bool en_bnum = board->bn_enabled();
	uint32_t bnum = 0;
	uint64_t bnum_ts = 0;
	if(en_bnum)
		current_measure->bnum_en(true);

	// Start watchdog alarm
	rt_alarm_start(&watchdog, 1000000, 1000000);

	// Begin measurement. We use exceptions to catch errors
	try {

		do {
			// Bunch number as real timestamp
			bnum_ts = (uint64_t)rt_timer_read() / 1000;

			// Spawn realtime measurement task
			rval = rt_task_spawn(&meas_task, "rt_meas", 0, 99, T_JOINABLE, rt_measure, (void*)&data_exch);
			if(rval) {
				switch(rval) {
					case -ENOMEM:
						syslog(ATMD_ERR, "Measure [Direct read mode]: error starting measurement thread: insufficient memory.");
						break;
					case EEXIST:
						syslog(ATMD_ERR, "Measure [Direct read mode]: error starting measurement thread: thread id already in use.");
						break;
					case -EPERM:
						syslog(ATMD_ERR, "Measure [Direct read mode]: error starting measurement thread: permission denied.");
						break;
					default:
						syslog(ATMD_ERR, "Measure [Direct read mode]: error starting measurement thread: unknown error (%d).", rval);
						break;
				}
				throw(ATMD_ERR_TH_SPAWN);
			}

			// Wait for the task to finish
			rval = rt_task_join(&meas_task);
			if(rval) {
				switch(rval) {
					case -EINVAL:
						syslog(ATMD_ERR, "Measure [Direct read mode]: error joining measurement thread: task not joinable.");
						break;
					case -EDEADLK:
						syslog(ATMD_ERR, "Measure [Direct read mode]: error joining measurement thread: self-join.");
						break;
					case -ESRCH:
						syslog(ATMD_ERR, "Measure [Direct read mode]: error joining measurement thread: cannot find task.");
						break;
					default:
						syslog(ATMD_ERR, "Measure [Direct read mode]: error joining measurement thread: unknown error (%d).", rval);
						break;
				}
				throw(ATMD_ERR_TH_JOIN);
			} else {
				if(enable_debug)
					syslog(ATMD_DEBUG, "Measure [Direct read mode]: successfully joined RT measurement thread.");
			}

			// Send bunch number
			if(en_bnum) {
				if(board->send_bn(bnum, bnum_ts)) { // NOTE network call switches to secondary mode
					syslog(ATMD_WARN, "Measure [Direct read mode]: failed to send bunch number %d.", bnum);
				}
			}

			// Check if any data was acquired
			if(!data_exch.good_start) {
				// If we have already got some start, this condition happens just because the measurement time is finished.
				if(measure_start_counter == 0) {
					// No data acquired. Return error.
					syslog(ATMD_ERR, "Measure [Direct read mode]: didn't got any start event. Terminating measurement.");
					throw(ATMD_ERR_NOSTART);
				}

			} else {
				// We have a good start, so we can increment the counter
				measure_start_counter++;
				if(enable_debug)
					syslog(ATMD_DEBUG, "Measure [Direct read mode]: got start #%d.", measure_start_counter);


				// Create start object
				current_start = new StartData(); // NOTE memory allocation switches to secondary mode
				current_start->reserve(data_exch.num_events); // NOTE memory allocation switches to secondary mode
				for(size_t i = 0; i < data_exch.num_events; i++)
					current_start->add_event(data_exch.events[i].retrig_count, data_exch.events[i].stoptime, data_exch.events[i].ch);
				current_start->set_start01(data_exch.start01);
				current_start->set_timefrombegin(data_exch.measure_begin, data_exch.window_begin);
				current_start->set_time(data_exch.window_begin, data_exch.window_end);
				current_start->set_tbin(board->get_timebin());
				if(en_bnum) {
					current_start->set_bnum(bnum, bnum_ts);
					bnum++;
				}

				// Add current start to current measure
				if(current_measure->add_start(current_start)) { // NOTE memory allocation switches to secondary mode
					syslog(ATMD_ERR, "Measure [Direct read mode]: error saving start %d", measure_start_counter);
					throw(ATMD_ERR_ALLOC);
				}

				if(enable_debug)
					syslog(ATMD_DEBUG, "Measure [Direct read mode]: added start %d with %d stops.", measure_start_counter, current_start->count_stops());

				// Here we ended the part that requires standard syscalls so we can enforce again primary mode
				rval = rt_task_set_mode(0, T_NOSIG, NULL);
				if(rval)
					syslog(ATMD_ERR, "Measure [Direct read RT]: task set mode failed with error \"%s\" [%d].", (rval == -EINVAL) ? "invalid bitmask" : ((rval == -EPERM) ? "wrong context" : "unknown error"), rval);
			}

			// Update end time
			data_exch.measure_end = rt_timer_read();

			// Check if we exceeded the start count or the measure time
			if(board->get_tottime().is_raw()) {
				if(measure_start_counter >= board->get_tottime().get_raw())
					break;
			} else {
				if(board->check_measuretime_exceeded(data_exch.measure_begin, data_exch.measure_end))
					break;
			}

			if(board->get_stop())
				break;

			// Wait for a trigger from the watchdog
			rt_alarm_wait(&watchdog);

		} while(true);

	} catch(int e) {
		data_exch.measure_end = rt_timer_read();
		current_measure->set_incomplete(true);
		board->set_status(ATMD_STATUS_ERROR);
		board->reset();
		if(e == ATMD_ERR_NOSTART && current_measure->count_starts() == 0)
			e = ATMD_ERR_EMPTY;
		board->retval(e);

	} catch(exception& e) {
		syslog(ATMD_ERR, "Measure [Direct read mode]: caught an unexpected exception (\"%s\").", e.what());
		current_measure->set_incomplete(true);
		board->set_status(ATMD_STATUS_ERROR);
		board->reset();
		board->retval(ATMD_ERR_UNKNOWN_EX);

	} catch(...) {
		syslog(ATMD_ERR, "Measure [Direct read mode]: caught an unknown exception.");
		current_measure->set_incomplete(true);
		board->set_status(ATMD_STATUS_ERROR);
		board->reset();
		board->retval(ATMD_ERR_UNKNOWN_EX);
	}

	// Free event buffer
	delete data_exch.events;

	// We get measure end time and save total elapsed time
	current_measure->set_time(data_exch.measure_begin, data_exch.measure_end);
	if(board->get_stop())
		syslog(ATMD_INFO, "Measure [Direct read mode]: measure stopped. Got %d start events. Elapsed time is %lu us.", current_measure->count_starts(), current_measure->get_time());
	else
		syslog(ATMD_INFO, "Measure [Direct read mode]: measure finished. Got %d start events. Elapsed time is %lu us.", current_measure->count_starts(), current_measure->get_time());

	// Save the current measure
	if(current_measure->count_starts() > 0) {
		if(board->add_measure(current_measure)) {
			syslog(ATMD_ERR, "Measure [Direct read mode]: error adding measure to measure vector.");
			board->set_status(ATMD_STATUS_ERROR);
			board->retval(ATMD_ERR_EMPTY); // Here we return ATMD_ERR_EMPTY to know that there's no measure saved
		}
	} else {
		syslog(ATMD_WARN, "Measure [Direct read mode]: discarding an empty measurement.");
		board->set_status(ATMD_STATUS_ERROR);
		board->retval(ATMD_ERR_EMPTY);
	}

	if(board->get_status() != ATMD_STATUS_ERROR)
		// Setting board status to ATMD_STATUS_FINISHED
		board->set_status(ATMD_STATUS_FINISHED);

	rt_alarm_stop(&watchdog);
	rt_alarm_delete(&watchdog);

	return;
}


void rt_measure(void *arg) {

	if(enable_debug)
		syslog(ATMD_DEBUG, "Measure [Direct read RT]: started real time task.");

	// Make sure we are in primary mode!
	int rval = rt_task_set_mode(0, T_WARNSW | T_NOSIG, NULL); //T_LOCK |
	if(rval)
		syslog(ATMD_ERR, "Measure [Direct read RT]: task set mode failed with error \"%s\" [%d].", (rval == -EINVAL) ? "invalid bitmask" : ((rval == -EPERM) ? "wrong context" : "unknown error"), rval);

	// Cast argument to rt_data structure
	struct rt_data* data_exch = (struct rt_data*)arg;

	// Extract board object
	ATMDboard* board = data_exch->board;

	// Reset event buffer
	data_exch->num_events = 0;
	memset(data_exch->events, 0x00, sizeof(struct rt_event) * board->get_max_ev());
	data_exch->good_start = false;

	// We reset the window finish flag and the start received flag
	bool finish_window = false;
	bool finish_measure = false;

	// Check if we need to read both FIFOs or only one
	uint8_t en_channel = board->get_rising_mask() | board->get_falling_mask();
	bool en_fifo0 = (en_channel & 0x0F);
	bool en_fifo1 = (en_channel & 0xF0);

	uint16_t mbs = 0x0000; // Motherboard status register
	uint32_t dra_data = 0x00000000; // Reg12 -> Flag for the end of mtimer

	// Make a TDC-GPX master reset
	board->master_reset();

	// We enable the inputs
	board->mb_config(0x0000);

	// We set dra address to 0x000C to read reg12 and detect end of mtimer
	board->set_dra(0x000C);

	// We wait for a start pulse (through mtimer flag in reg12 of TDC-GPX)
	do {

		// Read the reg12 of TDC-GPX
		dra_data = board->read_dra();

		if(dra_data & 0x00001000) {
			// Set good_start flag
			data_exch->good_start = true;

			// We save the begin time of the measure window
			data_exch->window_begin = rt_timer_read();

			if(data_exch->first_start) {
				data_exch->first_start = false;
				// We synchronize the measure begin time with the start time
				data_exch->measure_begin = data_exch->window_begin;
			}

			if(enable_debug)
				syslog(ATMD_DEBUG, "Measure [Direct read RT]: received start.");

			// Exit loop
			break;
		}

		// TODO: add a timeout in the case of a raw measure time

		// We update the measure_end structure
		data_exch->measure_end = rt_timer_read();

		// Check if the measure time is exceeded
		if(board->check_measuretime_exceeded(data_exch->measure_begin, data_exch->measure_end)) {
			finish_measure = true;
			break;
		}

	} while(!board->get_stop());

	// If we exited the previous cycle for a stop or time exceed we stop here.
	if(finish_measure || board->get_stop()) {
		if(enable_debug)
			syslog(ATMD_DEBUG, "Measure [Direct read RT]: exiting without any measurement.");
		return;
	}

	// Flags to detect overflow of internal start counter
	bool act_intflag = false;
	bool prv_intflag = false;
	//bool med_intflag0 = false;
	//bool med_intflag1 = false;

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
	int16_t start_count = 0;
	int16_t prev_start_count0 = -1;
	int16_t prev_start_count1 = -1;
	//int16_t med_start_count0 = 0;
	//int16_t med_start_count1 = 0;

	// Start data acquisition
	do {
		// Read motherboard status
		mbs = board->mb_status();

		// Get interrupt flag
		act_intflag = (bool)(mbs & 0x0020);

		/*
		if(!prv_intflag && act_intflag) {
			// Intflag change from 0 to 1 -> StartCount reached 128!
			med_intflag0 = true;
			med_intflag1 = true;
		}
		*/

		if(prv_intflag && !act_intflag) {
			// Intflag changed from 1 to 0 -> Overflow!
			main_retrig0 = true;
			main_retrig1 = true;
		}
		prv_intflag = act_intflag;

		// When we recieve the stop command, we set the finish_window flag
		if(board->get_stop())
			finish_window = true;

		if(en_fifo0) {
			// Read TDC-GPX FIFO0
			if( !(mbs & 0x0800) ) { // FIFO0 not empty
				board->set_dra(0x0008);
				dra_data = board->read_dra();

				// Get the start count from data
				start_count = (int16_t)((dra_data & 0x03FC0000) >> 18);

				// Get stop time
				int32_t stoptime = (int32_t)(dra_data & 0x0001FFFF) - board->get_start_offset();

				// Get channel
				int8_t ch = (int8_t)( ((dra_data & 0x0C000000) >> 26) + 1);
				if(((dra_data & 0x00020000) >> 17) == 0)
					ch = -ch;

				// If main_retrig0 flag is set we should find out if the main_startcounter0 should be updated
				if(main_retrig0) {
					if(prev_start_count0 == -1) {
						// This is the first datum we download from the board, so if the start_count < 128 is from the new external retrigger
						if(start_count < 128) {
							main_startcounter0++;
							main_retrig0 = false;
						}
					} else if(prev_start_count0 > start_count) {
						// In this case we are pretty sure that the counter should be updated
						main_startcounter0++;
						main_retrig0 = false;
					}
				}
				prev_start_count0 = start_count;

				/* Old criteria
				if(start_count < 128 && main_retrig0) {
					main_startcounter0++;
					main_retrig0 = false;
				}
				*/

				if(data_exch->num_events < board->get_max_ev()) {
					data_exch->events[data_exch->num_events].stoptime = stoptime;
					data_exch->events[data_exch->num_events].retrig_count = (uint32_t)start_count + (main_startcounter0 * 256);
					data_exch->events[data_exch->num_events].ch = ch;
					data_exch->num_events++;
				} else {
					finish_window = true;
					stop_fifo0 = true;
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

				// Get stop time
				int32_t stoptime = (int32_t)(dra_data & 0x0001FFFF) - board->get_start_offset();

				// Get channel
				int8_t ch = (int8_t)( ((dra_data & 0x0C000000) >> 26) + 5);
				if(((dra_data & 0x00020000) >> 17) == 0)
					ch = -ch;

				if(main_retrig1) {
					if(prev_start_count1 == -1) {
						// This is the first datum we download from the board, so if the start_count < 128 is from the new external retrigger
						if(start_count < 128) {
							main_startcounter1++;
							main_retrig1 = false;
						}
					} else if(prev_start_count1 > start_count) {
						// In this case we are pretty sure that the counter should be updated
						main_startcounter1++;
						main_retrig1 = false;
					}
				}
				prev_start_count1 = start_count;

				/* Old criteria
				prev_start_count0 = start_count;
				if(start_count < 128 && main_retrig1) {
					main_startcounter1++;
					main_retrig1 = false;
				}
				*/

				if(data_exch->num_events < board->get_max_ev()) {
					data_exch->events[data_exch->num_events].stoptime = stoptime;
					data_exch->events[data_exch->num_events].retrig_count = (uint32_t)start_count + (main_startcounter1 * 256);
					data_exch->events[data_exch->num_events].ch = ch;
					data_exch->num_events++;
				} else {
					finish_window = true;
					stop_fifo1 = true;
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

		// If the finish window flag is set we disable the inputs
		if(finish_window) {
			data_exch->window_end = rt_timer_read();
			board->mb_config(0x0008); // Disable inputs
		}

		// Check window time
		if(!finish_window) {
			data_exch->window_end = rt_timer_read();
			if(board->check_windowtime_exceeded(data_exch->window_begin, data_exch->window_end))
				finish_window = true;
		}

		// Stop condition
		if( (!en_fifo0 || stop_fifo0) && (!en_fifo1 || stop_fifo1) )
			break;

	} while(true);

	// We save the start01
	board->set_dra(0x000A);
	data_exch->start01 = (uint32_t)(board->read_dra() & 0x0001FFFF);
}
