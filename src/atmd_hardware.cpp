/*
 * ATMD Server version 3.0
 *
 * ATMD Agent - Hardware functions
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

#ifdef DEBUG
extern bool enable_debug;
#endif

#include "atmd_hardware.h"


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
  this->refclkdiv = ATMD_DEF_REFCLK;
  this->hsdiv = ATMD_DEF_HSDIV;

  // Set timestamps offset
  this->_start_offset = ATMD_DEF_STARTOFFSET;

  // Set mtimer
  this->en_mtimer = true;
  this->mtimer = 0;

  // Unmask start counter msb to intflag
  this->start_to_intflag = true;

  // Reset status
  _status = ATMD_STATUS_IDLE;
}


/* @fn ATMDboard::init()
 * Initialize the board.
 *
 * @param address Board address
 * @return Retrun 0 on success, -1 on error.
 *
 * NOTE: this function is not safe to be called in real-time domain.
 */
int ATMDboard::init(uint16_t address) {
  if(inw(address + 0x4) == 0x8000) {
    // ATMD-GPX Found!
    this->base_address = address;
    return 0;

  } else {
    syslog(ATMD_CRIT, "Board [init]: the supplied board address is invalid (addr = 0x%04X).", address);
    return -1;
  }
}


/* @fn ATMDboard::search_board(uint16_t *addresses, size_t addr_len)
 * Scan the PCI bus looking for the ATMD-GPX boards. The found board hardware addresses
 * are saved into the array 'addresses' passed as pointer.
 *
 * @param addresses Pointer to an array of type uint16_t.
 * @param addr_len Length of the addresses array.
 * @return Retrun the number of found boards on success, -1 on error.
 *
 * NOTE: this function is not safe to be called in real-time domain.
 */
int ATMDboard::search_board(uint16_t *addresses, size_t addr_len) {

  // Cleanup addresses
  for(size_t i = 0; i < addr_len; i++)
    addresses[i] = 0x0000;

  // Getting I/O privileges
  if(iopl(3)) {
    syslog(ATMD_CRIT, "Board [search_board]: failed to get direct I/O privileges (Error: %m).");
    return -1;
  } else {
#ifdef DEBUG
    if(enable_debug)
      syslog(ATMD_DEBUG, "Board [search_board]: direct I/O access privileges correctly configured.");
#endif
  }

  // Begin ATMD-GPX board search
  // Initialize PCI structs
  struct pci_access *pci_acc;
  struct pci_dev *pci_dev;
  pci_acc = pci_alloc();
  pci_init(pci_acc);
  pci_scan_bus(pci_acc);
  uint16_t curr_addr;
  size_t board_index = 0;

  // Scan PCI bus checking vendor and device ids searching for ATMD-GPX
  for(pci_dev=pci_acc->devices; pci_dev; pci_dev=pci_dev->next) {
    pci_fill_info(pci_dev, PCI_FILL_IDENT | PCI_FILL_BASES);

    if(pci_dev->vendor_id == 0x10b5 && pci_dev->device_id == 0x9050) {

      // If ids match, check all addresses to find the correct one...
      for(size_t i = 0; i<6; i++) {
        curr_addr = (uint16_t)(pci_dev->base_addr[i] & PCI_ADDR_IO_MASK);
        if(curr_addr) {
#ifdef DEBUG
          if(enable_debug)
            syslog(ATMD_INFO, "Board [search_board]: Checking hardware address 0x%04x.", curr_addr);
#endif

          // We check register at offset 0x4 from base address for module identification value
          if(inw(curr_addr + 0x4) == 0x8000) {
            // ATMD-GPX Found!
            board_index++;
            if(board_index <= addr_len) {
              addresses[board_index-1] = curr_addr;
              syslog(ATMD_INFO, "Board [search_board]: found ATMD-GPX board at address set to 0x%04X.", addresses[board_index-1]);
            } else {
              syslog(ATMD_INFO, "Board [search_board]: found ATMD-GPX board at address set to 0x%04X. The board will not be used.", addresses[board_index-1]);
            }
            break;
          }
        }
      }
    }
  }
  pci_cleanup(pci_acc);

  if(board_index == 0) {
    syslog(ATMD_CRIT, "Board [search_board]: no ATMD-GPX board not found.");
    return -1;
  }
  return board_index;
}


/* @fn ATMDboard::master_reset()
 * Perform a ATMD-GPX board total reset.
 *
 * NOTE: this function can be called only by a Xenomai real-time task.
 */
void ATMDboard::master_reset() {
#ifdef DEBUG
  if(enable_debug)
    rt_syslog(ATMD_INFO, "Board [master_reset] Performing TDC-GPX master reset.");
#endif

  // TDC-GPX software master reset (we need to keep the start timer!)
  uint32_t reg = (0x42400000) | (ATMD_AUTORETRIG);
  reg = reg | ((this->en_mtimer) ? 0x04000000 : 0x0); // Enable mtimer tigger on start pulse
  this->write_register(reg);

  // Sleep for 1 us
  rt_task_sleep(1000);
}


