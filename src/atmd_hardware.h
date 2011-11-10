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

#ifndef ATMD_HARDWARE_H
#define ATMD_HARDWARE_H

extern "C" {
	#include <pci/pci.h>
}
#include <sys/io.h>
#include "common.h"
#include "atmd_timings.h"
#include "atmd_measure.h"

// Matlab file library
#include "MatFile.h"


class ATMDboard {

	public:
	ATMDboard();
	~ATMDboard() { curl_easy_cleanup(this->easy_handle); };

	// Initialize board
	int init(bool simulate);

	// Reset board configuration
	void reset_config();

	// Tell if we are in simulation mode
	bool simulation() { return (this->base_address == 0x0); };

	// Interfaces for managing start channel
	void enable_start_rising(bool enable) { this->en_start_rising = enable; };
	void enable_start_falling(bool enable) { this->en_start_falling = enable; };
	bool get_start_rising() { return this->en_start_rising; };
	bool get_start_falling() { return this->en_start_falling; };

	// Interfaces for managing channels. Disable both rising and falling to disable a channel
	void enable_rising(int number, bool enable) { this->en_rising[number-1] = enable; };
	void enable_falling(int number, bool enable) { this->en_falling[number-1] = enable; };
	uint8_t get_rising_mask();
	uint8_t get_falling_mask();

	// Interfaces for managing TDC start offset
	void set_start_offset(uint32_t offset) { this->start_offset = offset; };
	uint32_t get_start_offset() { return this->start_offset; };

	// Intefaces for managing TDC resolution
	void set_resolution(uint16_t refclk, uint16_t hs);
	void get_resolution(uint16_t &refclk, uint16_t &hs) { refclk = this->refclkdiv; hs = this->hsdiv; };
	double get_timebin() { return this->time_bin; };

	// Intefaces for managing measure timings
	int set_window(string timestr);
	Timings& get_window() { return this->measure_window; };
	int set_tottime(string timestr);
	Timings& get_tottime() { return this->measure_time; };

	bool check_measuretime_exceeded(RTIME begin, RTIME now);
	bool check_windowtime_exceeded(RTIME begin, RTIME now);

	// Maximum number of events per start
	uint32_t get_max_ev() { return max_ev; }
	void set_max_ev(uint32_t val) { this->max_ev = val; };

	// Start a new measure
	int start_measure();

	// Stop a measure (saving data)
	int stop_measure();

	// Abort a measure (discarding data)
	int abort_measure();

	// Check the return status of the measurement thread
	int collect_measure();


	// MEASURE MANAGEMENT INTERFACE
	// Add a measure
	int add_measure(Measure* measure);

	// Delete a measure
	int del_measure(uint32_t measure_number);

	// Clear all measures
	void clear_measures() {
		for(uint32_t i = 0; i < this->measures.size(); i++)
			delete measures[i];
		this->measures.clear();
	};

	// Return the number of measures saved
	size_t num_measures() { return this->measures.size(); };

	// Save the requested measure object to a file
	int save_measure(uint32_t measure_number, string filename, uint32_t format);

	// Get measure statistics
	int stat_measure(uint32_t measure_number, uint32_t& starts);

	// Get statistics about stops in a measure
	int stat_stops(uint32_t measure_number, vector< vector<uint32_t> >& stop_counts);
	int stat_stops(uint32_t measure_number, vector< vector<uint32_t> >& stop_counts, string win_start, string win_ampl);

	// Main thread return value
	int retval() { return this->_retval; };
	void retval(int val) { this->_retval = val; };

	// Board status interface
	void set_status(int status) { this->board_status = status; };
	int get_status() { return this->board_status; };

	// Board autostart interface
	void set_autostart(int status) { this->autostart = status; };
	int get_autostart() { return this->autostart; };
	void set_prefix(string& prefix) { this->fileprefix = prefix; this->measure_counter = 0; };
	string& get_prefix() { return this->fileprefix; };
	int set_autorange(string& timestr);
	string get_autorange() { return this->auto_range.get(); };
	void set_autoformat(uint32_t format) { this->auto_format = format; };
	uint32_t get_autoformat() { return this->auto_format; };
	int reset_autostart_counters() {
		this->autostart_counter = 0;
		memset(&(this->autostart_begin), 0x00, sizeof(struct timeval));
		return gettimeofday(&(this->autostart_begin), NULL);
	};

