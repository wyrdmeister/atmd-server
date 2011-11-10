/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * atmd_hardware.cpp
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

// Prototypes of external measuring functions
extern void *burst_measure(void *arg);
extern void *direct_measure(void *arg);

// Global termination interrupt
extern bool terminate_interrupt;

// Global debug flag
extern bool enable_debug;

/* @fn ATMDboard::ATMDboard(bool simulate)
 * ATMDboard class constructor. Initialise default values and search for the
 * ATMD-GPX PCI board.
 */
ATMDboard::ATMDboard() {
	// Initialization of member variables

	// Hardware base address
	this->base_address = 0x0000;
	
	// Reset board configuration
	this->reset_config();
}


/* @fn void ATMDboard::ATMDboard()
 * Reset board configuration.
 */
void ATMDboard::reset_config() {

	// Channels configuration
	int i;
	for(i = 0; i < 8; i++) {
		this->en_rising[i] = true;
		this->en_falling[i] = false;
	}
	this->en_start_rising = true;
	this->en_start_falling = false;

	// Disable stop before start and start after start
	this->stop_dis_start = true;
	this->start_dis_start = true;

	// Set board resolution
	this->refclkdiv = 7;
	this->hsdiv = 183;
	this->time_bin = (ATMD_TREF * pow(2.0, this->refclkdiv)) / (216 * this->hsdiv) * 1e12;

	// Set timestamps offset
	this->start_offset = 2000;

	// Set mtimer
	this->en_mtimer = true;
	this->mtimer = 0;

	// Unmask start counter msb to intflag
	this->start_to_intflag = true;

	// Set measure window and time to zero
	this->measure_window.set("0s");
	this->measure_time.set("0s");

	// Set measure thread parameters
	this->tid = 0;

	// Board status
	this->board_status = ATMD_STATUS_IDLE;
}


/* @fn ATMDboard::init(bool simulate)
 * Initialize the board.
 *
 * @param simulate Enable board simulation mode. Default to disabled
 * @return Retrun 0 on success, -1 on error.
 */
int ATMDboard::init(bool simulate = false) {
	if(simulate) {
		// Board simulation enabled
		syslog(ATMD_INFO, "Board init: enabled board simulation mode.");
		return 0;
	} else {
		// Board simulation disabled, so we search for the hardware
		return this->search_board();
	}
}


/* @fn ATMDboard::search_board()
 * Scan the PCI bus looking for the ATMD-GPX board. When it founds it, set the
 * correct base address.
 *
 * @return Retrun 0 on success, -1 on error.
 */
int ATMDboard::search_board() {

	// Getting I/O privileges
	if(iopl(3)) {
		syslog(ATMD_CRIT, "Board init: failed to get direct I/O privileges (Error: %m).");
		return -1;
	} else {
		syslog(ATMD_DEBUG, "Board init: direct I/O access privileges correctly configured.");
	}

	// Begin ATMD-GPX board search
	// Initialize PCI structs
	struct pci_access *pci_acc;
	struct pci_dev *pci_dev;
	pci_acc = pci_alloc();
	pci_init(pci_acc);
	pci_scan_bus(pci_acc);

	// Scan PCI bus checking vendor and device ids searching for ATMD-GPX
	for(pci_dev=pci_acc->devices; pci_dev; pci_dev=pci_dev->next) {
		pci_fill_info(pci_dev, PCI_FILL_IDENT | PCI_FILL_BASES);

		if(pci_dev->vendor_id == 0x10b5 && pci_dev->device_id == 0x9050) {
			// If ids match, check all addresses to find the correct one...
			int i;
			for(i=0; i<6; i++) {
				if(pci_dev->base_addr[i]) {
					// We check register at offset 0x4 from base address for module identification value
					if(inw(pci_dev->base_addr[i] + 0x4) == 0x8000) {
						// ATMD-GPX Found!
						this->base_address = pci_dev->base_addr[i];
						break;
					}
				}
			}
			if(this->base_address)
				// We have found the board so we could stop searching the pci bus
				// NOTE: multiple boards are not supported!
				break;
		}
	}
	pci_cleanup(pci_acc);

	if(this->base_address) {
		syslog(ATMD_INFO, "Board init: ATMD-GPX board found at address: 0x%x.", this->base_address);
	} else {
		syslog(ATMD_CRIT, "Board init: ATMD-GPX board not found.");
		return -1;
	}
	return 0;
}


/* @fn ATMDboard::reset()
 * Perform a ATMD-GPX board total reset.
 */
void ATMDboard::reset() {
	syslog(ATMD_INFO, "Performing ATMD-GPX board reset.");

	// Board total reset
	this->mb_config(0x0101);

	struct timespec sleeptime;
	sleeptime.tv_sec = 0;
	sleeptime.tv_nsec = 1000;
	nanosleep(&sleeptime, NULL);

	// Disable inputs by hardware
	this->mb_config(0x0008);
}


