/*
 * ATMD Server version 3.0
 *
 * ATMD Server - Virtual board
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

// Global debug flag
#ifdef DEBUG
extern bool enable_debug;
#endif

#include "atmd_virtualboard.h"


/* @fn VirtualBoard::init()
 *
 */
int VirtualBoard::init() {

  int retval = 0;

  // Init measures vector mutex
  retval = rt_mutex_create(&_meas_mutex, ATMD_RT_MEAS_MUTEX);
  if(retval) {
    switch(retval) {
      case -ENOMEM:
        syslog(ATMD_CRIT, "VirtualBoard [init]: rt_mutex_create() failed. Not enough memory.");
        break;

      case -EEXIST:
        syslog(ATMD_CRIT, "VirtualBoard [init]: rt_mutex_create() failed. The given name is already in use.");
        break;

      case -EPERM:
        syslog(ATMD_CRIT, "VirtualBoard [init]: rt_mutex_create() failed. Called from an invalid context.");
        break;

      default:
        syslog(ATMD_CRIT, "VirtualBoard [init]: rt_mutex_create() failed with an unexpected return code (%d).", retval);
        break;
    }
    return -1;
  }

  // The first thing to do is to start the control RT thread
  retval = rt_task_spawn(&_ctrl_task, ATMD_RT_CTRL_TASK, 0, 75, T_FPU|T_JOINABLE, VirtualBoard::control_task, (void*)this);
  if(retval) {
    switch(retval) {
      case -ENOMEM:
        syslog(ATMD_CRIT, "VirtualBoard [init]: rt_task_spawn() failed because not enough memory was available to create the task.");
        break;

      case -EEXIST:
        syslog(ATMD_CRIT, "VirtualBoard [init]: rt_task_spawn() failed because the given name is already in use.");
        break;

      case -EPERM:
        syslog(ATMD_CRIT, "VirtualBoard [init]: rt_task_spawn() failed because called from an invalid context.");
        break;

      default:
        syslog(ATMD_CRIT, "VirtualBoard [init]: rt_task_spawn() failed with an unexpected return code (%d).", retval);
        break;
    }
    return -1;
  }

  // Then we can start the data retrieving task.
  retval = rt_task_spawn(&_rt_data_task, ATMD_RT_DATA_TASK, 0, 75, T_FPU|T_JOINABLE, VirtualBoard::rt_data_task, (void*)this);
  if(retval) {
    switch(retval) {
      case -ENOMEM:
        syslog(ATMD_CRIT, "VirtualBoard [init]: rt_task_spawn() failed because not enough memory was available to create the task.");
        break;

      case -EEXIST:
        syslog(ATMD_CRIT, "VirtualBoard [init]: rt_task_spawn() failed because the given name is already in use.");
        break;

      case -EPERM:
        syslog(ATMD_CRIT, "VirtualBoard [init]: rt_task_spawn() failed because called from an invalid context.");
        break;

      default:
        syslog(ATMD_CRIT, "VirtualBoard [init]: rt_task_spawn() failed with an unexpected return code (%d).", retval);
        break;
    }
    return -1;
  }

  // Finally we can start the non-RT data data thread
  retval = rt_task_spawn(&_data_task, ATMD_NRT_DATA_TASK, 0, 0, T_FPU|T_JOINABLE, VirtualBoard::data_task, (void*)this);
  if(retval) {
    switch(retval) {
      case -ENOMEM:
        syslog(ATMD_CRIT, "VirtualBoard [init]: rt_task_spawn() failed because not enough memory was available to create the task.");
        break;

      case -EEXIST:
        syslog(ATMD_CRIT, "VirtualBoard [init]: rt_task_spawn() failed because the given name is already in use.");
        break;

      case -EPERM:
        syslog(ATMD_CRIT, "VirtualBoard [init]: rt_task_spawn() failed because called from an invalid context.");
        break;

      default:
        syslog(ATMD_CRIT, "VirtualBoard [init]: rt_task_spawn() failed with an unexpected return code (%d).", retval);
        break;
    }
    return -1;
  }

  return 0;
}


/* @fn VirtualBoard::control_task(void *arg)
 * This function is executed by the control thread. At first wait for the configured agents,
 * then it loops waiting for packets from agents (or send commands to them, when awaken by the main thread)
 * @param arg Cookie for the RT thread
 * @return This function does not return
 */
