/*
 * ATMD Server version 3.0
 *
 * ATMD Agent - Measurement functions
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

// Global termination interrupt
extern bool terminate_interrupt;

#ifdef DEBUG
extern bool enable_debug;
#endif

#include "atmd_agentmeasure.h"


/* @fn void atmd_measure(void *arg)
 * This function is executed as real-time thread. When triggered acquire a single ATMD-GPX start event.
 * The function takes as argument a structure...
 * Data is saved into a memory area allocated within a RT heap. The pointer is passed to the main RT thread through a RT message queue.
 */
void atmd_measure(void *arg) {
  int retval = 0;

  // Cast argument
  InitData *sys = static_cast<InitData*>(arg);

  // Bind to heap
  RT_HEAP heap;
  retval = rt_heap_bind(&heap, sys->heap_name, TM_NONBLOCK);
  if(retval) {
    switch(retval) {
      case -EFAULT: // Invalid memory
        rt_syslog(ATMD_ERR, "Measure [atmd_measure]: rt_heap_bind() failed. Invalid memory reference.");
        return;

      case -EWOULDBLOCK: // Object not registered
        rt_syslog(ATMD_ERR, "Measure [atmd_measure]: rt_heap_bind() failed. Object named '%s' is not registered.", sys->heap_name);
        return;

      case -ENOENT: // /dev/rtheap not present
        rt_syslog(ATMD_ERR, "Measure [atmd_measure]: rt_heap_bind() failed. /dev/rtheap not present.");
        return;

      default:
        rt_syslog(ATMD_ERR, "Measure [atmd_measure]: rt_heap_bind() failed with unexpected return value (%d).", retval);
        return;
    }
  }

  // Bind to message queue
  RT_QUEUE queue;
  retval = rt_queue_bind(&queue, sys->queue_name, TM_NONBLOCK);
  if(retval) {
    switch(retval) {
      case -EFAULT: // Invalid memory
        rt_syslog(ATMD_ERR, "Measure [atmd_measure]: rt_queue_bind() failed. Invalid memory reference.");
        return;

      case -EWOULDBLOCK: // Object not registered
        rt_syslog(ATMD_ERR, "Measure [atmd_measure]: rt_queue_bind() failed. Object named '%s' is not registered.", sys->queue_name);
        return;

      case -ENOENT: // /dev/rtheap not present
        rt_syslog(ATMD_ERR, "Measure [atmd_measure]: rt_queue_bind() failed. /dev/rtheap not present.");
        return;

      default:
        rt_syslog(ATMD_ERR, "Measure [atmd_measure]: rt_queue_bind() failed with unexpected return value (%d).", retval);
        return;
    }
  }

  // Create structure to hold start data
  EventData events(&heap);
  try {
    events.reserve(ATMD_BLOCK);

  } catch(int e) {
    switch(e) {
      case -EINVAL:
        rt_syslog(ATMD_ERR, "Measure [atmd_measure]: rt_heap_alloc() failed. Invalid heap descriptor.");
        return;

      case -EIDRM:
        rt_syslog(ATMD_ERR, "Measure [atmd_measure]: rt_heap_alloc() failed. Deleted heap descriptor.");
        return;

      case -EWOULDBLOCK:
        rt_syslog(ATMD_ERR, "Measure [atmd_measure]: rt_heap_alloc() failed. No memory available.");
        return;

      default:
        rt_syslog(ATMD_ERR, "Measure [atmd_measure]: unhandled exception. Return value was (%d).", e);
        return;
    }
  }

  while(true) {

    // Check termination flag
    if(terminate_interrupt)
      break;

    // Put thread to sleep, waiting for a start command
    retval = rt_task_suspend(NULL);
    if(retval) {
      switch(retval) {
        case -EINTR: // Received a signal
          // TODO: error handling / termination (?)
          return;

        case -EPERM: // Invalid context
          rt_syslog(ATMD_ERR, "Measure [atmd_measure]: rt_task_suspend called from an invalid context.");
          return;

        default:
          rt_syslog(ATMD_ERR, "Measure [atmd_measure]: rt_task_suspend failed with unexpected error code (%d).", retval);
          return;
      }
    }

    // Starting measure
    sys->board->status(ATMD_STATUS_RUNNING);

    // Read configuration from message queue
    MeasureDef *meas_info = NULL;
    retval = rt_queue_receive(&queue, (void**)&meas_info, 1000000000); // 1sec timeout
    if(retval < 0) {
      switch(retval) {
        case -EINVAL:
        case -EIDRM:
          rt_syslog(ATMD_ERR, "Measure [atmd_measure]: rt_queue_receive failed. Invalid queue descriptor. Terminating.");
          // Terminate server
          terminate_interrupt = true;
          return;

        case -ETIMEDOUT:
          rt_syslog(ATMD_ERR, "Measure [atmd_measure]: rt_queue_receive failed because timeout was reached. Aborting measure.");
          sys->board->status(ATMD_STATUS_ERR);
          break;

        case -EINTR:
          rt_syslog(ATMD_ERR, "Measure [atmd_measure]: rt_queue_receive failed. rt_task_unblock() has been called. Aborting measure.");
          sys->board->status(ATMD_STATUS_ERR);
          break;

        default:
          rt_syslog(ATMD_ERR, "Measure [atmd_measure]: rt_queue_receive failed with an unexpected return value (%d).", retval);
          // Terminate server
          terminate_interrupt = true;
          return;
      }

      // Try again!
      continue;

    } else if(retval != sizeof(MeasureDef)) {
      // The message received is incomplete
      rt_syslog(ATMD_ERR, "Measure [atmd_measure]: rt_queue_receive received an incomplete message. Aborting measure.");
      rt_queue_free(&queue, (void*)meas_info);
      continue;
    }

    // Synchronization to TDMA (wait 10 cycles from master sync)
    sys->sock->wait_tdma(meas_info->tdma_cycle() + 10);

    // Start measure cycle
    RTIME measure_start = rt_timer_read();
    RTIME measure_end = measure_start;
    uint32_t index = 0;
    while( (measure_end - measure_start) < (meas_info->measure_time() - meas_info->window_time()) ) {

      // Clear event structure
      events.clear();

      // Call measure function
      retval = atmd_get_start(sys->board, meas_info->window_time(), meas_info->timeout(), events);
      if(retval) {
        rt_syslog(ATMD_ERR, "Measure [atmd_measure]: failed to get start. Terminating measure.");
        measure_end = rt_timer_read();
        break;
      }

      // Send data through real time network
      retval = atmd_send_start(index, events, sys->sock, sys->addr);
      if(retval) {
        rt_syslog(ATMD_ERR, "Measure [atmd_measure]: failed to send start data. Terminating measure.");
        measure_end = rt_timer_read();
        break;
      }

      // Increment start counter
      index++;

      // Update measure_end
      measure_end = rt_timer_read();

      if(sys->board->stop())
        break;

      // Sleep to let RTnet to transfer the data
      rt_task_sleep(meas_info->deadtime());
    }

    // Send termination packet on the data socket
    DataMsg packet;
    packet.type(ATMD_DT_TERM);
    packet.window_start(measure_start);
    packet.window_time(measure_end - measure_start);
    packet.encode();
    try {
      sys->sock->send(packet, sys->addr);
    } catch(int e) {
      rt_syslog(ATMD_CRIT, "Measure [atmd_measure]: failed to send a packet over RTnet. Error: %d", e);

      // Terminate agent
      terminate_interrupt = true;
      return;
    }

    // Free message structure
    rt_queue_free(&queue, (void*)meas_info);
  }

  // Cleanup
  rt_queue_unbind(&queue);
  rt_heap_unbind(&heap);
}