/* @fn ATMDboard::master_reset()
 * Perform a ATMD-GPX board total reset.
 */
void ATMDboard::master_reset() {
	syslog(ATMD_INFO, "Performing TDC-GPX master reset.");

	// TDC-GPX software master reset (we need to keep the start timer!)
	uint32_t reg = (0x42400000) | (ATMD_AUTORETRIG);
	reg = reg | ((this->en_mtimer) ? 0x04000000 : 0x0); // Enable mtimer tigger on start pulse
	this->write_register(reg);

	struct timespec sleeptime;
	sleeptime.tv_sec = 0;
	sleeptime.tv_nsec = 1000;
	nanosleep(&sleeptime, NULL);
}


/* @fn ATMDboard::write_register(unsigned int value)
 * Write the first 28bit of value to a configuration register of the TDC-GPX 
 * identified by the most significant 4 bits of value.
 *
 * @param value Data to be written to the configuration register.
 */
void ATMDboard::write_register(uint32_t value) {
	outw((uint16_t)(value & 0x0000FFFF), this->base_address);
	outw((uint16_t)((value & 0xFFFF0000) >> 16), this->base_address+0x2);
}


/* @fn ATMDboard::boardconfig()
 * Reset the board and configure it. Return an error if the PLL cannot lock.
 *
 * @return Return 0 on success, -1 on error.
 */
int ATMDboard::boardconfig() {

	// Reset the board
	this->reset();

	// Register value
	uint32_t reg;

	// Configure reg0: rising and falling edges
	uint16_t rising = 0x0000 | ((this->en_start_rising) ? 0x0001 : 0x0000) | (this->get_rising_mask() << 1);
	uint16_t falling = 0x0000 | ((this->en_start_falling) ? 0x0001 : 0x0000) | (this->get_falling_mask() << 1);
	reg = (0x00000081) | ((rising << 10) | (falling << 19));
	syslog(ATMD_DEBUG, "Config: reg0: 0x%08X.", reg);
	this->write_register(reg);

	// Configure reg1: channel adjust... useless!
	reg = (0x10000000);
	syslog(ATMD_DEBUG, "Config: reg1: 0x%08X.", reg);
	this->write_register(reg);

	// Configure reg2: channels enable and I-Mode
	uint16_t chan_disable = ~(rising | falling) & 0x01FF;
	reg = (0x20000002) | (chan_disable << 3);
	syslog(ATMD_DEBUG, "Config: reg2: 0x%08X.", reg);
	this->write_register(reg);

	// Configure reg3: not used
	reg = (0x30000000);
	syslog(ATMD_DEBUG, "Config: reg3: 0x%08X.", reg);
	this->write_register(reg);

	// Configure reg4: start timer
	reg = (0x42000000) | (ATMD_AUTORETRIG);
	reg = reg | ((this->en_mtimer) ? 0x04000000 : 0x0); // Enable mtimer tigger on start pulse
	syslog(ATMD_DEBUG, "Config: reg4: 0x%08X.", reg);
	this->write_register(reg);

	// Configure reg5: disable stop before start, disable start after start, start offset
	reg = (0x50000000);
	reg = reg | ((this->start_dis_start) ? 0x00400000 : 0x0); // Disable start after start
	reg = reg | ((this->stop_dis_start) ? 0x00200000 : 0x0); // Disable stop before start
	reg = reg | this->start_offset; // Set the start offset
	syslog(ATMD_DEBUG, "Config: reg5: 0x%08X.", reg);
	this->write_register(reg);

	// Configure reg6: nothing relevant
	reg = (0x600000FF);
	syslog(ATMD_DEBUG, "Config: reg6: 0x%08X.", reg);
	this->write_register(reg);

	// Configure reg7: MTimer and PLL settings
	reg = (0x70001800);
	reg = reg | (this->hsdiv & 0x00FF); // Configure high speed divider
	reg = reg | ((this->refclkdiv & 0x0007) << 8); // Configure reference clock divider
	reg = reg | (this->mtimer << 15); // Configure mtimer
	syslog(ATMD_DEBUG, "Config: reg7: 0x%08X.", reg);
	this->write_register(reg);

	// Configure reg11: unmask error flags
	reg = (0xB7FF0000);
	syslog(ATMD_DEBUG, "Config: reg11: 0x%08X.", reg);
	this->write_register(reg);

	// Configure reg12: msb of start to intflag or mtimer end to intflag
	reg = (0xC0000000) | ((this->start_to_intflag) ? 0x04000000 : 0x02000000);
	syslog(ATMD_DEBUG, "Config: reg12: 0x%08X.", reg);
	this->write_register(reg);

	// Sleep to let the PLL lock
	struct timespec sleeptime;
	sleeptime.tv_sec = 0;
	sleeptime.tv_nsec = 500000000;
	nanosleep(&sleeptime, NULL);

	// Check if the PLL is locked
	this->set_dra(0x000C);
	reg = this->read_dra();
	if(enable_debug)
		syslog(ATMD_DEBUG, "Config: status register is 0x%X", reg);
	if(reg & 0x00000400) {
		syslog(ATMD_ERR, "Config: TDC-GPX PLL not locked. Probably wrong resolution (refclkdiv = %d, hsdiv = %d).", this->refclkdiv, this->hsdiv);
		return -1;
	} else {
		syslog(ATMD_DEBUG, "Config: TDC-GPX PLL locked. Using a resolution of %f ps.", this->time_bin);
		return 0;
	}
}