void VirtualBoard::control_task(void *arg) {

  // Init rt_printf and rt_syslog
  rt_print_auto_init(1);
  rt_syslog(ATMD_INFO, "VirtualBoard [control_task]: successfully started real-time control thread.");

  // Cast back 'this' pointer
  VirtualBoard * pthis = (VirtualBoard*)arg;

  // Create real-time network control socket
  pthis->ctrl_sock().rtskbs(pthis->config().rtskbs());
  pthis->ctrl_sock().protocol(ATMD_PROTO_CTRL);
  pthis->ctrl_sock().interface(pthis->config().rtif());
  if(pthis->ctrl_sock().init(true)) {
    rt_syslog(ATMD_CRIT, "VirtualBoard [control_task]: failed to init RTnet socket.");

    // Terminate server
    terminate_interrupt = true;
    return;
  }

  // Now we can search for agents!
  AgentMsg packet;
  packet.type(ATMD_CMD_BRD);
  packet.version(VERSION);
  packet.encode();

  // Broadcast address
  struct ether_addr brd_addr;
  ether_aton_r("FF:FF:FF:FF:FF:FF", &brd_addr);

  // Send broadcast packet
  try {
    pthis->ctrl_sock().send(packet, &brd_addr);

  } catch (int e) {
    rt_syslog(ATMD_CRIT, "VirtualBoard [control_task]: failed to send broadcast packet. Error: %d", e);

    // Terminate server
    terminate_interrupt = true;
    return;
  }

  // Then wait for responses
  size_t ag_count = 0;
  struct ether_addr remote_addr;
  packet.clear();

  while(ag_count < pthis->config().agents()) {
    // Receive packet
    try {
      pthis->ctrl_sock().recv(packet, &remote_addr);
    } catch(int e) {
      rt_syslog(ATMD_CRIT, "VirtualBoard [control_task]: failed to receive agent answer packet. Error: %d", e);

      // Terminate server
      terminate_interrupt = true;
      return;
    }

    // Check type and version
    packet.decode();
    if(packet.type() != ATMD_CMD_HELLO) {
      // Packet is not an answer, ignore
#ifdef DEBUG
      if(enable_debug)
        rt_syslog(ATMD_DEBUG, "VirtualBoard [control_task]: received an unexpected packet with type (%d).", packet.type());
#endif
      continue;
    }

    if(strncmp(packet.version(), VERSION, ATMD_VER_LEN) != 0) {
      // Packet has wrong version
#ifdef DEBUG
      if(enable_debug)
        rt_syslog(ATMD_DEBUG, "VirtualBoard [control_task]: received HELLO packet with wrong version number (it was '%s' instead of '%s').", packet.version(), VERSION);
#endif
      continue;
    }

    // Compare address with configured ones
    for(size_t i = 0; i < pthis->config().agents(); i++) {
      if(memcmp(&remote_addr, pthis->config().get_agent(i), sizeof(struct ether_addr)) == 0) {

        // Found! Now compare address with those already received
        for(size_t j = 0; j < pthis->agents(); j++) {
          if(memcmp(&remote_addr, pthis->get_agent(j).agent_addr(), sizeof(struct ether_addr)) == 0)
            goto ignore;
        }
        // Add agent
        pthis->add_agent(i, &remote_addr);
        ag_count++;
        break;
      }
    }
    ignore:
      // Ignoring agent packet...
      continue;
  }

  // Now agents are configured. We can cycle for commands on RTnet...
  // TODO...

}


/* @fn void VirtualBoard::rt_data_task(void *arg)
 *
 */
