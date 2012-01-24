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

  // Init libCURL
  if((this->easy_handle = curl_easy_init()) == NULL) {
    syslog(ATMD_INFO, "Board init: failed to initialize libcurl.");
    return -1;
  }
  memset(this->curl_error, 0x0, CURL_ERROR_SIZE);
  curl_easy_setopt(this->easy_handle, CURLOPT_ERRORBUFFER, this->curl_error);

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

  // Init the command queue
  retval = rt_queue_create(&_ctrl_queue, ATMD_RT_QUEUE_NAME, 1000000, Q_UNLIMITED, Q_SHARED);
  if(retval) {
    switch(retval) {
      case -ENOMEM:
        rt_syslog(ATMD_CRIT, "VirtualBoard [init]: rt_queue_create() failed. Not enough memory available.");
        break;

      case -EEXIST:
        rt_syslog(ATMD_CRIT, "VirtualBoard [init]: rt_queue_create() failed. The given name is already in use.");
        break;

      case -EPERM:
        rt_syslog(ATMD_CRIT, "VirtualBoard [init]: rt_queue_create() failed. Called from invalid context.");
        break;

      case -EINVAL:
        rt_syslog(ATMD_CRIT, "VirtualBoard [init]: rt_queue_create() failed. Invalid parameter.");
        break;

      case -ENOENT:
        rt_syslog(ATMD_CRIT, "VirtualBoard [init]: rt_queue_create() failed. Cannot open /dev/rtheap.");
        break;

      default:
        rt_syslog(ATMD_CRIT, "VirtualBoard [init]: rt_queue_create() failed with an unexpected error (%d).", retval);
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


/* @fn VirtualBoard::recv_command(GenMsg& packet)
 *
 */
void VirtualBoard::recv_command(GenMsg& packet) throw(int) {

  void * buffer = NULL;

  // Receive packet
  int retval = rt_queue_receive(&_ctrl_queue, &buffer, TM_INFINITE);
  if(retval < 0) {
    rt_syslog(ATMD_CRIT, "VirtualBoard [recv_command]: failed to send command to control queue.");
    throw(retval);
  }

  // Copy buffer
  memcpy(packet.get_buffer(), buffer, retval);

  // Free buffer
  rt_queue_free(&_ctrl_queue, buffer);
};


/* @fn VirtualBoard::send_command(const GenMsg& packet)
 *
 */
void VirtualBoard::send_command(const GenMsg& packet) throw(int) {

  // Alloc buffer
  void * buffer = rt_queue_alloc(&_ctrl_queue, packet.size());
  if(!buffer)
    throw(ATMD_ERR_ALLOC);

  // Copy packet
  memcpy(buffer, packet.get_buffer(), packet.size());

  // Send packet
  int retval = rt_queue_send(&_ctrl_queue, buffer, packet.size(), Q_NORMAL);
  if(retval < 0) {
    rt_syslog(ATMD_CRIT, "VirtualBoard [send_command]: failed to send command to control queue. Error: %d.", retval);
    throw(ATMD_ERR_QUEUE);
  }
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
  while(true) {

    // Check termination interrupt
    if(terminate_interrupt)
      break;

    // Wait for a packet on the queue
    packet.clear();
    try {
      pthis->recv_command(packet);
    } catch(int e) {
      // TODO: handle unblock
      rt_syslog(ATMD_CRIT, "VirtualBoard [control_task]: failed to receive a command from the queue.");
      // Terminate server
      terminate_interrupt = true;
      return;
    }

    // If the packet is start
    packet.decode();
    if(packet.type() != ATMD_CMD_MEAS_SET && packet.type() != ATMD_CMD_MEAS_CTR) {
      // Wrong packet type, ignore.
      rt_syslog(ATMD_WARN, "VirtualBoard [control_task]: ignoring packet from control queue with wrong type.");
      continue;
    }

    if(packet.type() == ATMD_CMD_MEAS_CTR) {
      // A control message should be sent as a broadcast and synchronized with TDMA
      uint32_t cycle = pthis->ctrl_sock().wait_tdma();
#ifdef DEBUG
      if(enable_debug)
        rt_syslog(ATMD_DEBUG, "VirtualBoard [control_task]: sync on TDMA cycle %ld.", cycle);
#endif
      packet.tdma_cycle(cycle);
      packet.encode();

      try {
        pthis->ctrl_sock().send(packet, &brd_addr);

      } catch(int e) {
        // Failed to send packet
        rt_syslog(ATMD_CRIT, "VirtualBoard [control_task]: failed to send control packet over RTnet. Error: %d.", e);
        // Terminate server
        terminate_interrupt = true;
        return;
      }

      // Wait for acknowledge from all agents
      ag_count = 0;
      std::vector<uint16_t> agent_answers;
      std::vector<size_t> agent_ids;

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

        // Check address
        for(size_t i = 0; i < pthis->config().agents(); i++) {
          if(memcmp(&remote_addr, pthis->config().get_agent(i), sizeof(struct ether_addr)) == 0) {
            // Compare to already received ACKs
            for(size_t j = 0; j < agent_ids.size(); j++) {
              if(i == agent_ids[j]) {
                rt_syslog(ATMD_WARN, "VirtualBoard [control_task]: received multiple acknowledge from agent with address '%s'.", ether_ntoa(&remote_addr));
                goto ignore_ack;
              }
            }
            agent_answers.push_back(packet.type());
            agent_ids.push_back(i);
            ag_count++;

            ignore_ack:
            break;
          }
        }
      }

      // All ACK/BUSY/ERR received. Analyze result.
      size_t count_bad = 0;

      // Count errors
      for(size_t i = 0; i < agent_answers.size(); i++)
        if(agent_answers[i] == ATMD_CMD_ERROR)
          count_bad++;

      if(count_bad != 0) {
        if(count_bad == agent_answers.size()) {
          // All error. Build packet
          packet.clear();
          packet.type(ATMD_CMD_ERROR);
          packet.encode();
          // Send packet
          try {
            pthis->send_command(packet);
          } catch(int e) {
            rt_syslog(ATMD_CRIT, "VirtualBoard [control_task]: failed to send a command to the queue.");
            // Terminate server
            terminate_interrupt = true;
            return;
          }

        } else {
          // Some error. This is bad!
          rt_syslog(ATMD_WARN, "VirtualBoard [control_task]: agent answers are not consistent. Only some of them where ERR.");
          // TODO: must send a stop

          // Send blank bad packet
          packet.clear();
          try {
            pthis->send_command(packet);
          } catch(int e) {
            rt_syslog(ATMD_CRIT, "VirtualBoard [control_task]: failed to send a command to the queue.");
            // Terminate server
            terminate_interrupt = true;
            return;
          }
        }

      } else {
        // Count busy
        for(size_t i = 0; i < agent_answers.size(); i++)
          if(agent_answers[i] == ATMD_CMD_BUSY)
            count_bad++;
        if(count_bad != 0) {
          if(count_bad == agent_answers.size()) {
            // All busy. Build packet
            packet.clear();
            packet.type(ATMD_CMD_BUSY);
            packet.encode();
            // Send packet
            try {
              pthis->send_command(packet);
            } catch(int e) {
              rt_syslog(ATMD_CRIT, "VirtualBoard [control_task]: failed to send a command to the queue.");
              // Terminate server
              terminate_interrupt = true;
              return;
            }

          } else {
            // Some busy. This is bad!
            rt_syslog(ATMD_WARN, "VirtualBoard [control_task]: agent answers are not consistent. Only some of them where BUSY.");
            // TODO: boh!

            // Send blank bad packet
            packet.clear();
            try {
              pthis->send_command(packet);
            } catch(int e) {
              rt_syslog(ATMD_CRIT, "VirtualBoard [control_task]: failed to send a command to the queue.");
              // Terminate server
              terminate_interrupt = true;
              return;
            }
          }

        } else {
          // Everything ok!
          packet.clear();
          packet.type(ATMD_CMD_ACK);
          packet.encode();
          // Send packet
          try {
            pthis->send_command(packet);
          } catch(int e) {
            rt_syslog(ATMD_CRIT, "VirtualBoard [control_task]: failed to send a command to the queue.");
            // Terminate server
            terminate_interrupt = true;
            return;
          }
        }
      }

    } else { // ATMD_CMD_MEAS_SET

      uint32_t agent = packet.agent_id();

      // Send packets to single agent
      try {
        pthis->ctrl_sock().send(packet, pthis->get_agent(agent).agent_addr());

      } catch(int e) {
        // Failed to send packet
        rt_syslog(ATMD_CRIT, "VirtualBoard [control_task]: failed to send control packet over RTnet. Error: %d.", e);
        // Terminate server
        terminate_interrupt = true;
        return;
      }

      while(true) {
        // Wait for acknowledge from agent
        try {
          pthis->ctrl_sock().recv(packet, &remote_addr);
        } catch(int e) {
          rt_syslog(ATMD_CRIT, "VirtualBoard [control_task]: failed to receive agent answer packet. Error: %d", e);

          // Terminate server
          terminate_interrupt = true;
          return;
        }

        // Decode packet
        packet.decode();

        // Check agent address
        if(memcmp(&remote_addr, pthis->get_agent(agent).agent_addr(), sizeof(struct ether_addr)) == 0) {
          // Good agent, pass answer to non-RT side
          try {
            pthis->send_command(packet);
          } catch(int e) {
            rt_syslog(ATMD_CRIT, "VirtualBoard [control_task]: failed to send a command to the queue.");
            // Terminate server
            terminate_interrupt = true;
            return;
          }
          break;

        } else {
          rt_syslog(ATMD_WARN, "VirtualBoard [control_task]: received acknowledge from wrong agent. Address was: '%s'.", ether_ntoa(&remote_addr));
        }
      }
    }
  }
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
      if(curr_measure) {
        curr_measure->add_time(packet.window_start(), packet.window_time());
      } else {
        syslog(ATMD_ERR, "VirtualBoard [data_task]: received measure terminated packet, but current measure pointer in NULL.");
      }
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


    // If measure is NULL, create one
    if(curr_measure == NULL) {
      curr_measure = new Measure;
      // Reset current start id
      for(size_t i = 0; i < pthis->agents(); i++)
        curr_start_id[i] = 0;
    }


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
        return;
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
        return;
      }
    }


    // Handle autosave
    if(pthis->get_autosave() != 0 && curr_measure->count_starts() != 0 && (curr_measure->count_starts() == pthis->get_autosave() || measure_end)) {

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
        return;
      }

      // Set measure times
      for(size_t i = 0; i < pthis->agents(); i++) {
        uint64_t measure_begin = 0;
        uint64_t measure_time = 0;
        if(curr_measure->get_start(0)->times() > i)
          measure_begin = curr_measure->get_start(0)->get_window_begin(i);
        if(curr_measure->get_start(curr_measure->count_starts()-1)->times() > i)
          measure_time = curr_measure->get_start(curr_measure->count_starts()-1)->get_window_begin(i) +
                         curr_measure->get_start(curr_measure->count_starts()-1)->get_window_time(i) - measure_begin;
        curr_measure->add_time(measure_begin, measure_time);
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

      // We format the filename
      std::stringstream file_number(std::stringstream::out);
      file_number.width(4);
      file_number.fill('0');
      file_number << pthis->get_counter(); // TODO implement counter
      std::string filename = pthis->get_prefix() + "_" + file_number.str();
      pthis->increment_counter();

      // Save measure
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
        return;
      }
    }

    if(measure_end) {
      // Reset end flags
      for(size_t i = 0; i < pthis->agents(); i++)
        agent_end[i] = false;

      // Set status
      pthis->status(ATMD_STATUS_IDLE);

      // Reset measure counter
      pthis->reset_counter();

      continue;
    }

    // If current start is NULL, create one
    if(curr_start[agent_id] == NULL) {
      if(packet.type() == ATMD_DT_FIRST || packet.type() == ATMD_DT_ONLY) {
        curr_start[agent_id] = new StartData;
        curr_start[agent_id]->add_time(packet.window_start(), packet.window_time());
        curr_start[agent_id]->set_tbin(pthis->get_tbin());
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

  // Autosave counter
  _auto_counter = 0;

  // Save format
  _format = ATMD_FORMAT_MATPS3;

}


/* @fn int VirtualBoard::save_measure(size_t measure_number, std::string filename)
 * Save a measure object to a file in the specified format.
 *
 * @param measure_number The number of the measure that we want to save.
 * @param filename The filename including complete path.
 * @return Return 0 on success, -1 on error.
 */
int VirtualBoard::save_measure(size_t measure_number, std::string filename) {

  std::fstream savefile;
  MatFile mat_savefile;

  // Check if the measure number is valid.
  if(measure_number < 0 || measure_number >= this->measures()) {
    syslog(ATMD_ERR, "VirtualBoard [save_measure]: trying to save a non existent measure.");
    return -1;
  }

  // For safety we remove all relative path syntax
  pcrecpp::RE("\\.\\.\\/").GlobalReplace("", &filename);
  if(filename[0] == '/')
  filename.erase(0,1);

  // Find 'user' uid
  int bufsize = sysconf(_SC_GETPW_R_SIZE_MAX);
  if (bufsize == -1)      // Value was indeterminate
      bufsize = 16384;    // Should be more than enough

  char * pwbuf = new char[bufsize];
  if(pwbuf == NULL) {
    syslog(ATMD_ERR, "VirtualBoard [save_measure]: cannot allocate buffer for getpwnam_r.");
    return -1;
  }

  uid_t uid = 0;
  struct passwd pwd;
  struct passwd *result;
  int rval = getpwnam_r("user", &pwd, pwbuf, bufsize, &result);
  if (result == NULL) {
    if(rval == 0)
      syslog(ATMD_WARN, "VirtualBoard [save_measure]: cannot find user 'user'.");
    else
      syslog(ATMD_WARN, "VirtualBoard [save_measure]: error retreiving 'user' UID (Error: %m).");
  } else {
    uid = pwd.pw_uid;
  }

  // For safety the supplied path is taken relative to /home/data
  std::string fullpath = "/home/data/";

  // Now we check that all path elements exists
  pcrecpp::StringPiece input(filename);
  pcrecpp::RE re("(\\w*)\\/");
  std::string dir;
  struct stat st;
  while (re.Consume(&input, &dir)) {
    fullpath.append(dir);
    fullpath.append("/");
    if(stat(fullpath.c_str(), &st) == -1) {
      if(errno == ENOENT) {
        // Path does not exist
        if(mkdir(fullpath.c_str(), S_IRWXU | S_IRWXG)) {
          syslog(ATMD_ERR, "VirtualBoard [save_measure]: error creating save path \"%s\" (Error: %m).", fullpath.c_str());
          return -1;
        }

        // Change owner to 'user'
        if(chown(fullpath.c_str(), uid, -1)) {
          syslog(ATMD_WARN, "VirtualBoard [save_measure]: error changing owner for director \"%s\" (Error %m).", fullpath.c_str());
        }

      } else {
        // Error
        syslog(ATMD_ERR, "VirtualBoard [save_measure]: error checking save path \"%s\" (Error: %m).", fullpath.c_str());
        return -1;
      }
    }
  }
  fullpath.append(input.as_string());

  // URL for FTP transfers
  std::string fullurl = "";
  if(_format == ATMD_FORMAT_MATPS2_FTP || _format == ATMD_FORMAT_MATPS2_ALL || _format == ATMD_FORMAT_MATPS3_FTP || _format == ATMD_FORMAT_MATPS3_ALL) {
    if(this->_hostname == "") {
      syslog(ATMD_ERR, "VirtualBoard [save_measure]: requested to save a measure using FTP but hostname is not set.");
      return -1;
    }

    fullurl.append("ftp://");
    if(this->_username != "") {
      fullurl.append(this->_username);
      if(this->_password != "") {
        fullurl.append(":");
        fullurl.append(this->_password);
      }
      fullurl.append("@");
    }
    fullurl.append(this->_hostname);
    fullurl.append("/");
    fullurl.append(filename);
  }

  if(_format != ATMD_FORMAT_MATPS2_FTP && _format != ATMD_FORMAT_MATPS3_FTP) {

    // Opening file
    switch(_format) {
      case ATMD_FORMAT_BINPS:
      case ATMD_FORMAT_BINRAW:
        syslog(ATMD_ERR, "VirtualBoard [save_measure]: binary format is not supported any more.");
        return -1;

      case ATMD_FORMAT_DEBUG:
        syslog(ATMD_ERR, "VirtualBoard [save_measure]: debug text format is not supported any more.");
        return -1;

      case ATMD_FORMAT_MATPS1:
      case ATMD_FORMAT_MATPS2:
      case ATMD_FORMAT_MATPS3:
      case ATMD_FORMAT_MATRAW:
      case ATMD_FORMAT_MATPS2_ALL:
      case ATMD_FORMAT_MATPS3_ALL:
        mat_savefile.open(fullpath);
        if(!mat_savefile.IsOpen()) {
          syslog(ATMD_ERR, "VirtualBoard [save_measure]: cannot open Matlab file %s.", fullpath.c_str());
          return -1;
        }
        break;

      case ATMD_FORMAT_RAW:
      case ATMD_FORMAT_PS:
      case ATMD_FORMAT_US:
        // Open in text format
        savefile.open(fullpath.c_str(), std::fstream::out | std::fstream::trunc);
        savefile.precision(15);
        if(!savefile.is_open()) {
          syslog(ATMD_ERR, "VirtualBoard [save_measure]: error opening text file \"%s\".", fullpath.c_str());
          return -1;
        }
        break;
    }

    syslog(ATMD_INFO, "VirtualBoard [save_measure]: saving measure %lu to file \"%s\".", measure_number, fullpath.c_str());
  }

  StartData* current_start;
  uint32_t start_count = this->_measures[measure_number]->count_starts();

  double stoptime;
  int8_t channel;
  uint32_t ref_count;

  // Matlab definitions
  MatVector<double> data("data", mxDOUBLE_CLASS);
  MatVector<uint32_t> data_st("start", mxUINT32_CLASS);
  MatVector<int8_t> data_ch("channel", mxINT8_CLASS);
  MatVector<double> data_stop("stoptime", mxDOUBLE_CLASS);
  MatVector<uint32_t> measure_begin("measure_begin", mxUINT32_CLASS);
  MatVector<uint32_t> measure_time("measure_time", mxUINT32_CLASS);
  MatVector<uint32_t> stat_times("stat_times", mxUINT32_CLASS);

  uint32_t num_events = 0, ev_ind = 0;

  std::stringstream txtbuffer(std::stringstream::in);

  switch(_format) {
    case ATMD_FORMAT_RAW:
    case ATMD_FORMAT_US:
    case ATMD_FORMAT_PS:
    default:

      // We make sure that the string stream is empty
      txtbuffer.str("");

      // We write an header
      if(_format == ATMD_FORMAT_RAW)
        txtbuffer << "start\tchannel\tslope\trefcount\tstoptime" << std::endl;
      else
        txtbuffer << "start\tchannel\tslope\tstoptime" << std::endl;

      // We cycle over all starts
      for(size_t i = 0; i < start_count; i++) {
        if(!(current_start = this->_measures[measure_number]->get_start(i))) {
          syslog(ATMD_ERR, "VirtualBoard [save_measure]: trying to save a non existent start.");
          return -1;
        }

        // We cycle over all stops of current start
        for(size_t j = 0; j < current_start->count_stops(); j++) {
          current_start->get_channel(j, channel);

          if(_format == ATMD_FORMAT_RAW) {
            current_start->get_rawstoptime(j, ref_count, stoptime);
            txtbuffer << i+1 << "\t" << (int32_t)channel << "\t" << ref_count << "\t" << stoptime << std::endl;

          } else {
            current_start->get_stoptime(j, stoptime);
            txtbuffer << i+1 << "\t" << (int32_t)channel << "\t" << ((_format == ATMD_FORMAT_US) ? (stoptime / 1e6) : stoptime) << std::endl;
          }
        }
      }

      // We write the string stream to the output file
      savefile << txtbuffer.str();
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
        if(!(current_start = this->_measures[measure_number]->get_start(i))) {
          syslog(ATMD_ERR, "VirtualBoard [save_measure]: trying to stat a non existent start.");
          return -1;
        }
        num_events += current_start->count_stops();
      }

      // Measure times
      measure_begin.resize(agents(),2);
      measure_time.resize(agents(),2);
      for(size_t i = 0; i < agents(); i++) {
        if(_measures[measure_number]->times() > i) {
          measure_begin(i,0) = (uint64_t)(this->_measures[measure_number]->get_begin(i) / 1000) / 1000000;
          measure_begin(i,1) = (uint64_t)(this->_measures[measure_number]->get_begin(i) / 1000) % 1000000;
          measure_time(i,0) = (uint64_t)(this->_measures[measure_number]->get_time(i) / 1000) / 1000000;
          measure_time(i,1) = (uint64_t)(this->_measures[measure_number]->get_time(i) / 1000) % 1000000;
        }
      }

      // Vector resizes
      if(_format == ATMD_FORMAT_MATPS1) {
        data.resize(num_events,3);
      } else {
        data_st.resize(num_events,1);
        data_ch.resize(num_events,1);
        data_stop.resize(num_events,1);
      }

      if(_format == ATMD_FORMAT_MATPS3 || _format == ATMD_FORMAT_MATPS3_FTP || _format == ATMD_FORMAT_MATPS3_ALL)
        stat_times.resize(start_count, 2 * agents());

      // Save data
      ev_ind = 0;
      for(size_t i = 0; i < start_count; i++) {
        if(!(current_start = this->_measures[measure_number]->get_start(i))) {
          syslog(ATMD_ERR, "VirtualBoard [save_measure]: trying to save a non existent start.");
          return -1;
        }
        // Save data
        for(size_t j = 0; j < current_start->count_stops(); j++) {
          current_start->get_channel(j, channel);
          current_start->get_stoptime(j, stoptime);

          if(_format == ATMD_FORMAT_MATPS1) {
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

        // Save start times
        if(_format == ATMD_FORMAT_MATPS3 || _format == ATMD_FORMAT_MATPS3_FTP || _format == ATMD_FORMAT_MATPS3_ALL) {
          size_t k = current_start->times();
          if(k > agents()) {
            k = agents();
            syslog(ATMD_WARN, "VirtualBoard [save_measure]: found a start that had more timings than the number of agents.");
          }
          for(size_t j = 0; j < k; j++) {
            stat_times(i,2*j) = (uint32_t)( current_start->get_window_begin(j) / 1000 );
            stat_times(i,2*j+1) = (uint32_t)( current_start->get_window_time(j) / 1000 );
          }
        }
      }


      if(_format != ATMD_FORMAT_MATPS2_FTP && _format != ATMD_FORMAT_MATPS3_FTP) {
        // Write vectors to file
        measure_begin.write(mat_savefile);
        measure_time.write(mat_savefile);

        if(_format == ATMD_FORMAT_MATPS1) {
          data.write(mat_savefile);

        } else {
          data_st.write(mat_savefile);
          data_ch.write(mat_savefile);
          data_stop.write(mat_savefile);
        }

        if(_format == ATMD_FORMAT_MATPS3 || _format == ATMD_FORMAT_MATPS3_ALL)
          stat_times.write(mat_savefile);
      }


      if(_format == ATMD_FORMAT_MATPS2_FTP || _format == ATMD_FORMAT_MATPS2_ALL || _format == ATMD_FORMAT_MATPS3_FTP || _format == ATMD_FORMAT_MATPS3_ALL) {

        MatObj matobj;

        matobj.add_obj((MatMatrix*) &measure_begin);
        matobj.add_obj((MatMatrix*) &measure_time);
        matobj.add_obj((MatMatrix*) &data_st);
        matobj.add_obj((MatMatrix*) &data_ch);
        matobj.add_obj((MatMatrix*) &data_stop);
        if(_format == ATMD_FORMAT_MATPS3 || _format == ATMD_FORMAT_MATPS3_FTP)
          matobj.add_obj((MatMatrix*) &stat_times);

        // Configure FTP in binary mode
        struct curl_slist *headerlist = NULL;
        headerlist = curl_slist_append(headerlist, "TYPE I");
        curl_easy_setopt(this->easy_handle, CURLOPT_QUOTE, headerlist);
        curl_easy_setopt(this->easy_handle, CURLOPT_FTP_CREATE_MISSING_DIRS, CURLFTP_CREATE_DIR_RETRY);

        // Setup of CURL options
        curl_easy_setopt(this->easy_handle, CURLOPT_READFUNCTION, this->curl_read);
        curl_easy_setopt(this->easy_handle, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(this->easy_handle, CURLOPT_URL, fullurl.c_str());
        curl_easy_setopt(this->easy_handle, CURLOPT_READDATA, (void*) &matobj);
        curl_easy_setopt(this->easy_handle, CURLOPT_INFILESIZE_LARGE, (curl_off_t)matobj.total_size());

        syslog(ATMD_INFO, "VirtualBoard [save_measure]: remotely saving measurement to \"%s/%s\".", this->_hostname.c_str(), filename.c_str());

        if(enable_debug)
          syslog(ATMD_DEBUG, "VirtualBoard [save_measure]: full url: %s.", fullurl.c_str());

        // Start network transfer
        struct timeval t_begin, t_end;
        gettimeofday(&t_begin, NULL);

        if(curl_easy_perform(this->easy_handle)) {
          syslog(ATMD_ERR, "VirtualBoard [save_measure]: failed to transfer file with libcurl with error \"%s\".", this->curl_error);
          return -1;
        }

        // We measure elapsed time for statistic purposes
        gettimeofday(&t_end, NULL);
        syslog(ATMD_INFO, "VirtualBoard [save_measure]: remote save performed in %fs.", (double)(t_end.tv_sec - t_begin.tv_sec) + (double)(t_end.tv_usec - t_begin.tv_usec) / 1e6);
      }
      break;

    case ATMD_FORMAT_MATRAW: /* Raw Matlab format with separate variables for start, channel, retriger count and stoptime */
      syslog(ATMD_ERR, "VirtualBoard [save_measure]: raw MAT format is not yet implemented.");
      return -1;
      break;
  }


  switch(_format) {
    case ATMD_FORMAT_RAW:
    case ATMD_FORMAT_PS:
    case ATMD_FORMAT_US:
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
  if(_format != ATMD_FORMAT_MATPS2_FTP && _format != ATMD_FORMAT_MATPS3_FTP)
    if(chown(fullpath.c_str(), uid, -1))
      syslog(ATMD_ERR, "Measure [save_measure]: cannot change owner of file \"%s\" (Error: %m).", fullpath.c_str());

  return 0;
}


/* @fn VirtualBoard::curl_read(void *ptr, size_t size, size_t count, void *data)
 * Read callback for CURL network transfers
 *
 * @param ptr Pointer to the curl output buffer
 * @param size Size of the data block
 * @param count Number of data block to copy
 * @param data Pointer to the input data structure
 * @return Return the number of bytes copied to ptr buffer
 */
size_t VirtualBoard::curl_read(void *ptr, size_t size, size_t count, void *data) {
  // Restore pointer to buffer object
  struct MatObj* matobj = (MatObj*)data;

  size_t bn = matobj->get_bytes((uint8_t*)ptr, size*count);
  // Return number of byte copied to buffer
  return bn;
}


/* @fn int VirtualBoard::start_measure()
 *
 */
int VirtualBoard::start_measure() {
  // Check status
  if(_status != ATMD_STATUS_IDLE)
    return -1;

  AgentMsg packet;

  // Set status
  _status = ATMD_STATUS_RUNNING;

  // First send configuration to agents
  for(size_t i = 0; i < agents(); i++) {
    packet.clear();
    packet.type(ATMD_CMD_MEAS_SET);

    // Manage agent ID
    packet.agent_id(i);

    // Build channel masks
    uint8_t rising_mask = 0;
    uint8_t falling_mask = 0;
    for(size_t j = 0; j < 8; j++) {
      if(_ch_rising[j+i*8])
        rising_mask = rising_mask | (0x1 << i);
      if(_ch_falling[j+i*8])
        falling_mask = falling_mask | (0x1 << i);
    }

    // Channel info
    packet.start_rising(_start_rising);
    packet.start_falling(_start_falling);
    packet.rising_mask(rising_mask);
    packet.falling_mask(falling_mask);

    // Timing info
    packet.measure_time(_measure_time.get_nsec());
    packet.window_time(_window_time.get_nsec());
    packet.timeout(_timeout_time.get_nsec());
    packet.deadtime(_deadtime.get_nsec());

    // Board parameters
    packet.start_offset(_offset);
    packet.refclk(_refclk);
    packet.hsdiv(_hsdiv);

    // Send configuration packet
    try {
      send_command(packet);
    } catch(int e) {
      syslog(ATMD_CRIT, "VirtualBoard [start_measure]: failed to send a command to the queue.");
      // Terminate server
      terminate_interrupt = true;
      return -1;
    }

    // Wait for answer
    packet.clear();
    try {
      recv_command(packet);
    } catch(int e) {
      syslog(ATMD_CRIT, "VirtualBoard [start_measure]: failed to receive a command to the queue.");
      // Terminate server
      terminate_interrupt = true;
      return -1;
    }

    packet.decode();
    if(packet.type() == ATMD_CMD_ERROR) {
      syslog(ATMD_ERR, "VirtualBoard [start_measure]: error sending configuration to agent with address '%s'.", ether_ntoa(get_agent(i).agent_addr()));
      _status = ATMD_STATUS_ERR;
      return -1;
    } else if(packet.type() == ATMD_CMD_BUSY) {
      syslog(ATMD_ERR, "VirtualBoard [start_measure]: agent with address '%s' was busy while sending configuration.", ether_ntoa(get_agent(i).agent_addr()));
      _status = ATMD_STATUS_ERR;
      return -1;
    }
  }

  // Start measure with a broadcast
  packet.clear();
  packet.type(ATMD_CMD_MEAS_CTR);
  packet.action(ATMD_ACTION_START);
  try {
    send_command(packet);
  } catch(int e) {
    syslog(ATMD_CRIT, "VirtualBoard [start_measure]: failed to send a command to the queue.");
    // Terminate server
    terminate_interrupt = true;
    return -1;
  }

  // Wait for answer
  packet.clear();
  try {
    recv_command(packet);
  } catch(int e) {
    syslog(ATMD_CRIT, "VirtualBoard [start_measure]: failed to receive a command to the queue.");
    // Terminate server
    terminate_interrupt = true;
    return -1;
  }

  packet.decode();
  if(packet.type() == ATMD_CMD_ACK)
    return 0;

  if(packet.type() == ATMD_CMD_ERROR) {
    syslog(ATMD_ERR, "VirtualBoard [start_measure]: error sending start command.");
    _status = ATMD_STATUS_ERR;
    return -1;
  }

  if(packet.type() == ATMD_CMD_BUSY) {
    syslog(ATMD_ERR, "VirtualBoard [start_measure]: agents where busy while sending start command.");
    _status = ATMD_STATUS_ERR;
    return -1;
  }

  syslog(ATMD_ERR, "VirtualBoard [start_measure]: got bad answer type while sending start command.");
  _status = ATMD_STATUS_ERR;
  return -1;
}


/* @fn int VirtualBoard::stop_measure()
 *
 */
int VirtualBoard::stop_measure() {

  if(_status != ATMD_STATUS_RUNNING)
    return -1;

  AgentMsg packet;

  // Start measure with a broadcast
  packet.clear();
  packet.type(ATMD_CMD_MEAS_CTR);
  packet.action(ATMD_ACTION_STOP);
  try {
    send_command(packet);
  } catch(int e) {
    syslog(ATMD_CRIT, "VirtualBoard [start_measure]: failed to send a command to the queue.");
    // Terminate server
    terminate_interrupt = true;
    return -1;
  }

  // Wait for answer
  packet.clear();
  try {
    recv_command(packet);
  } catch(int e) {
    syslog(ATMD_CRIT, "VirtualBoard [start_measure]: failed to receive a command to the queue.");
    // Terminate server
    terminate_interrupt = true;
    return -1;
  }

  packet.decode();
  if(packet.type() == ATMD_CMD_ACK)
    return 0;

  if(packet.type() == ATMD_CMD_ERROR) {
    syslog(ATMD_ERR, "VirtualBoard [start_measure]: error sending start command.");
    _status = ATMD_STATUS_ERR;
    return -1;
  }

  if(packet.type() == ATMD_CMD_BUSY) {
    syslog(ATMD_ERR, "VirtualBoard [start_measure]: agents where busy while sending start command.");
    _status = ATMD_STATUS_ERR;
    return -1;
  }

  syslog(ATMD_ERR, "VirtualBoard [start_measure]: got bad answer type while sending start command.");
  _status = ATMD_STATUS_ERR;
  return -1;
}