/* @fn ATMDboard::set_window(string timestr)
 * Set the time window of the measure.
 *
 * @param timestr Time string.
 */
int ATMDboard::set_window(string timestr) {
	// Decode time string
	int retval = this->measure_window.set(timestr);

	// If the supplied string is valid, we check that the window time make sense
	if(!retval) {

		// A raw value as the measure window has no meaning
		if(this->measure_window.is_raw()) {
			syslog(ATMD_ERR, "Config: error setting time windows. A raw value is not allowed.");
			this->measure_window.set("0s");
			return -1;
		}
		return 0;
	} else {
		return -1;
	}
}


/* @fn ATMDboard::set_tottime(string timestr)
 * Set the time window of the measure.
 *
 * @param timestr Time string.
 */
int ATMDboard::set_tottime(string timestr) {
	// Decode time string
	int retval = this->measure_time.set(timestr);

	// If the supplied string is valid, we check that the measure time make sense
	if(!retval) {

		// If the saved time is zero we return an error
		if(this->measure_time.is_zero()) {
			syslog(ATMD_ERR, "Config: error setting measure time. A zero value is not valid.");
			return -1;
		}

		// If the time is not raw we check that the measure time is not less than the measure window 
		if(!this->measure_time.is_raw()) {
			double time = this->measure_time.get_sec() * 1e6 + this->measure_time.get_usec();
			double window = this->measure_window.get_sec() * 1e6 + this->measure_window.get_usec();
			if(time < window) {
				syslog(ATMD_ERR, "Config: error setting measure time. Window time cannot be greater than measure time.");
				return -1;
			}
		}
		return 0;
	} else {
		return -1;
	}
}


/* @fn ATMDboard::set_resolution()
 * Set the PLL parameters and calculate the time bin.
 *
 * @param refclk Reference clock divider.
 * @param hs High speed divider.
 */
void ATMDboard::set_resolution(uint16_t refclk, uint16_t hs) {
	// Set PLL parameters
	this->refclkdiv = refclk;
	this->hsdiv = hs;

	// Calculate the time_bin in ps
	this->time_bin = ATMD_TREF * pow(2.0, this->refclkdiv) / (216 * this->hsdiv) * 1e12;
}


/* @fn ATMDboard::start_measure()
 * Start a new measure.
 *
 * @return Return 0 on success, -1 on error.
 */
