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

// Prototypes of external measuring function
extern void direct_measure(void *arg);

// Global termination interrupt
extern bool terminate_interrupt;

// Global debug flag
extern bool enable_debug;

// RT_TASK objects
extern RT_TASK main_task;
extern RT_TASK meas_task;


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

	// Max number of events per start
	this->max_ev = ATMD_MAX_EV;

	// Unmask start counter msb to intflag
	this->start_to_intflag = true;

	// Set measure window and time to zero
	this->measure_window.set("0s");
	this->measure_time.set("0s");

	// Measure thread return value
	this->_retval = 0;

	// Board status
	this->board_status = ATMD_STATUS_IDLE;

	// Autorestart
	this->autostart = ATMD_AUTOSTART_DISABLED;
	this->fileprefix = "";
	this->auto_range.set("0s");
	this->auto_format = ATMD_FORMAT_MATPS2;
	this->measure_counter = 0;

	// FTP variables
	memset(this->curl_error, 0x0, CURL_ERROR_SIZE);
	this->host = "";
	this->username = "";
	this->password = "";

	// BN host
	this->_bnhost = "";
	this->_bnport = 0;
	this->_bnsock = 0;
	memset(&(this->_bn_address), 0, sizeof(struct sockaddr_in));
}


/* @fn ATMDboard::init(bool simulate)
 * Initialize the board.
 *
 * @param simulate Enable board simulation mode. Default to disabled
 * @return Retrun 0 on success, -1 on error.
 */