	// Function that manage the complete autorestart process
	int measure_autorestart(int err_status);

	// FTP save interface
	void set_host(string& address) { this->host = address; };
	void set_user(string& user) { this->username = user; };
	void set_password(string& psw) { this->password = psw; };
	string& get_host() { return this->host; };
	string& get_user() { return this->username; };
	bool psw_set() { return (this->password != ""); };

	// Interface to stop a measure
	void set_stop(bool stop) { this->measure_stop = stop; };
	bool get_stop() { return this->measure_stop; };


	// These should be public to be called from the thread (maybe...)
	// Perform a board reset
	void reset();

	// Perform a TDC-GPX master reset (keeps configuration data)
	void master_reset();

	// Utility to get the motherboard status
	uint16_t mb_status() { return inw(this->base_address+0x8); }

	// Write to the board configuration register
	void mb_config(uint16_t reg) { outw(reg, this->base_address+0xC); };

	// Utility function to set the direct read address
	void set_dra(uint16_t address) { outw(address, this->base_address+0x4); };

	// Utility function to direct read data from TDC-GPX
	uint32_t read_dra(void) { return inl(this->base_address); };

	// Utility to read motherboard fifo in burst mode
	uint32_t mb_readfifo() { return (inw(this->base_address+0xA) + (inw(this->base_address+0xC) << 16)); };

	// Interface for the bunch number
	void set_bnhost(string &host) { this->_bnhost = host; };
	void set_bnport(uint32_t port) { this->_bnport = port; };
	string get_bnhost() { return this->_bnhost; };
	uint16_t get_bnport() { return this->_bnport; };
	bool bn_enabled() { if(this->_bnsock) return true; else return false; };
	int connect_bnhost();
	void close_bnhost() { close(this->_bnsock); this->_bnsock = 0; };
	int send_bn(uint32_t bn, uint64_t ts);


	private:
	// CURL easy handle
	CURL* easy_handle;
	char curl_error[CURL_ERROR_SIZE];
	string host;
	string username;
	string password;

	// Board base address
	uint16_t base_address;

	// <<Channel configuration>>
	// Enable start rising edge
	bool en_start_rising;
	// Enable start falling edge
	bool en_start_falling;
	// Enable sensitivity on rising edge
	bool en_rising[8];
	// Enable sensitivity on falling edge
	bool en_falling[8];
	// Disable stop channels before start pulse
	bool stop_dis_start;
	// Disable start channel after the first start
	bool start_dis_start;

	// <<Resolution configuration>>
	// Reference clock divider
	uint16_t refclkdiv;
	// High speed divider
	uint16_t hsdiv;
	// Time bin in ps
	double time_bin;

	// Start autoretrigger
	uint8_t start_timer;

	// Start offset added to all timestamp for correct chip ALU calculations
	uint32_t start_offset;

	// Mtimer configuration. Used in direct read to check if a start arrived.
	bool en_mtimer;
	uint16_t mtimer;

	// Max number of events per start
	uint32_t max_ev;

	// Start counter msb to intflag
	bool start_to_intflag;

	// Measure times
	Timings measure_time;
	Timings measure_window;

	// Measure data
	vector<Measure*> measures;

	// Measure thread return value
	int _retval;

	// Board status variables
	int board_status;
	bool measure_stop;

	// Autorestart variables
	int autostart;
	string fileprefix;
	Timings auto_range;
	uint32_t auto_format;
	uint32_t measure_counter;
	uint32_t autostart_counter;
	struct timeval autostart_begin;

	// Private functions

	// Send configuration to board
	int boardconfig();

	// Private function to search for board base address
	int search_board();

	// Utility function to write board configuration registers
	void write_register(uint32_t value);

	// Host for bunch number
	string _bnhost;
	uint16_t _bnport;
	struct sockaddr_in _bn_address;
	int _bnsock;
};


// Callback for curl transfers
static size_t curl_read(void *ptr, size_t size, size_t count, void *data);


#endif