int ATMDboard::start_measure() {
	/*
	First of all we have to choose the measurement mode. There are two available
	modes:
	1) If time_windows <= 1ms
		We do the measure in burst mode. Measure window duration is controlled by
		the global timer. We do not use an external start counter.
	2) If time windows > 1ms
		We do a full measure with almost unlimited time_window. We use a software
		32 bit start counter.
		In principle the measure window is limited to the number of start events
		that we can count. With a retriggering of 5us and a 32 bit counter the limit
		should be 5.96 hours.

	In every mode the measure time is only limited by the timeval structure taken
	as input by gettimeofday, theoretically giving up to 2^32 seconds of measure
	time, that is approx 136 years.
	*/

	// These checks are here only to be sure that everything is ok. Other checks should
	// have prevented us from reaching this point without the right values in place.
	if(this->measure_window.is_zero() || this->measure_time.is_zero()) {
		syslog(ATMD_ERR, "Measure [Start]: could not start a measure if the timings are not set.");
		return -1;
	}
	if(!this->get_start_rising() && !this->get_start_falling()) {
		syslog(ATMD_ERR, "Measure [Start]: start channel is disabled. Cannot start the measure.");
		return -1;
	}
	if( !(this->get_rising_mask() | this->get_falling_mask()) ) {
		syslog(ATMD_ERR, "Measure [Start]: all stop channels are disabled. Cannot start the measure.");
		return -1;
	}

	// We check if we should simulate the measure_stop
	if(this->simulation()) {
		// TODO: here we should simulate a measure...
		return 0;
	}

	// Thread attributes structure
	pthread_attr_t thread_attributes;
	sched_param thread_sched;

	// Initialize the attribute structure
	if(pthread_attr_init(&thread_attributes)) {
		syslog(ATMD_ERR, "Measure [Start]: could not initialize the thread attribute structure (Error: %m).");
		return -1;
	};

	// Setting the scheduling policy
	pthread_attr_setschedpolicy(&thread_attributes, SCHED_FIFO);

	// Forcing explicit scheduling instead of scheduling inherited from parent process
	pthread_attr_setinheritsched(&thread_attributes, PTHREAD_EXPLICIT_SCHED);

	// Setting priority to the maximum available
	int process_priority = sched_getscheduler(0);
	//thread_sched.sched_priority = process_priority + (sched_get_priority_max(SCHED_FIFO) - process_priority) / 2;
	thread_sched.sched_priority = sched_get_priority_max(SCHED_FIFO);
	syslog(ATMD_INFO, "Measure [Start]: setting measurement thread static priority to %d (Process priority: %d).", thread_sched.sched_priority, process_priority);
	pthread_attr_setschedparam(&thread_attributes, &thread_sched);

	// We set the process at maximum priority
	errno = 0; // reset of errno
	if(setpriority(PRIO_PROCESS, 0, -20) == -1) {
		if(errno != 0) {
			syslog(ATMD_ERR, "Measure [Start]: failed to set real time priority (Error: \"%m\").");
			return -1;
		}
	}

	// BURST MODE DISABLED! DOESN'T WORK!
	//if(this->measure_window.get_sec() == 0 && this->measure_window.get_usec() <= 1000) {
	if(0) {
		// Very short window (Burst mode)

		//We do not unmask the start msb to intflag, but instead mtimer.
		this->start_to_intflag = false;

		// We commit board configuration
		if(this->boardconfig())
			// Board config failed. PLL not locked.
			return -1;

		// We start the measure in burst mode running the measure function in another thread
		this->set_status(ATMD_STATUS_STARTING);
		this->set_stop(false);
		if(pthread_create(&(this->tid), &thread_attributes, &(burst_measure), (void *)this)) {
			syslog(ATMD_ERR, "Measure [Burst mode]: error starting measuring thread with error \"%m\".");
			return -1;
		} else {
			syslog(ATMD_ERR, "Measure [Burst mode]: successfully started measuring thread (tid = %d).", (int)this->get_tid());
		}

	} else {
		// Standard measuring mode (Direct read)

		// MSB of start counter unmasked to intflag
		this->start_to_intflag = true;

		// We commit board configuration
		if(this->boardconfig()) {
			// Board config failed. PLL not locked.
			return -1;
		}

		// We start the measure in direct mode running the measure function in another thread
		this->set_status(ATMD_STATUS_STARTING);
		this->set_stop(false);
		if(pthread_create(&(this->tid), &thread_attributes, &(direct_measure), (void *)this)) {
			syslog(ATMD_ERR, "Measure [Direct read mode]: error starting measuring thread with error \"%m\".");
			return -1;
		} else {
			syslog(ATMD_ERR, "Measure [Direct read mode]: successfully started measuring thread (tid = %d).", (int)this->get_tid());
		}
	}

	// Here we poll board status waiting for ATMD_STATUS_RUNNING
	struct timespec sleeptime;
	sleeptime.tv_sec = 0;
	sleeptime.tv_nsec = 1000;
	time_t start_time = time(NULL);
	time_t end_time = 0;

	do {
		nanosleep(&sleeptime, NULL);
		if(this->get_status() != ATMD_STATUS_STARTING)
			return 0;

		end_time = time(NULL);
		if(end_time - start_time > ATMD_THREAD_TIMEOUT) {
			// Timeout exceeded and thread does not changed board status. We try to abort the measure.
			syslog(ATMD_CRIT, "Measure [Start]: thread hasn't set board status to RUNNING. Something went wrong badly!");
			this->abort_measure();
			return -1;
		}
	} while(true);
}


/* @fn ATMDboard::stop_measure()
 * Stop a measure.
 *
 * @return Return 0 on success, -1 on error.
 */
int ATMDboard::stop_measure() {
	// We set the stop flag
	this->set_stop(true);

	int retval;
	if(this->collect_measure(retval)) {
		// Error recollecting measurement thread
		return -1;

	} else {
		// TODO: we should check the thread return value (maybe...)
		// TODO: here we should check the measurement data (maybe...)

		this->set_stop(false);
		return 0;
	}
}


/* @fn ATMDboard::abort_measure()
 * Abort a measure.
 *
 * @return Return 0 on success, -1 on error.
 */