void VirtualBoard::rt_data_task(void *arg) {

  int retval = 0;

  // Init rt_printf and rt_syslog
  rt_print_auto_init(1);
  rt_syslog(ATMD_INFO, "VirtualBoard [rt_data_task]: successfully started real-time data thread.");

  // Cast back the 'this' pointer
  VirtualBoard* pthis = (VirtualBoard*)arg;

  // Create data socket
  pthis->data_sock().rtskbs(pthis->config().rtskbs());
  pthis->data_sock().interface(pthis->config().rtif());
  pthis->data_sock().protocol(ATMD_PROTO_DATA);
  if(pthis->data_sock().init(true)) {
    rt_syslog(ATMD_CRIT, "VirtualBoard [rt_data_task]: failed to init RTnet socket.");

    // Terminate server
    terminate_interrupt = true;
    return;
  }

  // Create data queue to data thread in linux-mode
  RT_QUEUE data_queue;
  retval = rt_queue_create(&data_queue, ATMD_RT_DATA_QUEUE, 10000000, Q_UNLIMITED, Q_SHARED);
  if(retval) {
    switch(retval) {
      case -ENOMEM:
        rt_syslog(ATMD_CRIT, "VirtualBoard [rt_data_task]: rt_queue_create() failed. Not enough memory available.");
        break;

      case -EEXIST:
        rt_syslog(ATMD_CRIT, "VirtualBoard [rt_data_task]: rt_queue_create() failed. The given name is already in use.");
        break;

      case -EPERM:
        rt_syslog(ATMD_CRIT, "VirtualBoard [rt_data_task]: rt_queue_create() failed. Called from invalid context.");
        break;

      case -EINVAL:
        rt_syslog(ATMD_CRIT, "VirtualBoard [rt_data_task]: rt_queue_create() failed. Invalid parameter.");
        break;

      case -ENOENT:
        rt_syslog(ATMD_CRIT, "VirtualBoard [rt_data_task]: rt_queue_create() failed. Cannot open /dev/rtheap.");
        break;

      default:
        rt_syslog(ATMD_CRIT, "VirtualBoard [rt_data_task]: rt_queue_create() failed with an unexpected error (%d).", retval);
        break;
    }

    // Terminate server
    terminate_interrupt = true;
    return;
  }

  // Cycle waiting for data
  DataMsg packet;
  struct ether_addr remote_addr;

  while(true) {

    // Check termination interrupt
    if(terminate_interrupt) {
      rt_queue_delete(&data_queue);
      pthis->data_sock().close();
      return;
    }

    // Get a packet
    try {
      pthis->data_sock().recv(packet, &remote_addr);
    } catch(int e) {
      rt_syslog(ATMD_CRIT, "VirtualBoard [rt_data_task]: failed to receive a packet over RTnet socket.");

      // Terminate server
      terminate_interrupt = true;
      return;
    }

    // Check address
    bool good_agent = false;
    size_t agent_id = 0;
    for(size_t i = 0; i < pthis->agents(); i++) {
      if(memcmp(&remote_addr, pthis->get_agent(i).agent_addr(), sizeof(struct ether_addr)) == 0) {
        agent_id = pthis->get_agent(i).id();
        good_agent = true;
        break;
      }
    }

    // The packet comes from a valid agent
    if(good_agent) {

      // Allocate QUEUE buffer
      void *msg = rt_queue_alloc(&data_queue, sizeof(size_t) + ATMD_PACKET_SIZE);
      if(msg == NULL) {
        // Message allocation failed
        rt_syslog(ATMD_CRIT, "VirtualBoard [rt_data_task]: rt_queue_alloc() failed.");

        // Terminate server
        terminate_interrupt = true;
        continue;
      }

      // Translate packet buffer to queue message (it just correnct the channels based on agent id)
      *(static_cast<size_t*>(msg)) = agent_id;
      memcpy((void*)((uint8_t*)msg+sizeof(size_t)), packet.get_buffer(), ATMD_PACKET_SIZE);

      retval = rt_queue_send(&data_queue, msg, ATMD_PACKET_SIZE+sizeof(size_t), Q_NORMAL);
      if(retval < 0) {
        switch(retval) {
          case -EINVAL:
            rt_syslog(ATMD_CRIT, "VirtualBoard [rt_data_task]: rt_queue_send() failed. Invalid queue descriptor.");
            break;

          case -EIDRM:
            rt_syslog(ATMD_CRIT, "VirtualBoard [rt_data_task]: rt_queue_send() failed. Close queue descriptor.");
            break;

          case -ENOMEM:
            rt_syslog(ATMD_CRIT, "VirtualBoard [rt_data_task]: rt_queue_send() failed. Not enough memory for the message.");
            break;

          default:
            rt_syslog(ATMD_CRIT, "VirtualBoard [rt_data_task]: rt_queue_send() failed with an unexpected error (%d)", retval);
            break;
        }

        // Terminate server
        terminate_interrupt = true;
      }
    }
  }
}


