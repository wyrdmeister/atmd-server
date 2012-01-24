/*
 * ATMD Server version 3.0
 *
 * ATMD Agent - Hardware functions header
 *
 * Copyright (C) Michele Devetta 2012 <michele.devetta@unimi.it>
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

#ifndef ATMD_AGENT_HW_H
#define ATMD_AGENT_HW_H

// General
#include <stdint.h>

// HW specific
extern "C" {
  #include <pci/pci.h>
}
#include <sys/io.h>

// Xenomai
#include <rtdk.h>
#include <native/task.h>

// ATMD specific
#include "common.h"


/*
 * Main ATMD-GPX board management class
 */
class ATMDboard {

public:
  ATMDboard() : base_address(0), _status(ATMD_STATUS_IDLE), _stop(false) { this->reset_config(); };
  ~ATMDboard() {};

  // Initialize board
  int init(uint16_t address);

  // Reset board configuration
  void reset_config();

  // Interfaces for managing start channel
  void start_rising(bool enable) { this->en_start_rising = enable; };
  void start_falling(bool enable) { this->en_start_falling = enable; };
  bool start_rising() { return this->en_start_rising; };
  bool start_falling() { return this->en_start_falling; };

  // Interfaces for managing channels. Disable both rising and falling to disable a channel
  void ch_rising(int number, bool enable) { this->en_rising[number-1] = enable; };
  void ch_falling(int number, bool enable) { this->en_falling[number-1] = enable; };
  bool ch_rising(int number) { return this->en_rising[number-1]; };
  bool ch_falling(int number) { return this->en_falling[number-1]; };
  void set_rising_mask(uint8_t mask) {
    for(size_t i = 0; i < 8; i++)
      if( mask & (0x1 << i) )
        en_rising[i] = true;
  };
  void set_falling_mask(uint8_t mask) {
    for(size_t i = 0; i < 8; i++)
      if( mask & (0x1 << i) )
        en_falling[i] = true;
  };
  uint8_t get_rising_mask() {
    uint8_t rising = 0x0;
    for(size_t i = 0; i < 8; i++)
      if(this->en_rising[i])
        rising = rising | (0x1 << i);
    return rising;
  };
  uint8_t get_falling_mask() {
    uint8_t falling = 0x0;
    for(size_t i = 0; i < 8; i++)
      if(this->en_falling[i])
        falling = falling | (0x1 << i);
    return falling;
  };

  // Interfaces for managing TDC start offset
  void start_offset(uint32_t offset) { this->_start_offset = offset; };
  uint32_t start_offset() { return this->_start_offset; };

  // Intefaces for managing TDC resolution
  void set_resolution(uint16_t refclk, uint16_t hs) { this->refclkdiv = refclk; this->hsdiv = hs; };
  void get_resolution(uint16_t &refclk, uint16_t &hs) { refclk = this->refclkdiv; hs = this->hsdiv; };

  // Perform a full board reset
  void reset() {
    syslog(ATMD_INFO, "Board [reset]: Performing ATMD-GPX board reset.");
    this->mb_config(0x0101);    // Board total reset
    rt_task_sleep(1000);        // Sleep for 1 us
    this->mb_config(0x0008);    // Disable inputs by hardware
  };

  // Perform a TDC-GPX master reset (keeps configuration data)
  void master_reset();

  // Commit configuration to board
  int config();

  // Utility to get the motherboard status
  uint16_t mb_status() { return inw(this->base_address+0x8); }

  // Write to the board configuration register
  void mb_config(uint16_t reg) { outw(reg, this->base_address+0xC); };

  // Utility function to set the direct read address
  void set_dra(uint16_t address) { outw(address, this->base_address+0x4); };

  // Utility function to direct read data from TDC-GPX
  uint32_t read_dra(void) { return inl(this->base_address); };

  // Static function to search for ATMD-GPX boards on PCI bus
  static int search_board(uint16_t *addresses, size_t addr_len);

  // Manage status
  void status(int val) { _status = val; };
  int status()const { return _status; };

  // Stop measure
  void stop(bool val) { _stop = val; };
  bool stop()const { return _stop; };

private:
  // Utility function to write to board configuration registers
  void write_register(uint32_t value) {
    outw((uint16_t)(value & 0x0000FFFF), this->base_address);
    outw((uint16_t)((value & 0xFFFF0000) >> 16), this->base_address+0x2);
  }

private:
  // Board base address
  uint16_t base_address;

  // <<Channel configuration>>
  bool en_start_rising;     // Enable start rising edge
  bool en_start_falling;    // Enable start falling edge
  bool en_rising[8];        // Enable sensitivity on rising edge
  bool en_falling[8];       // Enable sensitivity on falling edge
  bool stop_dis_start;      // Disable stop channels before start pulse
  bool start_dis_start;     // Disable start channel after the first start

  // <<Resolution configuration>>
  uint16_t refclkdiv;       // Reference clock divider
  uint16_t hsdiv;           // High speed divider

  // Start autoretrigger
  uint8_t start_timer;

  // Start offset added to all timestamp for correct chip ALU calculations
  uint32_t _start_offset;

  // Mtimer configuration. Used in direct read to check if a start arrived.
  bool en_mtimer;
  uint16_t mtimer;

  // Start counter msb to intflag
  bool start_to_intflag;

  // Board status
  int _status;

  // Stop flag
  bool _stop;
};

#endif