int ATMDboard::abort_measure() {
	// We set the stop flag
	this->set_stop(true);

	int retval;
	if(this->collect_measure(retval)) {
		// Error recollecting measurement thread
		return -1;

	} else {
		// Here we delete the last measure because we are aborting
		if(this->get_status() == ATMD_STATUS_ERROR) {
			if(retval != ATMD_ERR_EMPTY && this->num_measures() > 0)
				this->del_measure(this->num_measures()-1);
		} else {
			if(this->num_measures() > 0)
				this->del_measure(this->num_measures()-1);
		}

		this->set_stop(false);
		return 0;
	}
}


/* @fn ATMDboard::collect_measure()
 * Get return status of the measurement thread.
 *
 * @return Return 0 on success, -1 on error.
 */
int ATMDboard::collect_measure(int &return_value) {
	int pt_ret;
	int *retval = NULL;

	pt_ret = pthread_join(this->get_tid(), (void **) &retval);

	// Then we check if there was any error
	if(pt_ret == -1)
		// Error waiting for thread to terminate
		syslog(ATMD_ERR, "Measure [Collect]: pthread_join failed with error \"%m\".");

	else
		// Thread terminated correctly
		syslog(ATMD_INFO, "Measure [Collect]: measurement thread terminated correctly. Return value was %d.", *retval);

	if(retval) {
		return_value = *retval;
		free(retval);
	} else {
		return_value = 0;
	}
	
	// We set the process priority back to normal
	errno = 0; // reset of errno
	if(setpriority(PRIO_PROCESS, 0, 0) == -1) {
		if(errno != 0) {
			syslog(ATMD_ERR, "Measure [Collect]: failed to set priority back to normal (Error: \"%m\").");
			return_value = -1;
		} else {
			syslog(ATMD_INFO, "Measure [Collect]: set priority back to normal.");
		}
	} else {
		syslog(ATMD_INFO, "Measure [Collect]: set priority back to normal.");
	}

	this->clear_tid();
	this->set_status(ATMD_STATUS_IDLE);
	return pt_ret;
}


/* @fn ATMDboard::check_measuretime_exceeded(struct timeval& begin, struct timeval& now)
 * Check if the configured measure time has been exceeded.
 *
 * @param begin The timeval structure of the time in which the measure begun (as returned by gettimeofday)
 * @param now The timeval structure of the current time (as returned by gettimeofday)
 * @return Return true time has been exceeded, false otherwise.
 */
bool ATMDboard::check_measuretime_exceeded(struct timeval& begin, struct timeval& now) {

	// Measure time
	int64_t conf_time = (int64_t)this->measure_time.get_sec() * 1000000 + this->measure_time.get_usec();
	int64_t win_time = (int64_t)this->measure_window.get_sec() * 1000000 + this->measure_window.get_usec();

	// Actual time
	int64_t actual_time = ((int64_t)now.tv_sec * 1000000 + now.tv_usec) - ((int64_t)begin.tv_sec * 1000000 + begin.tv_usec);

	if(actual_time < 0) {
		// The begin time is in the future. Something bad happened. We log an error and return true,
		//so we are sure that we don't get stuck in an infinite loop.
		syslog(ATMD_ERR, "Measure [check_windowtime_exceeded]: given begin time is in the future.");
		return true;
	}

	if(actual_time + win_time > conf_time) {
		return true;
	} else {
		return false;
	}
}


/* @fn ATMDboard::check_windowtime_excemeeded(struct timeval& begin, struct timeval& now)
 * Check if the configured window time has been exceeded.
 *
 * @param begin The timeval structure of the time in which the window begun (as returned by gettimeofday)
 * @param now The timeval structure of the current time (as returned by gettimeofday)
 * @return Return true time has been exceeded, false otherwise.
 */
bool ATMDboard::check_windowtime_exceeded(struct timeval& begin, struct timeval& now) {

	// Window time
	int64_t conf_time = (int64_t)this->measure_window.get_sec() * 1000000 + this->measure_window.get_usec();

	// Actual time
	int64_t actual_time = ((int64_t)now.tv_sec * 1000000 + now.tv_usec) - ((int64_t)begin.tv_sec * 1000000 + begin.tv_usec);

	if(actual_time < 0) {
		// The begin time is in the future. Something bad happened. We log an error and return true,
		//so we are sure that we don't get stuck in an infinite loop.
		syslog(ATMD_ERR, "Measure [check_windowtime_exceeded]: given begin time is in the future.");
		return true;
	}

	if(actual_time >= conf_time) {
		return true;
	} else {
		return false;
	}
}


/* @fn ATMDboard::get_rising_mask()
 * Return the bit mask of stop channels rising edge sensitivity.
 *
 * @return Return an 8 bit mask representing the status of the rising edge of channels
 */
uint8_t ATMDboard::get_rising_mask() {
	int i;
	uint8_t rising = 0x0;
	for(i=0; i<8; i++)
		if(this->en_rising[i])
			rising = rising | (0x1 << i);
	return rising;
}