/* @fn static void VirtualBoard::data_task(void *arg)
 *
 */
void VirtualBoard::data_task(void *arg) {

  int retval = 0;

  // Cast back 'this' pointer
  VirtualBoard *pthis = (VirtualBoard*)arg;

  // Prevent this task to send SIGDEBUG when switching to secondary mode
  rt_task_set_mode(T_WARNSW, 0, NULL);

  // Bind to data queue
  RT_QUEUE data_queue;
  retval = rt_queue_bind(&data_queue, ATMD_RT_DATA_QUEUE, TM_INFINITE);
  if(retval) {
    switch(retval) {
      case -EFAULT:
        rt_syslog(ATMD_CRIT, "VirtualBoard [data_task]: rt_queue_bind() failed. Invalid parameter.");
        break;

      case -EINTR:
        rt_syslog(ATMD_CRIT, "VirtualBoard [data_task]: rt_queue_bind() failed. Unblocked by rt_task_unblock().");
        break;

      case -EPERM:
        rt_syslog(ATMD_CRIT, "VirtualBoard [data_task]: rt_queue_bind() failed. Called from an invalid context.");
        break;

      case -ENOENT:
        rt_syslog(ATMD_CRIT, "VirtualBoard [data_task]: rt_queue_bind() failed. Cannot open /dev/rtheap.");
        break;

      default:
        rt_syslog(ATMD_CRIT, "VirtualBoard [data_task]: rt_queue_bind() failed with an unexpected error (%d).", retval);
        break;
    }
  }

  // Service variables
  DataMsg packet;

  // Vector of bool to check if we received the data from all the agents
  std::vector<bool> agent_done;

  // Vector of bool to check if the measure has ended
  std::vector<bool> agent_end;

  // Vector of current start pointers
  std::vector<StartData*> curr_start;

  // Vector of current start ID
  std::vector<uint32_t> curr_start_id;

  // Init vectors
  for(size_t i = 0; i < pthis->agents(); i++) {
    agent_done.push_back(false);
    agent_end.push_back(false);
    curr_start.push_back(NULL);
    curr_start_id.push_back(0);
  }

  // Current measure
  Measure* curr_measure = NULL;


  // Cycle of the queue
  while(true) {

    // Check termination interrupt
    if(terminate_interrupt)
      break;

    void * msg = NULL;
    retval = rt_queue_receive(&data_queue, &msg, TM_INFINITE);
    if(retval < 0) {
      switch(retval) {
        case -EINVAL:
          rt_syslog(ATMD_CRIT, "VirtualBoard [data_task]: rt_queue_receive() failed. Invalid queue descriptor.");
          break;

        case -EIDRM:
          rt_syslog(ATMD_CRIT, "VirtualBoard [data_task]: rt_queue_receive() failed. Deleted queue descriptor.");
          break;

        case -EINTR:
          rt_syslog(ATMD_CRIT, "VirtualBoard [data_task]: rt_queue_receive() failed. Unblocked by rt_task_unblock().");
          break;

        case -EPERM:
          rt_syslog(ATMD_CRIT, "VirtualBoard [data_task]: rt_queue_receive() failed. Called from an invalid context.");
          break;

        default:
          rt_syslog(ATMD_CRIT, "VirtualBoard [data_task]: rt_queue_receive() failed. ");
          break;
      }

      // Terminate server
      terminate_interrupt = true;
      break;
    }

    // Translate the message back to a packet
    size_t agent_id = *( static_cast<size_t*>(msg) );
    memcpy(packet.get_buffer(), (void*)((uint8_t*)msg+sizeof(size_t)), ATMD_PACKET_SIZE);
    packet.decode();

    // Clear queue buffer
    rt_queue_free(&data_queue, msg);


    // === START PACKET ASSEMBLING ===


    // If packet type is ATMD_DT_TERM, the measure has ended (at least for the current agent)
    if(packet.type() == ATMD_DT_TERM) {
      agent_end[agent_id] = true;
      // TODO: save measure times
      continue;
    }

    if(agent_end[agent_id]) {
      // We should not recive other packets from this agent!
      syslog(ATMD_ERR, "VirtualBoard [data_task]: received a packet from an agent that has terminated its measure. Something is wrong!");
      continue;
    }

    bool measure_end = true;
    for(size_t i = 0; i < agent_end.size(); i++)
      measure_end &= agent_end[i];

    if(measure_end && pthis->get_autosave() == 0) {

      // == Add measure to storage ==
      // Lock measures object
      retval = pthis->acquire_lock();
      if(retval) {
        switch(retval) {
          case -EINVAL:
          case -EIDRM:
            syslog(ATMD_CRIT, "VirtualBoard [data_task]: failed to acquire measure mutex because it was an invalid object.");
            break;

          case -EINTR:
            syslog(ATMD_CRIT, "VirtualBoard [data_task]: failed to acquire measure mutex because the task was unlocked.");
            break;

          case -EPERM:
            syslog(ATMD_CRIT, "VirtualBoard [data_task]: failed to acquire measure mutex because it was asked in an invalid context.");
            break;

          default:
            syslog(ATMD_CRIT, "VirtualBoard [data_task]: failed to acquire measure mutex for an unexpected reason (Code: %d).", retval);
            break;
        }

        // Terminate server
        terminate_interrupt = true;
        break;
      }

      // Add measure
      pthis->add_measure(curr_measure);
      curr_measure = NULL;

      // Release lock of measure object
      retval = pthis->release_lock();
      if(retval) {
        switch(retval) {
          case -EINVAL:
          case -EIDRM:
            syslog(ATMD_CRIT, "VirtualBoard [data_task]: failed to release measure mutex because it was an invalid object.");
            break;

          case -EPERM:
            syslog(ATMD_CRIT, "VirtualBoard [data_task]: failed to release measure mutex because it was asked in an invalid context.");
            break;

          default:
            syslog(ATMD_CRIT, "VirtualBoard [data_task]: failed to release measure mutex for an unexpected reason (Code: %d).", retval);
            break;
        }

        // Terminate server
        terminate_interrupt = true;
        break;
      }

      // Reset end flags
      for(size_t i = 0; i < pthis->agents(); i++)
        agent_end[i] = false;

      continue;
    }


    // Handle autosave
    if(pthis->get_autosave() != 0 && (curr_measure->count_starts() == pthis->get_autosave() || measure_end)) {

      // == Autosave measure ==
      // Lock measures object
      retval = pthis->acquire_lock();
      if(retval) {
        switch(retval) {
          case -EINVAL:
          case -EIDRM:
            syslog(ATMD_CRIT, "VirtualBoard [data_task]: failed to acquire measure mutex because it was an invalid object.");
            break;

          case -EINTR:
            syslog(ATMD_CRIT, "VirtualBoard [data_task]: failed to acquire measure mutex because the task was unlocked.");
            break;

          case -EPERM:
            syslog(ATMD_CRIT, "VirtualBoard [data_task]: failed to acquire measure mutex because it was asked in an invalid context.");
            break;

          default:
            syslog(ATMD_CRIT, "VirtualBoard [data_task]: failed to acquire measure mutex for an unexpected reason (Code: %d).", retval);
            break;
        }

        // Terminate server
        terminate_interrupt = true;
        break;
      }

      // Add measure
      pthis->add_measure(curr_measure);
      if(!measure_end) {
        curr_measure = new Measure;
      } else {
        curr_measure = NULL;
        // Reset end flags
        for(size_t i = 0; i < pthis->agents(); i++)
          agent_end[i] = false;
        // Set board status to IDLE
        pthis->status(ATMD_STATUS_IDLE);
      }

      // Save measure
      std::string filename = "???"; // TODO: compose filename!
      if(pthis->save_measure(pthis->measures()-1, filename)) {
        // TODO: handle errors
      }

      // Delete last measure
      if(pthis->delete_measure(pthis->measures()-1)) {
        // TODO: handle errors
      }

      // Release lock of measure object
      retval = pthis->release_lock();
      if(retval) {
        switch(retval) {
          case -EINVAL:
          case -EIDRM:
            syslog(ATMD_CRIT, "VirtualBoard [data_task]: failed to release measure mutex because it was an invalid object.");
            break;

          case -EPERM:
            syslog(ATMD_CRIT, "VirtualBoard [data_task]: failed to release measure mutex because it was asked in an invalid context.");
            break;

          default:
            syslog(ATMD_CRIT, "VirtualBoard [data_task]: failed to release measure mutex for an unexpected reason (Code: %d).", retval);
            break;
        }

        // Terminate server
        terminate_interrupt = true;
        break;
      }
    }


    // If measure is NULL, create one
    if(curr_measure == NULL) {
      curr_measure = new Measure;
      // Reset current start id
      for(size_t i = 0; i < pthis->agents(); i++)
        curr_start_id[i] = 0;
    }

    // If current start is NULL, create one
    if(curr_start[agent_id] == NULL) {
      if(packet.type() == ATMD_DT_FIRST || packet.type() == ATMD_DT_ONLY) {
        curr_start[agent_id] = new StartData;
        curr_start[agent_id]->set_time(packet.window_start(), packet.window_time());
        curr_start_id[agent_id] = packet.id();

      } else {
        // Error! We missed the first packet or something very bad happened
        syslog(ATMD_ERR, "VirtualBoard [data_task]: missed the first packet of a data sequence. Discarding current start.");
        continue;
      }

    } else {
      // Start id should not change until all the packets are received.
      // The order of packets from a single agent is guaranteed, but it is not between different agents.
      // We need to handle out of sequence packets (maybe we can implement a FIFO of out of sequence packets that will be processed after the current start is over)
      if(curr_start_id[agent_id] != packet.id()) {
        syslog(ATMD_ERR, "VirtualBoard [data_task]: packet out of squence! PANIC!");
        // TODO: handle out of sequence packets
        continue;
      }
    }


    // Extract events from packet
    for(size_t i = 0; i < packet.numev(); i++) {
      int8_t ch;
      int32_t stop;
      uint32_t retrig;
      packet.getevent(i, ch, stop, retrig);
      curr_start[agent_id]->add_event(retrig, stop, (ch > 0) ? ch + 8*agent_id : ch - 8*agent_id);
    }

    // If the packet was the last of its series set the done flag for this agent
    if(packet.type() == ATMD_DT_LAST || packet.type() == ATMD_DT_ONLY)
      agent_done[agent_id] = true;

    // Check if all the agent are done
    bool start_end = true;
    for(size_t i = 0; i < agent_done.size(); i++)
      start_end &= agent_done[i];

    // If all agent are done, add start to measure
    if(start_end) {
      // Add current start to curr_measure
      curr_measure->add_start(curr_start);
      for(size_t i = 0; i < curr_start.size(); i++)
        curr_start[i] = NULL;

      // Reset flags
      for(size_t i = 0; i < agent_done.size(); i++)
        agent_done[i] = false;
    }
  }
}