/* @fn ATMDboard::boardconfig()
 * Reset the board and configure it. Return an error if the PLL cannot lock.
 *
 * @return Return 0 on success, -1 on error.
 *
 * NOTE: this function can be called only by a Xenomai real-time task.
 */
int ATMDboard::config() {

  // Reset the board
  this->reset();

  // Register value
  uint32_t reg;

  // Configure reg0: rising and falling edges
  uint16_t rising = 0x0000 | ((this->en_start_rising) ? 0x0001 : 0x0000) | (this->get_rising_mask() << 1);
  uint16_t falling = 0x0000 | ((this->en_start_falling) ? 0x0001 : 0x0000) | (this->get_falling_mask() << 1);
  reg = (0x00000081) | ((rising << 10) | (falling << 19));
#ifdef DEBUG
  if(enable_debug)
    rt_syslog(ATMD_DEBUG, "Config [boardconfig]: reg0: 0x%08X.", reg);
#endif
  this->write_register(reg);

  // Configure reg1: channel adjust... useless!
  reg = (0x10000000);
#ifdef DEBUG
  if(enable_debug)
    rt_syslog(ATMD_DEBUG, "Config [boardconfig]: reg1: 0x%08X.", reg);
#endif
  this->write_register(reg);

  // Configure reg2: channels enable and I-Mode
  uint16_t chan_disable = ~(rising | falling) & 0x01FF;
  reg = (0x20000002) | (chan_disable << 3);
#ifdef DEBUG
  if(enable_debug)
    rt_syslog(ATMD_DEBUG, "Config [boardconfig]: reg2: 0x%08X.", reg);
#endif
  this->write_register(reg);

  // Configure reg3: not used
  reg = (0x30000000);
#ifdef DEBUG
  if(enable_debug)
    rt_syslog(ATMD_DEBUG, "Config [boardconfig]: reg3: 0x%08X.", reg);
#endif
  this->write_register(reg);

  // Configure reg4: start timer
  reg = (0x42000000) | (ATMD_AUTORETRIG);
  reg = reg | ((this->en_mtimer) ? 0x04000000 : 0x0); // Enable mtimer tigger on start pulse
#ifdef DEBUG
  if(enable_debug)
    rt_syslog(ATMD_DEBUG, "Config [boardconfig]: reg4: 0x%08X.", reg);
#endif
  this->write_register(reg);

  // Configure reg5: disable stop before start, disable start after start, start offset
  reg = (0x50000000);
  reg = reg | ((this->start_dis_start) ? 0x00400000 : 0x0); // Disable start after start
  reg = reg | ((this->stop_dis_start) ? 0x00200000 : 0x0); // Disable stop before start
  reg = reg | this->_start_offset; // Set the start offset
#ifdef DEBUG
  if(enable_debug)
    rt_syslog(ATMD_DEBUG, "Config [boardconfig]: reg5: 0x%08X.", reg);
#endif
  this->write_register(reg);

  // Configure reg6: nothing relevant
  reg = (0x600000FF);
#ifdef DEBUG
  if(enable_debug)
    rt_syslog(ATMD_DEBUG, "Config [boardconfig]: reg6: 0x%08X.", reg);
#endif
  this->write_register(reg);

  // Configure reg7: MTimer and PLL settings
  reg = (0x70001800);
  reg = reg | (this->hsdiv & 0x00FF); // Configure high speed divider
  reg = reg | ((this->refclkdiv & 0x0007) << 8); // Configure reference clock divider
  reg = reg | (this->mtimer << 15); // Configure mtimer
#ifdef DEBUG
  if(enable_debug)
    rt_syslog(ATMD_DEBUG, "Config [boardconfig]: reg7: 0x%08X.", reg);
#endif
  this->write_register(reg);

  // Configure reg11: unmask error flags
  reg = (0xB7FF0000);
#ifdef DEBUG
  if(enable_debug)
    rt_syslog(ATMD_DEBUG, "Config [boardconfig]: reg11: 0x%08X.", reg);
#endif
  this->write_register(reg);

  // Configure reg12: msb of start to intflag or mtimer end to intflag
  reg = (0xC0000000) | ((this->start_to_intflag) ? 0x04000000 : 0x02000000);
#ifdef DEBUG
  if(enable_debug)
    rt_syslog(ATMD_DEBUG, "Config [boardconfig]: reg12: 0x%08X.", reg);
#endif
  this->write_register(reg);

  // Sleep to let the PLL lock (for 500ms)
  rt_task_sleep(500000000);

  // Check if the PLL is locked
  this->set_dra(0x000C);
  reg = this->read_dra();

  if(reg & 0x00000400) {
    rt_syslog(ATMD_ERR, "Config [boardconfig]: TDC-GPX PLL not locked. Probably wrong resolution (refclkdiv = %d, hsdiv = %d).", this->refclkdiv, this->hsdiv);
    return -1;

  } else {
#ifdef DEBUG
    if(enable_debug)
      rt_syslog(ATMD_DEBUG, "Config [boardconfig]: TDC-GPX PLL locked.");
#endif
    return 0;
  }
}