/* @fn ATMDboard::get_falling_mask()
 * Return the bit mask of stop channels falling edge sensitivity.
 *
 * @return Return an 8 bit mask representing the status of the fallings edge of channels
 */
uint8_t ATMDboard::get_falling_mask() {
	int i;
	uint8_t falling = 0x0;
	for(i=0; i<8; i++)
		if(this->en_falling[i])
			falling = falling | (0x1 << i);
	return falling;
}


/* @fn ATMDboard::add_measure(const Measure& measure)
 * Add a measure object to the measures vector
 *
 * @param measure A reference to the measure that should be added.
 * @return Return 0 on success, -1 on error.
 */
int ATMDboard::add_measure(Measure& measure) {
	try {
		this->measures.push_back(measure);
		return 0;
	} catch (exception& e) {
		syslog(ATMD_ERR, "Measure [add_measure]: memory allocation failed with error \"%s\"", e.what());
		return -1;
	}
}


/* @fn ATMDboard::del_measure(int measure_number)
 * Delete a measure object from the measures vector
 *
 * @param measure_number The number of the measure that we want to delete.
 * @return Return 0 on success, -1 on error.
 */
int ATMDboard::del_measure(uint32_t measure_number) {
	if(measure_number < this->num_measures()) {
		this->measures.erase(this->measures.begin()+measure_number);
		return 0;
	} else {
		return -1;
	}
}


/* @fn ATMDboard::save_measure(int measure_number, int format)
 * Save a measure object to a file in the specified format.
 *
 * @param measure_number The number of the measure that we want to save.
 * @param filename The filename including complete path.
 * @param format The file format that should be used to save the measure.
 * @return Return 0 on success, -1 on error.
 */
int ATMDboard::save_measure(uint32_t measure_number, string filename, uint32_t format) {
	fstream savefile;

	// Check if the measure number is valid.
	if(measure_number < 0 || measure_number >= this->num_measures()) {
		syslog(ATMD_ERR, "Measure [save_measure]: trying to save a non exsitent measure.");
		return -1;
	}

	// Then we check that the supplied path is valid (for safety all files should be saved under /home)
	pcrecpp::RE path_re("^\\/home\\/[a-zA-Z0-9\\.\\_\\-\\/]+");
	if(!path_re.FullMatch(filename)) {
		syslog(ATMD_ERR, "Measure [save_measure]: the filename supplied is not valid.");
		return -1;
	}
	// For safety we remove all relative path syntax
	pcrecpp::RE("\\.\\.\\/").GlobalReplace("", &filename);

	// Opening file
	if(format == ATMD_FORMAT_BINPS || format == ATMD_FORMAT_BINRAW) {
		// Open in binary format
		savefile.open(filename.c_str(), fstream::out | fstream::trunc | fstream::binary);

	} else {
		// Open in text format
		savefile.open(filename.c_str(), fstream::out | fstream::trunc);
		savefile.precision(15);
	}

	if(!savefile.is_open()) {
		syslog(ATMD_ERR, "Measure [save_measure]: error opening file %s.", filename.c_str());
		return -1;
	}
	syslog(ATMD_INFO, "Measure [save_measure]: saving measure %d to file %s.", measure_number, filename.c_str());

	int i, j;
	StartData current_start;
	StopData current_stop;
	double stoptime;
	uint32_t ref_count;
	uint32_t start_count = this->measures[measure_number].count_starts();
	uint32_t stop_count = 0;
	uint8_t channel = 0, slope = 0;

	switch(format) {
		case ATMD_FORMAT_RAW:
			// We write an header
			savefile << "start\tchannel\tslope\trefcount\tstoptime" << endl;

			// We cycle over all starts
			for(i = 0; i < start_count; i++) {
				current_start = this->measures[measure_number].get_start(i);

				// We cycle over all stops of current start
				for(j = 0; j < current_start.count_stops(); j++) {
					current_stop = current_start.get_stop(j);
					this->compute_stoptime(measure_number, current_stop, current_start.get_start01(), stoptime, ref_count);
					savefile << i+1 << "\t" << (uint32_t)current_stop.get_channel() << "\t" << (uint32_t)current_stop.get_slope() << "\t" << ref_count << "\t" << stoptime << endl;
				}
			}
			break;

		case ATMD_FORMAT_US:
		case ATMD_FORMAT_PS:
		default:
			// We write an header
			savefile << "start\tchannel\tslope\tstoptime" << endl;

			// We cycle over all starts
			for(i = 0; i < start_count; i++) {
				current_start = this->measures[measure_number].get_start(i);

				// We cycle over all stops of current start
				for(j = 0; j < current_start.count_stops(); j++) {
					current_stop = current_start.get_stop(j);
					this->compute_stoptime(measure_number, current_stop, current_start.get_start01(), stoptime);
					savefile << i+1 << "\t" << (uint32_t)current_stop.get_channel() << "\t" << (uint32_t)current_stop.get_slope() << "\t" << ((format == ATMD_FORMAT_US) ? (stoptime / 1e6) : stoptime) << endl;
				}
			}
			break;

		case ATMD_FORMAT_BINPS:
		case ATMD_FORMAT_BINRAW:
			// We write the number of start in the measure
			savefile.write(reinterpret_cast<char *>(&start_count), sizeof(start_count));

			// We cycle over all starts
			for(i = 0; i < start_count; i++) {
				current_start = this->measures[measure_number].get_start(i);
				uint32_t stop_count = current_start.count_stops();

				// We write the number of stops for the current start
				savefile.write(reinterpret_cast<char *>(&stop_count), sizeof(stop_count));

				// We cycle over all stops of the current start
				for(j = 0; j < stop_count; j++) {
					current_stop = current_start.get_stop(j);
					channel = current_stop.get_channel();
					slope = current_stop.get_slope();

					if(format == ATMD_FORMAT_BINRAW) {
						this->compute_stoptime(measure_number, current_stop, current_start.get_start01(), stoptime, ref_count);
						savefile.write(reinterpret_cast<char *>(&channel), sizeof(channel));
						savefile.write(reinterpret_cast<char *>(&slope), sizeof(slope));
						savefile.write(reinterpret_cast<char *>(&ref_count), sizeof(ref_count));
						savefile.write(reinterpret_cast<char *>(&stoptime), sizeof(stoptime));

					} else {
						this->compute_stoptime(measure_number, current_stop, current_start.get_start01(), stoptime);
						savefile.write(reinterpret_cast<char *>(&channel), sizeof(channel));
						savefile.write(reinterpret_cast<char *>(&slope), sizeof(slope));
						savefile.write(reinterpret_cast<char *>(&stoptime), sizeof(stoptime));
					}
				}
			}
			break;

		case ATMD_FORMAT_DEBUG:
			// We write an header
			savefile << "start\tstart_time\ttimefrombegin\tchannel\tslope\trefcount\tstoptime" << endl;

			// We cycle over all starts
			for(i = 0; i < start_count; i++) {
				current_start = this->measures[measure_number].get_start(i);

				// We cycle over all stops of current start
				for(j = 0; j < current_start.count_stops(); j++) {
					current_stop = current_start.get_stop(j);
					this->compute_stoptime(measure_number, current_stop, current_start.get_start01(), stoptime, ref_count);
					savefile << i+1 << "\t" << current_start.get_time() << "\t" << current_start.get_timefrombegin() << "\t" << (uint32_t)current_stop.get_channel() << "\t" << (uint32_t)current_stop.get_slope() << "\t" << ref_count << "\t" << stoptime << endl;
				}
			}
			break;
	}
	savefile.close();

	return 0;
}