/* @fn int atmd_get_start(ATMDboard* board, RTIME window, RTIME timeout, EventData& event)
 * This function acquire a single start event and return data in the event structure.
 * @param board ATMD board object
 * @param window Measure window in nanoseconds
 * @param timeout Maximum time to wait for a start event in nanoseconds
 * @param event Reference to the event strucuture that will hold the data
 * @return Return 0 on success or a negative value on error
 */
int atmd_get_start(ATMDboard* board, RTIME window, RTIME timeout, EventData& events) {

  // We init the window finish flag
  bool finish_window = false;

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
  RTIME wait_start = rt_timer_read();
  do {

    // Read the reg12 of TDC-GPX
    dra_data = board->read_dra();

    if(dra_data & 0x00001000) {
      // We save the begin time of the measure window
      events.begin(rt_timer_read());

#ifdef DEBUG
      if(enable_debug)
        rt_syslog(ATMD_DEBUG, "Measure [atmd_get_start]: received start.");
#endif

      // Exit loop
      break;
    }

    // Check timeout
    if(rt_timer_read()-wait_start > timeout) {
      // Disable inputs
      board->mb_config(0x0008);
      rt_syslog(ATMD_DEBUG, "Measure [atmd_get_start]: timed out waiting for a start event.");
      return -ATMD_ERR_NOSTART;

    } else {
      // TODO: rt_task_sleep(??);
    }

  } while(true);


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
    // TODO: redefine stop command check (if needed...)

    if(en_fifo0) {
      // Read TDC-GPX FIFO0
      if( !(mbs & 0x0800) ) { // FIFO0 not empty
        board->set_dra(0x0008);
        dra_data = board->read_dra();

        // Get the start count from data
        start_count = (int16_t)((dra_data & 0x03FC0000) >> 18);

        // Get stop time
        int32_t stoptime = (int32_t)(dra_data & 0x0001FFFF) - board->start_offset();

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

        // Add stop
        try {
          events.add(ch, stoptime, (uint32_t)start_count + (main_startcounter0 * 256));

        } catch(int e) { // Catch errors from rt_heap_alloc
            switch(e) {
              case -EINVAL:
                rt_syslog(ATMD_ERR, "Measure [atmd_get_start]: rt_heap_alloc() failed. Invalid heap descriptor.");
                return -ATMD_ERR_ALLOC;

              case -EIDRM:
                rt_syslog(ATMD_ERR, "Measure [atmd_get_start]: rt_heap_alloc() failed. Deleted heap descriptor.");
                return -ATMD_ERR_ALLOC;

              case -EWOULDBLOCK:
                rt_syslog(ATMD_ERR, "Measure [atmd_get_start]: rt_heap_alloc() failed. No memory available.");
                return -ATMD_ERR_ALLOC;

              default:
                rt_syslog(ATMD_ERR, "Measure [atmd_get_start]: unhandled exception. Return value was (%d).", e);
                return -ATMD_ERR_ALLOC;
            }
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
          int32_t stoptime = (int32_t)(dra_data & 0x0001FFFF) - board->start_offset();

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

          // Add stop
          try {
            events.add(ch, stoptime, (uint32_t)start_count + (main_startcounter1 * 256));

          } catch(int e) { // Catch errors from rt_heap_alloc
            switch(e) {
              case -EINVAL:
                rt_syslog(ATMD_ERR, "Measure [atmd_get_start]: rt_heap_alloc() failed. Invalid heap descriptor.");
                return -ATMD_ERR_ALLOC;

              case -EIDRM:
                rt_syslog(ATMD_ERR, "Measure [atmd_get_start]: rt_heap_alloc() failed. Deleted heap descriptor.");
                return -ATMD_ERR_ALLOC;

              case -EWOULDBLOCK:
                rt_syslog(ATMD_ERR, "Measure [atmd_get_start]: rt_heap_alloc() failed. No memory available.");
                return -ATMD_ERR_ALLOC;

              default:
                rt_syslog(ATMD_ERR, "Measure [atmd_get_start]: unhandled exception. Return value was (%d).", e);
                return -ATMD_ERR_ALLOC;
            }
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

      // Check if window time was exceeded
      events.end(rt_timer_read());
      if(!finish_window && (events.end() - events.begin()) > window) {
        // Disable inputs
        board->mb_config(0x0008);
        finish_window = true;
      }

      // Stop condition
      if( (!en_fifo0 || stop_fifo0) && (!en_fifo1 || stop_fifo1) )
          break;

  } while(true);

  // We save the start01
  board->set_dra(0x000A);
  events.compute_start01((uint32_t)(board->read_dra() & 0x0001FFFF));

  return 0;
}


/* @fn int atmd_send_start(uint32_t id, int sock, EventData& events)
 * This function sends the data acquired in a start event through the RT network.
 * @param id Start identifier
 * @param events Reference to the event strucuture that will hold the data
 * @param sock RT socket
 * @param addr Remote ethernet address
 * @return Return 0 on success or a negative value on error
 */
int atmd_send_start(uint32_t id, EventData& events, RTnet* sock, const struct ether_addr* addr) {

  // Protcol:
  // 1) Header packet with all the parameters
  // 2) Data packets. NOTE: last packet is marked

  // Packet
  DataMsg packet;

  // Build first packet
  packet.window_start(events.begin());
  packet.window_time(events.end()-events.begin());

  // Cycle through events
  size_t count = 0;
  while(count < events.size()) {
    // Build packet
    if(count > 0)
      packet.clear();
    packet.id(id);
    count = packet.encode(count, events.ch(), events.stoptime(), events.retrig());

    // Send packet
    try {
      sock->send(packet, addr);
    } catch(int e) {
      rt_syslog(ATMD_ERR, "Measure [atmd_send_start]: failed to send message. Error: %d.", e);
      return -1;
    }
  }

  return 0;
}