/* @fn void VirtualBoard::clear_config()
 *
 */
void VirtualBoard::clear_config() {

  // Start channel
  _start_rising = true;
  _start_falling = false;

  // Channel configuration
  _ch_rising.clear();
  _ch_falling.clear();
  for(size_t i = 0; i < 8*_agents.size(); i++) {
    _ch_rising.push_back(true);
    _ch_falling.push_back(false);
  }

  // Window time
  _window_time.set("0s");

  // Measure time
  _measure_time.set("0s");

  // Timeout time
  _timeout_time.set("0s");

  // Deadtime
  _deadtime.set("0s");

  // Start offset
  _offset = ATMD_DEF_STARTOFFSET;

  // Resolution
  _refclk = ATMD_DEF_REFCLK;
  _hsdiv = ATMD_DEF_HSDIV;

  // FTP data
  _hostname = "";
  _username = "";
  _password= "";

  // Prefix for autosave
  _prefix = "";

  // Autosave after numofstarts
  _autosave = 0;

  // Save format
  _format = ATMD_FORMAT_MATPS3;

}


/* @fn int VirtualBoard::save_measure(size_t id, std::string name)
 *
 */
int VirtualBoard::save_measure(size_t id, std::string name) {
  
}


/* @fn int VirtualBoard::start_measure()
 *
 */
int VirtualBoard::start_measure() {
  // Check status
  if(_status != ATMD_STATUS_IDLE)
    return -1;


}


/* @fn int VirtualBoard::stop_measure()
 *
 */
int VirtualBoard::stop_measure() {
  
}