/* @fn ATMDboard::compute_stoptime(uint32_t measure_number, StopData& stop, uint32_t start01, double& stoptime)
 * Compute the stop time in ps.
 *
 * @param measure_number The number of the measure that we want to save.
 * @param stop The stop object
 * @param start01 The start01 value
 * @param stoptime The return value
 */
void ATMDboard::compute_stoptime(uint32_t measure_number, StopData& stop, uint32_t start01, double& stoptime) {
	if(stop.get_startcount() == 1) {
		stoptime = (double)(stop.get_bins() + start01) * this->measures[measure_number].get_tbin();

	} else if(stop.get_startcount() > 0) {
		stoptime = (double)(stop.get_bins() + start01) * this->measures[measure_number].get_tbin() + (double)(stop.get_startcount() - 1) * (ATMD_AUTORETRIG + 1) * ATMD_TREF * 1e12;

	} else {
		stoptime = (double)stop.get_bins() * this->measures[measure_number].get_tbin();
	}
}


/* @fn ATMDboard::compute_stoptime(uint32_t measure_number, StopData& stop, uint32_t start01, double& stoptime, uint64_t& start_count)
 * Compute the stop time in raw format.
 *
 * @param measure_number The number of the measure that we want to save.
 * @param stop The stop object
 * @param start01 The start01 value
 * @param stoptime The returned stoptime in ps (without start autoretriggers)
 * @param start_count The returet count of autoretriggers (in unit of ATMD_TREF)
 */
void ATMDboard::compute_stoptime(uint32_t measure_number, StopData& stop, uint32_t start01, double& stoptime, uint32_t& start_count) {
	if(stop.get_startcount() == 1) {
		stoptime = (double)(stop.get_bins() + start01) * this->measures[measure_number].get_tbin();
		start_count = 0;

	} else if(stop.get_startcount() > 0) {
		stoptime = (double)(stop.get_bins() + start01) * this->measures[measure_number].get_tbin();
		start_count = stop.get_startcount() - 1;

	} else {
		stoptime = (double)stop.get_bins() * this->measures[measure_number].get_tbin();
		start_count = 0;
	}
}