int ATMDboard::init(bool simulate = false) {
	// Initialize libcurl handle
	if((this->easy_handle = curl_easy_init()) == NULL) {
		syslog(ATMD_INFO, "Board init: failed to initialize libcurl.");
		return -1;
	}
	memset(this->curl_error, 0x0, CURL_ERROR_SIZE);
	curl_easy_setopt(this->easy_handle, CURLOPT_ERRORBUFFER, this->curl_error);

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
		syslog(ATMD_CRIT, "Board [search_board]: failed to get direct I/O privileges (Error: %m).");
		return -1;
	} else {
		syslog(ATMD_DEBUG, "Board [search_board]: direct I/O access privileges correctly configured.");
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
			if(enable_debug)
				syslog(ATMD_INFO, "Board [search_board]: Found matching board. VID 0x%04x, PID 0x%04x", pci_dev->vendor_id, pci_dev->device_id);

			int i;
			for(i=0; i<6; i++) {
				if(pci_dev->base_addr[i]) {
					if(enable_debug)
						syslog(ATMD_INFO, "Board [search_board]: Checking hardware address 0x%04x.", (uint16_t)(pci_dev->base_addr[i] & PCI_ADDR_IO_MASK));

					// We check register at offset 0x4 from base address for module identification value
					if(inw( (pci_dev->base_addr[i] & PCI_ADDR_IO_MASK) + 0x4) == 0x8000) {
						// ATMD-GPX Found!
						this->base_address = (uint16_t)(pci_dev->base_addr[i] & PCI_ADDR_IO_MASK);

						if(enable_debug)
							syslog(ATMD_INFO, "Board [search_board]: Board address set to 0x%04x.", this->base_address);

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
		syslog(ATMD_INFO, "Board [search_board]: ATMD-GPX board found at address: 0x%04x.", this->base_address);
	} else {
		syslog(ATMD_CRIT, "Board [search_board]: ATMD-GPX board not found.");
		return -1;
	}
	return 0;
}


/* @fn ATMDboard::reset()
 * Perform a ATMD-GPX board total reset.
 */
void ATMDboard::reset() {
	syslog(ATMD_INFO, "Board [reset]: Performing ATMD-GPX board reset.");

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
	if(enable_debug)
		syslog(ATMD_INFO, "Board [master_reset] Performing TDC-GPX master reset.");

	// TDC-GPX software master reset (we need to keep the start timer!)
	uint32_t reg = (0x42400000) | (ATMD_AUTORETRIG);
	reg = reg | ((this->en_mtimer) ? 0x04000000 : 0x0); // Enable mtimer tigger on start pulse
	this->write_register(reg);

	rt_task_sleep(1000);
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
	if(enable_debug)
		syslog(ATMD_DEBUG, "Config [boardconfig]: reg0: 0x%08X.", reg);
	this->write_register(reg);

	// Configure reg1: channel adjust... useless!
	reg = (0x10000000);
	if(enable_debug)
		syslog(ATMD_DEBUG, "Config [boardconfig]: reg1: 0x%08X.", reg);
	this->write_register(reg);

	// Configure reg2: channels enable and I-Mode
	uint16_t chan_disable = ~(rising | falling) & 0x01FF;
	reg = (0x20000002) | (chan_disable << 3);
	if(enable_debug)
		syslog(ATMD_DEBUG, "Config [boardconfig]: reg2: 0x%08X.", reg);
	this->write_register(reg);

	// Configure reg3: not used
	reg = (0x30000000);
	if(enable_debug)
		syslog(ATMD_DEBUG, "Config [boardconfig]: reg3: 0x%08X.", reg);
	this->write_register(reg);

	// Configure reg4: start timer
	reg = (0x42000000) | (ATMD_AUTORETRIG);
	reg = reg | ((this->en_mtimer) ? 0x04000000 : 0x0); // Enable mtimer tigger on start pulse
	if(enable_debug)
		syslog(ATMD_DEBUG, "Config [boardconfig]: reg4: 0x%08X.", reg);
	this->write_register(reg);

	// Configure reg5: disable stop before start, disable start after start, start offset
	reg = (0x50000000);
	reg = reg | ((this->start_dis_start) ? 0x00400000 : 0x0); // Disable start after start
	reg = reg | ((this->stop_dis_start) ? 0x00200000 : 0x0); // Disable stop before start
	reg = reg | this->start_offset; // Set the start offset
	if(enable_debug)
		syslog(ATMD_DEBUG, "Config [boardconfig]: reg5: 0x%08X.", reg);
	this->write_register(reg);

	// Configure reg6: nothing relevant
	reg = (0x600000FF);
	if(enable_debug)
		syslog(ATMD_DEBUG, "Config [boardconfig]: reg6: 0x%08X.", reg);
	this->write_register(reg);

	// Configure reg7: MTimer and PLL settings
	reg = (0x70001800);
	reg = reg | (this->hsdiv & 0x00FF); // Configure high speed divider
	reg = reg | ((this->refclkdiv & 0x0007) << 8); // Configure reference clock divider
	reg = reg | (this->mtimer << 15); // Configure mtimer
	if(enable_debug)
		syslog(ATMD_DEBUG, "Config [boardconfig]: reg7: 0x%08X.", reg);
	this->write_register(reg);

	// Configure reg11: unmask error flags
	reg = (0xB7FF0000);
	if(enable_debug)
		syslog(ATMD_DEBUG, "Config [boardconfig]: reg11: 0x%08X.", reg);
	this->write_register(reg);

	// Configure reg12: msb of start to intflag or mtimer end to intflag
	reg = (0xC0000000) | ((this->start_to_intflag) ? 0x04000000 : 0x02000000);
	if(enable_debug)
		syslog(ATMD_DEBUG, "Config [boardconfig]: reg12: 0x%08X.", reg);
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
		syslog(ATMD_DEBUG, "Config [boardconfig]: status register is 0x%X", reg);
	if(reg & 0x00000400) {
		syslog(ATMD_ERR, "Config [boardconfig]: TDC-GPX PLL not locked. Probably wrong resolution (refclkdiv = %d, hsdiv = %d).", this->refclkdiv, this->hsdiv);
		return -1;
	} else {
		if(enable_debug)
			syslog(ATMD_DEBUG, "Config [boardconfig]: TDC-GPX PLL locked. Using a resolution of %f ps.", this->time_bin);
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
			syslog(ATMD_ERR, "Config [set_window]: error setting time windows. A raw value is not allowed.");
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
			syslog(ATMD_ERR, "Config [set_tottime]: error setting measure time. A zero value is not valid.");
			return -1;
		}

		// If the time is not raw we check that the measure time is not less than the measure window
		if(!this->measure_time.is_raw()) {
			double time = this->measure_time.get_sec() * 1e6 + this->measure_time.get_usec();
			double window = this->measure_window.get_sec() * 1e6 + this->measure_window.get_usec();
			if(time < window) {
				syslog(ATMD_ERR, "Config [set_tottime]: error setting measure time. Window time cannot be greater than measure time.");
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
	The time windows is almost unlimited. We use a software 32 bit start counter,
	so in principle the measure window is limited to the number of start events
	that we can count. With a retriggering of 5us and a 32 bit counter the limit
	should be 5.96 hours.

	The measure time is only limited by the timeval structure taken
	as input by gettimeofday, theoretically giving up to 2^32 seconds of measure
	time, that is approx 136 years.
	*/

	// We check that the timings are set and that there are at least the start channel and one stop channel enabled
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
		// NOT IMPLEMENTED
		syslog(ATMD_ERR, "Measure [Start]: simulation mode enabled, but not implemeted.");
		return -1;
	}

	// Avoids memory swapping for this program
	if(mlockall(MCL_CURRENT|MCL_FUTURE)) {
		syslog(ATMD_ERR, "Measure [Start]: failed to lock memory (Error: \"%m\").");
		return -1;
	}

	// We set the process at maximum priority
	errno = 0; // reset of errno
	if(setpriority(PRIO_PROCESS, 0, -20) == -1) {
		if(errno != 0) {
			syslog(ATMD_ERR, "Measure [Start]: failed to set real time priority (Error: \"%m\").");
			munlockall();
			return -1;
		}
	}

	// MSB of start counter unmasked to intflag
	this->start_to_intflag = true;

	// We commit board configuration
	if(this->boardconfig()) {
		// Board config failed. PLL not locked.
		return -1;
	}


	// If BN host is configured, connect
	if(this->get_bnport()) {
		if(this->connect_bnhost()) {
			return -1;
		}
	}

	// Reset thread return value
	this->retval(0);

	// We start the measure in direct mode running the measure function in another thread
	this->set_status(ATMD_STATUS_STARTING);
	this->set_stop(false);

	int rval = rt_task_spawn(&main_task, "main_task", 0, 99, T_JOINABLE, direct_measure, (void*)this);
	if(rval) {
		syslog(ATMD_ERR, "Measure [Direct read mode]: error starting measuring thread with error \"%m\".");
		return -1;

	} else {
		syslog(ATMD_ERR, "Measure [Direct read mode]: successfully started main measuring thread.");
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

	if(this->collect_measure()) {
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

	if(this->collect_measure()) {
		// Error recollecting measurement thread
		return -1;

	} else {
		// Here we delete the last measure because we are aborting
		if(this->get_status() == ATMD_STATUS_ERROR) {
			if(this->retval() != ATMD_ERR_EMPTY && this->num_measures() > 0)
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
int ATMDboard::collect_measure() {

	// Join realtime task
	int rval = rt_task_join(&main_task);

	// Close bunch number socket
	if(this->_bnsock)
		this->close_bnhost();

	// If AUTORESTART mode is not enabled, set process priority back to normal
	if(this->autostart != ATMD_AUTOSTART_ENABLED) {
		errno = 0; // reset of errno
		if(setpriority(PRIO_PROCESS, 0, 0) == -1) {
			if(errno != 0) {
				syslog(ATMD_ERR, "Measure [Collect]: failed to set priority back to normal (Error: \"%m\").");
			} else {
				syslog(ATMD_INFO, "Measure [Collect]: set priority back to normal.");
			}
		} else {
			syslog(ATMD_INFO, "Measure [Collect]: set priority back to normal.");
		}
	}

	// Unlock memory
	if(munlockall()) {
		syslog(ATMD_ERR, "Measure [Collect]: failed to unlock system memory (Error: \"%m\").");
	}

	// Check error code from join
	if(rval) {
		//TODO error checking.
		syslog(ATMD_ERR, "Measure [Collect]: rt_task_join failed with error %d.", rval);
		this->set_status(ATMD_STATUS_IDLE);
		return -1;

	} else {
		syslog(ATMD_INFO, "Measure [Collect]: measurement thread terminated correctly. Return value was %d.", this->retval());
	}

	this->set_status(ATMD_STATUS_IDLE);
	return 0;
}


/* @fn ATMDboard::check_measuretime_exceeded(struct timeval& begin, struct timeval& now)
 * Check if the configured measure time has been exceeded.
 *
 * @param begin The timeval structure of the time in which the measure begun (as returned by gettimeofday)
 * @param now The timeval structure of the current time (as returned by gettimeofday)
 * @return Return true time has been exceeded, false otherwise.
 */
bool ATMDboard::check_measuretime_exceeded(RTIME begin, RTIME now) {

	if(now < begin) {
		// The begin time is in the future. Something bad happened. We log an error and return true,
		//so we are sure that we don't get stuck in an infinite loop.
		syslog(ATMD_ERR, "Measure [check_measure_time_exceeded]: begin time is after current time.");
		return true;
	}

	if(this->measure_time.is_raw())
		return false;

	// Measure time
	uint64_t conf_time = (uint64_t)this->measure_time.get_sec() * 1000000 + this->measure_time.get_usec();
	uint64_t win_time = (uint64_t)this->measure_window.get_sec() * 1000000 + this->measure_window.get_usec();

	// Actual time
	uint64_t actual_time = (uint64_t)(now - begin) / 1000;

	if(actual_time + win_time > conf_time)
		return true;
	else
		return false;
}


/* @fn ATMDboard::check_windowtime_excemeeded(struct timeval& begin, struct timeval& now)
 * Check if the configured window time has been exceeded.
 *
 * @param begin The timeval structure of the time in which the window begun (as returned by gettimeofday)
 * @param now The timeval structure of the current time (as returned by gettimeofday)
 * @return Return true time has been exceeded, false otherwise.
 */
bool ATMDboard::check_windowtime_exceeded(RTIME begin, RTIME now) {

	if(now < begin) {
		// The begin time is in the future. Something bad happened. We log an error and return true,
		//so we are sure that we don't get stuck in an infinite loop.
		syslog(ATMD_ERR, "Measure [check_windowtime_exceeded]: given begin time is after current time in the future.");
		return true;
	}

	// Window time
	uint64_t conf_time = (uint64_t)this->measure_window.get_sec() * 1000000 + this->measure_window.get_usec();

	// Actual time
	uint64_t actual_time = (uint64_t)(now - begin) / 1000;

	if(actual_time >= conf_time)
		return true;
	else
		return false;
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
int ATMDboard::add_measure(Measure* measure) {
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
		delete this->measures[measure_number];
		this->measures.erase(this->measures.begin()+measure_number);
		return 0;
	} else {
		return -1;
	}
}


/* @fn ATMDboard::save_measure(uint32_t measure_number, string filename, uint32_t format)
 * Save a measure object to a file in the specified format.
 *
 * @param measure_number The number of the measure that we want to save.
 * @param filename The filename including complete path.
 * @param format The file format that should be used to save the measure.
 * @return Return 0 on success, -1 on error.
 */
int ATMDboard::save_measure(uint32_t measure_number, string filename, uint32_t format) {
	fstream savefile;
	MatFile mat_savefile;

	// Check if the measure number is valid.
	if(measure_number < 0 || measure_number >= this->num_measures()) {
		syslog(ATMD_ERR, "Measure [save_measure]: trying to save a non existent measure.");
		return -1;
	}

	// For safety we remove all relative path syntax
	pcrecpp::RE("\\.\\.\\/").GlobalReplace("", &filename);
	if(filename[0] == '/')
	filename.erase(0,1);

	// Find 'user' uid
	int bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
	if (bufsize == -1)		// Value was indeterminate
		bufsize = 16384;	// Should be more than enough

	char * pwbuf = new char[bufsize];
	if(pwbuf == NULL) {
		syslog(ATMD_ERR, "Measure [save_measure]: cannot allocate buffer for getpwnam_r.");
		return -1;
	}

	uid_t uid = 0;
	struct passwd pwd;
	struct passwd *result;
	int rval = getpwnam_r("user", &pwd, pwbuf, bufsize, &result);
	if (result == NULL) {
		if(rval == 0)
			syslog(ATMD_WARN, "Measure [save_measure]: cannot find user 'user'.");
		else
			syslog(ATMD_WARN, "Measure [save_measure]: error retreiving 'user' UID (Error: %m).");
	} else {
		uid = pwd.pw_uid;
	}

	// For safety the supplied path is taken relative to /home/data
	string fullpath = "/home/data/";

	// Now we check that all path elements exists
	pcrecpp::StringPiece input(filename);
	pcrecpp::RE re("(\\w*)\\/");
	string dir;
	struct stat st;
	while (re.Consume(&input, &dir)) {
		fullpath.append(dir);
		fullpath.append("/");
		if(stat(fullpath.c_str(), &st) == -1) {
			if(errno == ENOENT) {
				// Path does not exist
				if(mkdir(fullpath.c_str(), S_IRWXU | S_IRWXG)) {
					syslog(ATMD_ERR, "Measure [save_measure]: error creating save path \"%s\" (Error: %m).", fullpath.c_str());
					return -1;
				}

				// Change owner to 'user'
				if(chown(fullpath.c_str(), uid, -1)) {
					syslog(ATMD_WARN, "Measure [save_measure]: error changing owner for director \"%s\" (Error %m).", fullpath.c_str());
				}

			} else {
				// Error
				syslog(ATMD_ERR, "Measure [save_measure]: error checking save path \"%s\" (Error: %m).", fullpath.c_str());
				return -1;
			}
		}
	}
	fullpath.append(input.as_string());

	// URL for FTP transfers
	string fullurl = "";
	if(format == ATMD_FORMAT_MATPS2_FTP || format == ATMD_FORMAT_MATPS2_ALL || format == ATMD_FORMAT_MATPS3_FTP || format == ATMD_FORMAT_MATPS3_ALL) {
		if(this->host == "") {
			syslog(ATMD_ERR, "Measure [save_measure]: requested to save a measure using FTP but hostname is not set.");
			return -1;
		}

		fullurl.append("ftp://");
		if(this->username != "") {
			fullurl.append(this->username);
			if(this->password != "") {
				fullurl.append(":");
				fullurl.append(this->password);
			}
			fullurl.append("@");
		}
		fullurl.append(this->host);
		fullurl.append("/");
		fullurl.append(filename);
	}

	if(format != ATMD_FORMAT_MATPS2_FTP && format != ATMD_FORMAT_MATPS3_FTP) {

		// Opening file
		switch(format) {
			case ATMD_FORMAT_BINPS:
			case ATMD_FORMAT_BINRAW:
				// Open in binary format
				savefile.open(fullpath.c_str(), fstream::out | fstream::trunc | fstream::binary);
				if(!savefile.is_open()) {
					syslog(ATMD_ERR, "Measure [save_measure]: error opening binary file \"%s\".", fullpath.c_str());
					return -1;
				}
				break;
				
			case ATMD_FORMAT_MATPS1:
			case ATMD_FORMAT_MATPS2:
			case ATMD_FORMAT_MATPS3:
			case ATMD_FORMAT_MATRAW:
			case ATMD_FORMAT_MATPS2_ALL:
			case ATMD_FORMAT_MATPS3_ALL:
				mat_savefile.open(fullpath);
				if(!mat_savefile.IsOpen()) {
					syslog(ATMD_ERR, "Measure [save_measure]: cannot open Matlab file %s.", fullpath.c_str());
					return -1;
				}
				break;

			case ATMD_FORMAT_RAW:
			case ATMD_FORMAT_PS:
			case ATMD_FORMAT_US:
			case ATMD_FORMAT_DEBUG:
				// Open in text format
				savefile.open(fullpath.c_str(), fstream::out | fstream::trunc);
				savefile.precision(15);
				if(!savefile.is_open()) {
					syslog(ATMD_ERR, "Measure [save_measure]: error opening text file \"%s\".", fullpath.c_str());
					return -1;
				}
				break;
		}

		syslog(ATMD_INFO, "Measure [save_measure]: saving measure %d to file \"%s\".", measure_number, fullpath.c_str());
	}


	StartData* current_start;
	uint32_t start_count = this->measures[measure_number]->count_starts();

	double stoptime;
	int8_t channel;
	uint32_t ref_count;

	// Matlab definitions
	MatVector<double> data("data", mxDOUBLE_CLASS);
	MatVector<uint32_t> bnumber("bnumber", mxUINT32_CLASS);
	MatVector<uint32_t> data_st("start", mxUINT32_CLASS);
	MatVector<int8_t> data_ch("channel", mxINT8_CLASS);
	MatVector<double> data_stop("stoptime", mxDOUBLE_CLASS);
	MatVector<uint32_t> measure_begin("measure_begin", mxUINT32_CLASS);
	MatVector<uint32_t> measure_time("measure_time", mxUINT32_CLASS);
	MatVector<uint32_t> stat_times("stat_times", mxUINT32_CLASS);

	uint32_t num_events = 0, ev_ind = 0;

	stringstream txtbuffer (stringstream::in);

	switch(format) {
		case ATMD_FORMAT_RAW:
		case ATMD_FORMAT_US:
		case ATMD_FORMAT_PS:
		case ATMD_FORMAT_DEBUG:
		default:

			// We make sure that the string stream is empty
			txtbuffer.str("");

			// We write an header
			if(format == ATMD_FORMAT_RAW)
				txtbuffer << "start\tchannel\tslope\trefcount\tstoptime" << endl;
			else if(format == ATMD_FORMAT_DEBUG)
				txtbuffer << "start\tstart_time\ttimefrombegin\tchannel\trefcount\tstoptime" << endl;
			else
				txtbuffer << "start\tchannel\tslope\tstoptime" << endl;

			// We cycle over all starts
			for(size_t i = 0; i < start_count; i++) {
				if(!(current_start = this->measures[measure_number]->get_start(i))) {
					syslog(ATMD_ERR, "Measure [save_measure]: trying to save a non existent start.");
					return -1;
				}

				// We cycle over all stops of current start
				for(size_t j = 0; j < current_start->count_stops(); j++) {
					current_start->get_channel(j, channel);

					if(format == ATMD_FORMAT_RAW) {
						current_start->get_rawstoptime(j, ref_count, stoptime);
						txtbuffer << i+1 << "\t" << (int32_t)channel << "\t" << ref_count << "\t" << stoptime << endl;

					} else if(format == ATMD_FORMAT_DEBUG) {
						current_start->get_rawstoptime(j, ref_count, stoptime);
						txtbuffer << i+1 << "\t" << current_start->get_time() << "\t" << current_start->get_timefrombegin() << "\t" << (int32_t)channel << "\t" << ref_count << "\t" << stoptime << endl;

					} else {
						current_start->get_stoptime(j, stoptime);
						txtbuffer << i+1 << "\t" << (int32_t)channel << "\t" << ((format == ATMD_FORMAT_US) ? (stoptime / 1e6) : stoptime) << endl;
					}
				}
			}

			// We write the string stream to the output file
			savefile << txtbuffer.str();
			break;


		case ATMD_FORMAT_BINPS:
		case ATMD_FORMAT_BINRAW:
			syslog(ATMD_ERR, "Measure [save_measure]: binary format is not yet implemented.");
			return -1;
			break;


		case ATMD_FORMAT_MATPS1: // Matlab format with all data in a single variable
		case ATMD_FORMAT_MATPS2: // Matlab format with separate variables for start, channel and stoptime
		case ATMD_FORMAT_MATPS2_FTP: // Same as previous but saved via FTP
		case ATMD_FORMAT_MATPS2_ALL: // Same as previous but saved both locally and via FTP
		case ATMD_FORMAT_MATPS3:
		case ATMD_FORMAT_MATPS3_FTP:
		case ATMD_FORMAT_MATPS3_ALL:

			/* First of all we should count all the events */
			num_events = 0;
			for(size_t i = 0; i < start_count; i++) {
				if(!(current_start = this->measures[measure_number]->get_start(i))) {
					syslog(ATMD_ERR, "Measure [save_measure]: trying to stat a non existent start.");
					return -1;
				}
				num_events += current_start->count_stops();
			}

			// Measure times
			measure_begin.resize(1,2);
			measure_begin(0,0) = (uint64_t)(this->measures[measure_number]->get_begin() / 1000) / 1000000;
			measure_begin(0,1) = (uint64_t)(this->measures[measure_number]->get_begin() / 1000) % 1000000;

			measure_time.resize(1,2);
			measure_time(0,0) = this->measures[measure_number]->get_time() / 1000000;
			measure_time(0,1) = this->measures[measure_number]->get_time() % 1000000;

			// Vector resizes
			if(format == ATMD_FORMAT_MATPS1) {
				data.resize(num_events,3);
			} else {
				data_st.resize(num_events,1);
				data_ch.resize(num_events,1);
				data_stop.resize(num_events,1);
			}

			if(this->measures[measure_number]->bnum_en())
				bnumber.resize(start_count,1);

			if(format == ATMD_FORMAT_MATPS3 || format == ATMD_FORMAT_MATPS3_FTP || format == ATMD_FORMAT_MATPS3_ALL)
				stat_times.resize(start_count,2);

			// Save data
			ev_ind = 0;
			for(size_t i = 0; i < start_count; i++) {
				if(!(current_start = this->measures[measure_number]->get_start(i))) {
					syslog(ATMD_ERR, "Measure [save_measure]: trying to save a non existent start.");
					return -1;
				}
				// Save data
				for(size_t j = 0; j < current_start->count_stops(); j++) {
					current_start->get_channel(j, channel);
					current_start->get_stoptime(j, stoptime);

					if(format == ATMD_FORMAT_MATPS1) {
						data(ev_ind, 0) = double(i+1);
						data(ev_ind, 1) = double(channel);
						data(ev_ind, 2) = stoptime;
					} else {
						data_st(ev_ind,0) = i+1;
						data_ch(ev_ind,0) = channel;
						data_stop(ev_ind,0) = stoptime;
					}
					ev_ind++;
				}

				// If needed save bunch number
				if(this->measures[measure_number]->bnum_en())
					bnumber(i,0) = current_start->get_bnum();

				// Save start times
				if(format == ATMD_FORMAT_MATPS3 || format == ATMD_FORMAT_MATPS3_FTP || format == ATMD_FORMAT_MATPS3_ALL) {
					stat_times(i,0) = (uint32_t)( current_start->get_timefrombegin() / 1000 );
					stat_times(i,1) = (uint32_t)( current_start->get_time() );
				}
			}


			if(format != ATMD_FORMAT_MATPS2_FTP && format != ATMD_FORMAT_MATPS3_FTP) {
				// Write vectors to file
				measure_begin.write(mat_savefile);
				measure_time.write(mat_savefile);

				if(format == ATMD_FORMAT_MATPS1) {
					data.write(mat_savefile);

				} else {
					data_st.write(mat_savefile);
					data_ch.write(mat_savefile);
					data_stop.write(mat_savefile);
				}

				if(this->measures[measure_number]->bnum_en())
					bnumber.write(mat_savefile);

				if(format == ATMD_FORMAT_MATPS3 || format == ATMD_FORMAT_MATPS3_ALL)
					stat_times.write(mat_savefile);
			}


			if(format == ATMD_FORMAT_MATPS2_FTP || format == ATMD_FORMAT_MATPS2_ALL || format == ATMD_FORMAT_MATPS3_FTP || format == ATMD_FORMAT_MATPS3_ALL) {

				MatObj matobj;

				matobj.add_obj((MatMatrix*) &measure_begin);
				matobj.add_obj((MatMatrix*) &measure_time);
				matobj.add_obj((MatMatrix*) &data_st);
				matobj.add_obj((MatMatrix*) &data_ch);
				matobj.add_obj((MatMatrix*) &data_stop);
				if(this->measures[measure_number]->bnum_en())
					matobj.add_obj((MatMatrix*) &bnumber);
				if(format == ATMD_FORMAT_MATPS3 || format == ATMD_FORMAT_MATPS3_FTP)
					matobj.add_obj((MatMatrix*) &stat_times);

				// Configure FTP in binary mode
				struct curl_slist *headerlist = NULL;
				headerlist = curl_slist_append(headerlist, "TYPE I");
				curl_easy_setopt(this->easy_handle, CURLOPT_QUOTE, headerlist);
				curl_easy_setopt(this->easy_handle, CURLOPT_FTP_CREATE_MISSING_DIRS, CURLFTP_CREATE_DIR_RETRY);

				// Setup of CURL options
				curl_easy_setopt(this->easy_handle, CURLOPT_READFUNCTION, curl_read);
				curl_easy_setopt(this->easy_handle, CURLOPT_UPLOAD, 1L);
				curl_easy_setopt(this->easy_handle, CURLOPT_URL, fullurl.c_str());
				curl_easy_setopt(this->easy_handle, CURLOPT_READDATA, (void*) &matobj);
				curl_easy_setopt(this->easy_handle, CURLOPT_INFILESIZE_LARGE, (curl_off_t)matobj.total_size());

				syslog(ATMD_INFO, "Measure [save_measure]: remotely saving measurement to \"%s/%s\".", this->host.c_str(), filename.c_str());

				if(enable_debug)
					syslog(ATMD_DEBUG, "Measure [save_measure]: full url: %s.", fullurl.c_str());

				// Start network transfer
				struct timeval t_begin, t_end;
				gettimeofday(&t_begin, NULL);

				if(curl_easy_perform(this->easy_handle)) {
					syslog(ATMD_ERR, "Measure [save_measure]: failed to transfer file with libcurl with error \"%s\".", this->curl_error);
					return -1;
				}

				// We measure elapsed time for statistic purposes
				gettimeofday(&t_end, NULL);
				syslog(ATMD_INFO, "Measure [save_measure]: remote save performed in %fs.", (double)(t_end.tv_sec - t_begin.tv_sec) + (double)(t_end.tv_usec - t_begin.tv_usec) / 1e6);
			}
			break;

		case ATMD_FORMAT_MATRAW: /* Raw Matlab format with separate variables for start, channel, retriger count and stoptime */
			syslog(ATMD_ERR, "Measure [save_measure]: raw MAT format is not yet implemented.");
			return -1;
			break;
	}


	switch(format) {
		case ATMD_FORMAT_BINPS:
		case ATMD_FORMAT_BINRAW:
		case ATMD_FORMAT_RAW:
		case ATMD_FORMAT_PS:
		case ATMD_FORMAT_US:
		case ATMD_FORMAT_DEBUG:
			savefile.close();
			break;

		case ATMD_FORMAT_MATPS1:
		case ATMD_FORMAT_MATPS2:
		case ATMD_FORMAT_MATPS3:
		case ATMD_FORMAT_MATRAW:
		case ATMD_FORMAT_MATPS2_ALL:
		case ATMD_FORMAT_MATPS3_ALL:
			mat_savefile.close();
			break;
	}

	// Change owner to file
	if(format != ATMD_FORMAT_MATPS2_FTP && format != ATMD_FORMAT_MATPS3_FTP)
		if(chown(fullpath.c_str(), uid, -1))
			syslog(ATMD_ERR, "Measure [save_measure]: cannot change owner of file \"%s\" (Error: %m).", fullpath.c_str());

	return 0;
}


/* @fn curl_read(void *ptr, size_t size, size_t count, void *data)
 * Read callback for curl network transfers
 *
 * @param ptr Pointer to the curl output buffer
 * @param size Size of the data block
 * @param count Number of data block to copy
 * @param data Pointer to the input data structure
 * @return Return the number of bytes copied to ptr buffer
 */
static size_t curl_read(void *ptr, size_t size, size_t count, void *data) {
	// Restore pointer to buffer object
	struct MatObj* matobj = (MatObj*)data;

	size_t bn = matobj->get_bytes((uint8_t*)ptr, size*count);
	// Return number of byte copied to buffer
	return bn;
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
		syslog(ATMD_ERR, "Measure [stat_measure]: trying to get statistics about a non existent measure.");
		return -1;
	}

	starts = this->measures[measure_number]->count_starts();
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
		syslog(ATMD_ERR, "Measure [stat_stops]: trying to get statistics about a non existent measure.");
		return -1;
	}

	vector<uint32_t> stops_ch(9,0);
	StartData* current_start;

	// We cycle over all starts
	for(size_t i = 0; i < this->measures[measure_number]->count_starts(); i++) {
		if(!(current_start = this->measures[measure_number]->get_start(i))) {
			syslog(ATMD_ERR, "Measure [stat_stops]: trying to stat a non existent start.");
			return -1;
		}

		// We save the measure time
		//stops_ch[0] = (uint32_t)(current_start->get_time() / (uint64_t)(((ATMD_AUTORETRIG + 1) * 25) / 1000));
		stops_ch[0] = (uint32_t)(current_start->get_time());

		// We cycle over all stops of current start
		for(size_t j = 0; j < current_start->count_stops(); j++) {
			int8_t ch;
			current_start->get_channel(j, ch);
			stops_ch[(ch > 0) ? ch : -ch]++;
		}

		// We add the vector with counts to the output
		stop_counts.push_back(stops_ch);

		// We reset the counts
		for(size_t j = 0; j < 9; j++)
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
		syslog(ATMD_ERR, "Measure [stat_stops]: trying to get statistics about a non existent measure.");
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
	StartData* current_start;

	// We cycle over all starts
	for(size_t i = 0; i < this->measures[measure_number]->count_starts(); i++) {
		if(!(current_start = this->measures[measure_number]->get_start(i))) {
			syslog(ATMD_ERR, "Measure [stat_stops]: trying to stat a non existent start.");
			return -1;
		}

		// We save the measure time
		stops_ch[0] = (uint32_t)(current_start->get_time() / (uint64_t)(((ATMD_AUTORETRIG + 1) * 25) / 1000));

		// We cycle over all stops of current start
		for(size_t j = 0; j < current_start->count_stops(); j++) {
			int8_t ch;
			double stoptime;
			current_start->get_channel(j, ch);
			current_start->get_stoptime(j, stoptime);

			if(stoptime > window_start.get_ps() && stoptime < window_start.get_ps()+window_amplitude.get_ps())
				stops_ch[(ch > 0) ? ch : -ch]++;
		}

		// We add the vector with counts to the output
		stop_counts.push_back(stops_ch);

		// We reset the counts
		for(size_t j = 0; j < 9; j++)
			stops_ch[j] = 0;
	}

	return 0;
}


/* @fn ATMDboard::measure_autorestart(int err_status)
 * Manage the autorestart process. In detail it will save the previous measure, then it will delete it,
 * recollect thread status and restart another measure.
 *
 * @return Return 0 on success, -1 on error.
 */
int ATMDboard::measure_autorestart(int err_status) {
	// We should get some statistics about lost time so we measure the time we need to restart a measurement
	struct timeval t_begin, t_end;

	// We initialize to zero the time structures so if the gettimeofday fail we do not get some absurd time but instead zero.
	memset(&t_begin, 0x00, sizeof(struct timeval));
	memset(&t_end, 0x00, sizeof(struct timeval));
	gettimeofday(&t_begin, NULL);

	if(err_status == ATMD_ERR_NONE) {
		/* First thing to do is to save the last measurement */
		uint32_t measure_id = this->measures.size()-1; // We get the last measurement id

		// We format the filename
		stringstream file_number(stringstream::out);
		file_number.width(4);
		file_number.fill('0');
		file_number << this->measure_counter;

		string filename = this->fileprefix + "_" + file_number.str();

		if(enable_debug)
			syslog(ATMD_DEBUG, "Measure [measure_autorestart]: autosaving measure to file \"%s\".", filename.c_str());

		switch(this->auto_format) {
			case ATMD_FORMAT_RAW:
			case ATMD_FORMAT_PS:
			case ATMD_FORMAT_US:
			case ATMD_FORMAT_DEBUG:
			default:
				filename += ".txt";
				break;

			case ATMD_FORMAT_BINPS:
			case ATMD_FORMAT_BINRAW:
				filename += ".dat";
				break;

			case ATMD_FORMAT_MATPS1:
			case ATMD_FORMAT_MATPS2:
			case ATMD_FORMAT_MATRAW:
			case ATMD_FORMAT_MATPS2_FTP:
			case ATMD_FORMAT_MATPS2_ALL:
			case ATMD_FORMAT_MATPS3:
			case ATMD_FORMAT_MATPS3_FTP:
			case ATMD_FORMAT_MATPS3_ALL:
				filename += ".mat";
				break;
		}

		if(this->save_measure(measure_id, filename, this->auto_format)) {
			// Measure autosave failed
			syslog(ATMD_ERR, "Measure [measure_autorestart]: failed to autosave measure %u to file \"%s\" with format %u.", measure_id, filename.c_str(), this->auto_format);

			// We set autostart to disabled because we are aborting and board status to error
			this->autostart = ATMD_AUTOSTART_DISABLED;
			this->board_status = ATMD_STATUS_ERROR;
			return -1;
		}

		// We increment the measure counter so we do not overwrite previous measurements
		this->measure_counter++;
		this->autostart_counter++;

		// Then we delete from memory the measure we just saved
		if(this->del_measure(measure_id)) {
			// Measure deletion failed
			syslog(ATMD_ERR, "Measure [measure_autorestart]: failed to delete measure %u (total number of measurements %u)", measure_id, (uint32_t)this->measures.size());

			// We set autostart to disabled because we are aborting and board status to error
			this->autostart = ATMD_AUTOSTART_DISABLED;
			this->board_status = ATMD_STATUS_ERROR;
			return -1;
		}

		// Then we recollect the previous thread status, but we should do this only if the status is ATMD_STATUS_FINISHED
		if(this->board_status == ATMD_STATUS_FINISHED) {
			if(this->collect_measure()) {
				// Preavious thread recollection failed
				syslog(ATMD_ERR, "Measure [measure_autorestart]: failed to recollect previous thread status.");

				// We set autostart to disabled because we are aborting and board status to error
				this->autostart = ATMD_AUTOSTART_DISABLED;
				this->board_status = ATMD_STATUS_ERROR;
				return -1;
			}
			this->board_status = ATMD_STATUS_IDLE;
		}
	}

	// Check if we have reached autostart range
	if(!this->auto_range.is_zero()) {
		if(this->auto_range.is_raw()) {
			if(this->auto_range.get_raw() < this->autostart_counter)
				this->autostart = ATMD_AUTOSTART_STOP;

		} else {
			struct timeval autostart_end;
			gettimeofday(&autostart_end, NULL);
			if(autostart_end.tv_sec - this->autostart_begin.tv_sec > this->auto_range.get_sec())
				this->autostart = ATMD_AUTOSTART_STOP;
		}
	}

	if(this->autostart == ATMD_AUTOSTART_STOP) {
		// If autostart status is set to STOP this is the time to disable it completely
		this->autostart = ATMD_AUTOSTART_DISABLED;

	} else {
		// Now we can start the next measurement
		if(this->start_measure()) {
			// Measure starting failed
			syslog(ATMD_ERR, "Measure [measure_autorestart]: failed to start a new measurement.");

			// We set autostart to disabled because we are aborting and board status to error
			this->autostart = ATMD_AUTOSTART_DISABLED;
			this->board_status = ATMD_STATUS_ERROR;
			return -1;
		}
	}

	gettimeofday(&t_end, NULL);
	syslog(ATMD_INFO, "Measure [measure_autorestart]: restart done in %fs.", (double)(t_end.tv_sec - t_begin.tv_sec) + (double)(t_end.tv_usec - t_begin.tv_usec) / 1e6);

	return 0;
}


/* @fn ATMDboard::set_autorange(string& timestr)
 * Set the autostart range, checking that the supplied time is not less than measure time.
 *
 * @param timestr A string representing the autostart range.
 * @return Return 0 on success, -1 on error.
 */
int ATMDboard::set_autorange(string& timestr) {
	if(this->auto_range.set(timestr)) {
		return -1;
	}

	if(this->measure_time.is_zero()) {
		syslog(ATMD_ERR, "Config [set_autorange]: error setting autorestart range. The measure time is not set.");
		return -1;
	}

	if(!this->auto_range.is_raw() && !this->auto_range.is_zero()) {
		// We check that the saved time make sense and that is greater than measure time
		uint32_t tot_time = this->auto_range.get_sec() + uint32_t(round(double(this->auto_range.get_usec())/1e6));
		uint32_t meas_time = this->measure_time.get_sec() + uint32_t(round(double(this->measure_time.get_usec())/1e6));
		if(tot_time <= meas_time) {
			syslog(ATMD_ERR, "Config [set_autorange]: error setting autorestart range. The supplied time (%ds) is not greater than measure time (%ds).", tot_time, meas_time);
			return -1;
		}
		stringstream autotime(stringstream::out);
		autotime << tot_time << "s";
		if(enable_debug)
			syslog(ATMD_DEBUG, "Config [set_autorange]: rounding autostart range to \"%s\".", autotime.str().c_str());
		this->auto_range.set(autotime.str());
	}
	return 0;
}


/* @fn ATMDboard::connect_bnhost()
 * Crete the UDP socket for bunch number host
 *
 * @return Return 0 on success, -1 on error.
 */
int ATMDboard::connect_bnhost() {

	// Create socket
	if( (this->_bnsock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1 ) {
		syslog(ATMD_ERR, "Measure [connect_bnhost]: failed to create UDP socket with error (Error: %m).");
		return -1;
	}

	// Set the socket as non-blocking
	if(fcntl(this->_bnsock, F_SETFL, O_NONBLOCK) == -1) {
		// Failed to set socket flags
		syslog(ATMD_ERR, "Measure [connect_bnhost]: failed to set socket flag O_NONBLOCK (Error: %m).");
		close(this->_bnsock);
		this->_bnsock = 0;
		return -1;
	}

	// Setup address
	memset(&(this->_bn_address), 0, sizeof(struct sockaddr_in));
	this->_bn_address.sin_family = AF_INET;
	this->_bn_address.sin_port = htons(this->_bnport);
	if( !inet_aton(this->_bnhost.c_str(), &(this->_bn_address.sin_addr)) ) {
		syslog(ATMD_ERR, "Measure [connect_bnhost]: failed to convert ip address \"%s\".", this->_bnhost.c_str());
		close(this->_bnsock);
		this->_bnsock = 0;
		return -1;
	}

	return 0;
}


/* @fn ATMDboard::send_bn(uint32_t bn)
 * Send the given bunch number through the UDP socket
 *
 * @return Return 0 on success, -1 on error.
 */
int ATMDboard::send_bn(uint32_t bn, uint64_t ts) {
	char buffer[256];
	sprintf(buffer, "BN: %u:%lu", bn, ts);

	int rval = 0;
	if( (rval = sendto(this->_bnsock, buffer, strlen(buffer), 0, (struct sockaddr*)&(this->_bn_address), sizeof(struct sockaddr_in))) == -1) {
		syslog(ATMD_ERR, "Measure [send_bn]: error sending bunch number (error: \"%m\")");
		return -1;
	}
	if((size_t)rval != strlen(buffer))
		syslog(ATMD_WARN, "Measure [send_bn]: datagram partially sent.");
	return 0;
}