/* @fn ATMDboard::stat_measure(uint32_t measure_number, uint32_t& starts)
 * Return the number of starts in the measure.
 *
 * @param measure_number The number of the measure that we want to save.
 * @param starts Reference to the output variable.
 * @return Return 0 on success, -1 on error.
 */
int ATMDboard::stat_measure(uint32_t measure_number, uint32_t& starts) {

	// Check if the measure number is valid.
	if(measure_number < 0 || measure_number >= this->num_measures()) {
		syslog(ATMD_ERR, "Measure [stat_measure]: trying to get statistics about a non exsitent measure.");
		return -1;
	}

	starts = this->measures[measure_number].count_starts();
	return 0;
}


/* @fn ATMDboard::stat_stops(uint32_t measure_number, vector< vector<uint32_t> >& stop_counts)
 * Return a vector of vectors of integers with the counts of stops on each channel.
 *
 * @param measure_number The number of the measure that we want to save.
 * @param stop_counts Reference to the vector of vectors to output the data.
 * @return Return 0 on success, -1 on error.
 */
int ATMDboard::stat_stops(uint32_t measure_number, vector< vector<uint32_t> >& stop_counts) {

	// Check if the measure number is valid.
	if(measure_number < 0 || measure_number >= this->num_measures()) {
		syslog(ATMD_ERR, "Measure [stat_stops]: trying to get statistics about a non exsitent measure.");
		return -1;
	}

	vector<uint32_t> stops_ch(9,0);
	int i, j;
	StartData current_start;
	StopData current_stop;

	// We cycle over all starts
	for(i = 0; i < this->measures[measure_number].count_starts(); i++) {
		current_start = this->measures[measure_number].get_start(i);

		// We save the measure time
		stops_ch[0] = (uint32_t)(current_start.get_time() / (uint64_t)(((ATMD_AUTORETRIG + 1) * 25) / 1000));

		// We cycle over all stops of current start
		for(j = 0; j < current_start.count_stops(); j++) {
			current_stop = current_start.get_stop(j);
			stops_ch[current_stop.get_channel()]++;
		}

		// We add the vector with counts to the output
		stop_counts.push_back(stops_ch);

		// We reset the counts
		for(j = 0; j < 9; j++)
			stops_ch[j] = 0;
	}

	return 0;
}


/* @fn ATMDboard::stat_stops(uint32_t measure_number, vector< vector<uint32_t> >& stop_counts, string win_start, string win_ampl)
 * Return a vector of vectors of integers with the counts of stops on each channel.
 *
 * @param measure_number The number of the measure that we want to save.
 * @param stop_counts Reference to the vector of vectors to output the data.
 * @return Return 0 on success, -1 on error.
 */
int ATMDboard::stat_stops(uint32_t measure_number, vector< vector<uint32_t> >& stop_counts, string win_start, string win_ampl) {

	// Check if the measure number is valid.
	if(measure_number < 0 || measure_number >= this->num_measures()) {
		syslog(ATMD_ERR, "Measure [stat_stops]: trying to get statistics about a non exsitent measure.");
		return -1;
	}

	Timings window_start, window_amplitude;
	if(window_start.set(win_start)) {
		syslog(ATMD_ERR, "Measure [stat_stops]: the time string passed is not valid (\"%s\").", win_start.c_str());
		return -1;
	}
	if(window_amplitude.set(win_ampl)) {
		syslog(ATMD_ERR, "Measure [stat_stops]: the time string passed is not valid (\"%s\").", win_ampl.c_str());
		return -1;
	}

	vector<uint32_t> stops_ch(9,0);
	int i, j;
	double stoptime;
	StartData current_start;
	StopData current_stop;

	// We cycle over all starts
	for(i = 0; i < this->measures[measure_number].count_starts(); i++) {
		current_start = this->measures[measure_number].get_start(i);

		// We save the measure time
		stops_ch[0] = (uint32_t)(current_start.get_time() / (uint64_t)(((ATMD_AUTORETRIG + 1) * 25) / 1000));

		// We cycle over all stops of current start
		for(j = 0; j < current_start.count_stops(); j++) {
			current_stop = current_start.get_stop(j);

			// We compute the stop time to compare it to the window
			this->compute_stoptime(measure_number, current_stop, current_start.get_start01(), stoptime);

			if(stoptime > window_start.get_ps() && stoptime < window_start.get_ps()+window_amplitude.get_ps())
				stops_ch[current_stop.get_channel()]++;
		}

		// We add the vector with counts to the output
		stop_counts.push_back(stops_ch);

		// We reset the counts
		for(j = 0; j < 9; j++)
			stops_ch[j] = 0;
	}

	return 0;
}
