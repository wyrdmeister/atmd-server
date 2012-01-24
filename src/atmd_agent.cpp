/*
 * ATMD Server version 3.0
 *
 * ATMD Agent - Main
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

// Termination interrupt flag
bool terminate_interrupt;

// Debug flag
#ifdef DEBUG
bool enable_debug;
#endif

#include "atmd_agent.h"


// Array of reasons to get SIGDEBUG (aka SIGXCPU)
#define SIGDEBUG_UNDEFINED          0
#define SIGDEBUG_MIGRATE_SIGNAL     1
#define SIGDEBUG_MIGRATE_SYSCALL    2
#define SIGDEBUG_MIGRATE_FAULT      3
#define SIGDEBUG_MIGRATE_PRIOINV    4
#define SIGDEBUG_NOMLOCK            5
#define SIGDEBUG_WATCHDOG           6

static const char *reason_str[] = {
  "undefined",
  "received signal",
  "invoked syscall",
  "triggered fault",
  "affected by priority inversion",
  "missing mlockall",
  "runaway thread",
};


/* @fn void gen_handler(int signal, siginfo_t *si, void* context)
 * Global signal handler
 * Log most common abort signals and get a backtrace in case of SIGDEBUG.
 */
void gen_handler(int signal, siginfo_t *si, void* context) {
  if(signal == SIGHUP) {
    terminate_interrupt = true;

  } else if(signal == SIGDEBUG) {
    // This signal should be ignored as they are sent
    // every time a task switch to secondary mode
    unsigned int reason = si->si_value.sival_int;
    void *bt[32];
    int nentries;
    ucontext_t *uc = (ucontext_t *)context;

    // Log SIGDEBUG
    syslog(ATMD_WARN, "\nSIGDEBUG received, reason %d: %s\n", reason, reason <= SIGDEBUG_WATCHDOG ? reason_str[reason] : "<unknown>");

    nentries = backtrace(bt,32);
    // overwrite sigaction with caller's address
#if defined(__i386__)
    bt[1] = (void *) uc->uc_mcontext.gregs[REG_EIP];
#elif defined(__x86_64__)
    bt[1] = (void *) uc->uc_mcontext.gregs[REG_RIP];
#endif
    char ** messages = backtrace_symbols(bt, nentries);

    // Print backtrace
    for(int i = 1; i < nentries; i++)
      syslog(ATMD_INFO, "[BT] %s#", messages[i]);

  } else {
    switch(signal) {
      case SIGINT:
        syslog(ATMD_CRIT, "Received signal SIGINT. Terminating.");
        break;
      case SIGFPE:
        syslog(ATMD_CRIT, "Received signal SIGFPE. Terminating.");
        break;
      case SIGSEGV:
        syslog(ATMD_CRIT, "Received signal SIGSEGV. Terminating.");
        break;
      case SIGPIPE:
        syslog(ATMD_CRIT, "Received signal SIGPIPE. Terminating.");
        break;
      default:
        syslog(ATMD_CRIT, "Received signal %d. Terminating.", signal);
        break;
    }

    exit(signal);
  }
}


/* @fn int main(int argc, char * const argv[])
 * Main entry point
 */
int main(int argc, char * const argv[]) {

  int retval = 0;

  // The first thing to do is to initialize syslog
  openlog("atmd-agent", 0, LOG_DAEMON);
  syslog(LOG_DAEMON | LOG_INFO, "Starting atmd-agent version %s.", VERSION);

  // Reset interrupt flag
  terminate_interrupt = false;

  // Configuration of signal handler for SIGHUP (server termination)
  struct sigaction handler;
  sigemptyset(&handler.sa_mask);
  handler.sa_flags = SA_SIGINFO;
  handler.sa_sigaction = gen_handler;

  // Catch SIGHUP for the termination of the process
  sigaction(SIGHUP, &handler, NULL);

  // Catch SIGXCPU and SIGDEBUG
  sigaction(SIGDEBUG, &handler, NULL);

  // Catch some bad signals to log them...
  sigaction(SIGINT,  &handler, NULL);
  sigaction(SIGFPE,  &handler, NULL);
  sigaction(SIGSEGV, &handler, NULL);
  sigaction(SIGPIPE, &handler, NULL);


  // Handling of command line parameters through getopt
  /* Accepted options:
   * -d: debug flag.
   * -n <tcp_port>: listening port, default to 2606.
   * -i <ip_address>: ip address to listen to, default to ANY_IP.
   * -p <pid_file>: file in which store the pid number, default to /var/run/atmd_server.pid.
   */

  // Pid file
  std::string pid_filename = ATMD_PID_FILE;
  std::string conf_filename = ATMD_CONF_FILE;
  std::ofstream pid_file;

#ifdef DEBUG
  enable_debug = false;
#endif

  int c;
  opterr = 0;
  pcrecpp::RE re("");
  while( (c = getopt(argc, argv, "dp:c:")) != -1 ) {
    switch(c) {
      case 'd':
#ifdef DEBUG
        enable_debug = true;
#else
        syslog(ATMD_WARN, "Debug option is not supported. Ignoring.");
#endif
        break;

      case 'p':
        re = "(^\\/[a-zA-Z0-9\\.\\_\\/]+)";
        if(!re.FullMatch(optarg, &pid_filename)) {
          syslog(ATMD_WARN, "Supplied an invalid pid file name (%s), using default.", optarg);
          pid_filename = ATMD_PID_FILE;
        }
        if(pid_filename.empty())
          pid_filename = ATMD_PID_FILE;
        break;

      case 'c':
        re = "(^\\/[a-zA-Z0-9\\.\\_\\/]+)";
        if(!re.FullMatch(optarg, &conf_filename)) {
          syslog(ATMD_WARN, "Supplied an invalid configuration file name (%s), using default.", optarg);
          conf_filename = ATMD_CONF_FILE;
        }
        if(conf_filename.empty())
          conf_filename = ATMD_CONF_FILE;
        break;

      case '?':
        syslog(ATMD_WARN, "Supplied unknown command line option \"%s\".", argv[optind-1]);
        break;
    }
  }

  // Opening pid file
  pid_file.open(pid_filename.c_str());
  if(!pid_file.is_open()) {
    if(pid_filename == ATMD_PID_FILE) {
      syslog(ATMD_CRIT, "Could not open pid file %s. Exiting.", pid_filename.c_str());
      exit(-1);
    } else {
      syslog(ATMD_ERR, "Could not open user supplied pid file %s. Trying default.", pid_filename.c_str());
      pid_filename = ATMD_PID_FILE;
      pid_file.open(pid_filename.c_str());
      if(!pid_file.is_open()) {
        syslog(ATMD_CRIT, "Could not open pid file %s. Exiting.", pid_filename.c_str());
        exit(-1);
      }
    }
  }


  // Read configuration
  AtmdConfig server_conf;
  if(server_conf.read(conf_filename)) {
    syslog(ATMD_CRIT, "Cannot open configuration file '%s'. Exiting.", conf_filename.c_str());
    exit(-1);
  }


  // Search for ATMD-GPX boards
  uint16_t board_addresses[8];
  retval = ATMDboard::search_board(board_addresses, 8);
  if(retval < 0) {
    // No board found
    exit(-1);
  }

  // Init board object
  ATMDboard board;
  board.init(board_addresses[0]);


  // Board found, so we can fork to background
  pid_t child_pid = fork();
  if(child_pid == -1) {
    // Fork failed.
    syslog(ATMD_CRIT, "Terminating atmd-server version %s because fork failed with error \"%m\".", VERSION);
    exit(-1);

  } else if(child_pid != 0) {
    // We are still in the parent. We save the child pid and then exit.
    pid_file << child_pid;
    pid_file.close();
    syslog(ATMD_INFO, "Fork to background successful. Child pid %d saved in %s.", child_pid, pid_filename.c_str());
    exit(0);
  }

  // Here we are in the child

  // Lock memory
  mlockall(MCL_CURRENT|MCL_FUTURE);

  // DAEMONIZE CHILD PROCESS

  // Change umask to 0
  umask(0);

  // Change session id to detach from controlling tty
  setsid();

  // Change current directory to root
  if(chdir("/"))
    syslog(ATMD_WARN, "Cannot change working directory to \"/\" (Error: %m).");

  // Redirect standard streams to /dev/null
  if(!freopen( "/dev/null", "r", stdin))
    syslog(ATMD_WARN, "Cannot redirect stdin to \"/dev/null\" (Error: %m).");
  if(!freopen( "/dev/null", "w", stdout))
    syslog(ATMD_WARN, "Cannot redirect stdout to \"/dev/null\" (Error: %m).");
  if(!freopen( "/dev/null", "w", stderr))
    syslog(ATMD_WARN, "Cannot redirect stderr to \"/dev/null\" (Error: %m).");

  // Shadow to RT task
  retval = rt_task_shadow(NULL, "atmd-agent", 50, T_FPU);
  if(retval) {
    switch(retval) {
      case -EBUSY:
        rt_syslog(ATMD_WARN, "The current Linux task is already mapped to a Xenomai context. This is unexpected.\n");
        break;

      case -ENOMEM:
        syslog(ATMD_CRIT, "The system fails to get enough dynamic memory from the global real-time heap in order to create or register the task.");
        exit(retval);

      case -EEXIST:
        syslog(ATMD_CRIT, "The name is already in use by some registered object.\n");
        exit(retval);

      case -EPERM:
        syslog(ATMD_CRIT, "This service was called from an asynchronous context.\n");
        exit(retval);

      default:
        syslog(ATMD_CRIT, "rt_task_shadow failed with an unexpected code (%d)\n.", retval);
        exit(retval);
    }
  }

  // Init rt_printf and rt_syslog
  rt_print_auto_init(1);
  rt_syslog(ATMD_INFO, "Successfully switched to real-time domain.");


  // Crete RTnet socket
  RTnet ctrl_sock;
  ctrl_sock.rtskbs(server_conf.rtskbs());
  ctrl_sock.protocol(ATMD_PROTO_CTRL);
  ctrl_sock.interface(server_conf.rtif());

  if(ctrl_sock.init(true)) {
    rt_syslog(ATMD_CRIT, "Failed to initialize RTnet control socket. Terminating.");
    exit(-1);
  }


  // Agent message object
  AgentMsg ctrl_packet;

  // Wait for master broadcast
  struct ether_addr master_addr;
  do {
    // Check for termination
    if(terminate_interrupt) {
      ctrl_sock.close();
      exit(0);
    }

    try {
      ctrl_sock.recv(ctrl_packet, &master_addr);

    } catch(int e) {
      rt_syslog(ATMD_CRIT, "Failed to receive a packet waiting for master broadcast. Terminating.");
      exit(-1);
    }

    // Check master broadcast
    if(ctrl_packet.decode())
      // Bad packet
      continue;

    if(ctrl_packet.type() != ATMD_CMD_BRD)
      // Not a broadcast
      continue;

    if(strncmp(ctrl_packet.version(), VERSION, ATMD_VER_LEN)) {
      rt_syslog(ATMD_WARN, "Received a broadcast start message from an ATMD server with wrong version (%s != %s).", ctrl_packet.version(), VERSION);
      continue;
    } else {
      break;
    }

  } while(true);


  // Prepare answer to master
  ctrl_packet.clear();
  ctrl_packet.type(ATMD_CMD_HELLO);
  ctrl_packet.version(VERSION);
  ctrl_packet.encode();

  // Answer to master
  try {
    ctrl_sock.send(ctrl_packet, &master_addr);

  } catch(int e) {
    rt_syslog(ATMD_CRIT, "Failed to send a packet while answering master broadcast. Terminating.");
    exit(-1);
  }

  // Create data socket
  RTnet data_sock;
  data_sock.protocol(ATMD_PROTO_DATA);
  data_sock.rtskbs(server_conf.rtskbs());
  if(data_sock.init()) {
    rt_syslog(ATMD_CRIT, "Failed to initialize RTnet data socket. Terminating.");
    ctrl_sock.close();
    exit(-1);
  }

  // Create RT_HEAP
  RT_HEAP data_heap;
  retval = rt_heap_create(&data_heap, ATMD_RT_HEAP_NAME, 200000000, H_MAPPABLE);
  if(retval) {
    switch(retval) {
      case -EEXIST:
        rt_syslog(ATMD_CRIT, "rt_heap_create(): the name is already in use.");
        break;

      case -EINVAL:
        rt_syslog(ATMD_CRIT, "rt_heap_create(): wrong heap size.");
        break;

      case -ENOMEM:
        rt_syslog(ATMD_CRIT, "rt_heap_create(): not enough memory available.");
        break;

      case -EPERM:
        rt_syslog(ATMD_CRIT, "rt_heap_create(): invalid context.");
        break;

      case -ENOSYS:
        rt_syslog(ATMD_CRIT, "rt_heap_create(): real-time support not available in userspace.");
        break;

      default:
        rt_syslog(ATMD_CRIT, "rt_heap_create(): unexpected return code (%d).", retval);
        break;
    }
    ctrl_sock.close();
    data_sock.close();
    exit(-1);
  }

  // Create RT_QUEUE
  RT_QUEUE ctrl_queue;
  retval = rt_queue_create(&ctrl_queue, ATMD_RT_QUEUE_NAME, 200000, Q_UNLIMITED, Q_SHARED);
  if(retval) {
    switch(retval) {
      case -EEXIST:
        rt_syslog(ATMD_CRIT, "rt_queue_create(): the name is already in use.");
        break;

      case -EINVAL:
        rt_syslog(ATMD_CRIT, "rt_queue_create(): invalid pool size.");
        break;

      case -ENOMEM:
        rt_syslog(ATMD_CRIT, "rt_queue_create(): not enough memory available.");
        break;

      case -EPERM:
        rt_syslog(ATMD_CRIT, "rt_queue_create(): invalid context.");
        break;

      case -ENOSYS:
        rt_syslog(ATMD_CRIT, "rt_queue_create(): real-time support not available in userspace.");
        break;

      case -ENOENT:
        rt_syslog(ATMD_CRIT, "rt_queue_create(): cannot open /dev/rtheap.");
        break;

      default:
        rt_syslog(ATMD_CRIT, "rt_queue_create(): unexpected return code (%d).", retval);
        break;
    }
    ctrl_sock.close();
    data_sock.close();
    exit(-1);
  }

  // Init thread params
  InitData th_info;
  th_info.heap_name = ATMD_RT_HEAP_NAME;
  th_info.queue_name = ATMD_RT_QUEUE_NAME;
  th_info.board = &board;
  th_info.sock = &data_sock;
  th_info.addr = &master_addr;

  // RT measurement thread
  RT_TASK meas_th;
  retval = rt_task_spawn(&meas_th, ATMD_RT_THREAD_NAME, 0, 98, T_FPU | T_JOINABLE, atmd_measure, (void*)&th_info);
  if(retval) {
    switch(retval) {
      case -ENOMEM:
        rt_syslog(ATMD_CRIT, "rt_task_spawn(): not enough memory available to create task.");
        break;

      case -EEXIST:
        rt_syslog(ATMD_CRIT, "rt_task_spawn(): the name is already in use.");
        break;

      case -EPERM:
        rt_syslog(ATMD_CRIT, "rt_task_spawn(): invalid context.");
        break;

      default:
        rt_syslog(ATMD_CRIT, "rt_task_spawn(): unexpected return code (%d).", retval);
        break;
    }
    ctrl_sock.close();
    data_sock.close();
    exit(-1);
  }

  // Remote address
  struct ether_addr remote_addr;

  // Measure parameters object
  MeasureDef measure_info;
  MeasureDef * minfo;

  // Cycle waiting for commands
  while(true) {

    // Termination
    if(terminate_interrupt)
      break;

    // Wait for a control packet
    try {
      ctrl_sock.recv(ctrl_packet, &remote_addr);
    } catch(int e) {
      rt_syslog(ATMD_CRIT, "Failed to receive packet from master. Terminating.");
      // TODO: add cleanup of RT tasks
      ctrl_sock.close();
      data_sock.close();
      exit(-1);
    }

    // Verify sender (if it's not our master we should ignore the packet)
    if(!memcmp(&remote_addr, &master_addr, sizeof(struct ether_addr)))
      // Bad address
      continue;

    // Decode packet
    ctrl_packet.decode();

    // Act based on packet type
    switch(ctrl_packet.type()) {

      case ATMD_CMD_BRD:
      case ATMD_CMD_HELLO:
      case ATMD_CMD_ACK:
        // Ignore these packet types
        break;

      case ATMD_CMD_MEAS_SET:
        // Store configuration into board
        // Init measure
        board.start_rising(ctrl_packet.start_rising());
        board.start_falling(ctrl_packet.start_falling());
        board.set_rising_mask(ctrl_packet.rising_mask());
        board.set_falling_mask(ctrl_packet.falling_mask());
        board.start_offset(ctrl_packet.start_offset());
        board.set_resolution(ctrl_packet.refclk(), ctrl_packet.hsdiv());

         // Set measure info
        measure_info.measure_time(ctrl_packet.measure_time());
        measure_info.window_time(ctrl_packet.window_time());
        measure_info.timeout(ctrl_packet.timeout());
        measure_info.deadtime(ctrl_packet.deadtime());

        // Configure board
        if(board.config())
          // PLL not locked. Abort measure
          board.status(ATMD_STATUS_ERR);

        // Send back ACK
        ctrl_packet.clear();
        ctrl_packet.type(ATMD_CMD_ACK);
        ctrl_packet.encode();
        try {
          ctrl_sock.send(ctrl_packet, &master_addr);
        } catch(int e) {
          rt_syslog(ATMD_CRIT, "Failed to receive packet from master. Terminating.");
          // TODO: add cleanup of RT tasks
          ctrl_sock.close();
          data_sock.close();
          exit(-1);
        }
        break;

      case ATMD_CMD_MEAS_CTR:
        switch(ctrl_packet.action()) {
          case ATMD_ACTION_START:
            // Start a new measurement
            if(board.status() == ATMD_STATUS_IDLE) {

              // 1) enqueue the MeasureDef struct
              minfo = static_cast<MeasureDef*>(rt_queue_alloc(&ctrl_queue, sizeof(MeasureDef)));
              if(!minfo) {
                rt_syslog(ATMD_CRIT, "rt_queue_alloc(): failed.");
                // TODO: add cleanup!
                exit(-1);
              }
              ::new(minfo) MeasureDef(measure_info);
              minfo->tdma_cycle(ctrl_packet.tdma_cycle());
              retval = rt_queue_send(&ctrl_queue, (void*)minfo, sizeof(MeasureDef), Q_NORMAL);
              if(retval) {
                switch(retval) {
                  case -EINVAL:
                  case -EIDRM:
                    rt_syslog(ATMD_CRIT, "rt_queue_send(): invalid queue descriptor.");
                    // TODO: add cleanup!
                    exit(-1);

                  case -ENOMEM:
                    rt_syslog(ATMD_CRIT, "rt_queue_send(): not enough memory available.");
                    // TODO: add cleanup!
                    exit(-1);

                  default:
                    rt_syslog(ATMD_CRIT, "rt_queue_send(): failed with unexpected return value (%d).", retval);
                    // TODO: add cleanup!
                    exit(-1);
                }
              }

              // 2) Send ACK
              ctrl_packet.clear();
              ctrl_packet.type(ATMD_CMD_ACK);
              ctrl_packet.encode();
              try {
                ctrl_sock.send(ctrl_packet, &master_addr);
              } catch(int e) {
                rt_syslog(ATMD_CRIT, "Failed to receive packet from master. Terminating.");
                // TODO: add cleanup of RT tasks
                ctrl_sock.close();
                data_sock.close();
                exit(-1);
              }

              // 3) resume measurement thread
              retval = rt_task_resume(&meas_th);
              if(retval) {
                switch(retval) {
                  case -EINVAL:
                  case -EIDRM:
                    rt_syslog(ATMD_CRIT, "rt_task_resume(): failed. Invalid task descriptor.");
                    exit(-1);
                  default:
                    rt_syslog(ATMD_CRIT, "rt_task_resume(): failed with unexpected return value (%d).", retval);
                    exit(-1);
                }
              }

            } else {
              // Send back BUSY or ERROR
              ctrl_packet.clear();
              ctrl_packet.type( (board.status() == ATMD_STATUS_ERR) ? ATMD_CMD_ERROR : ATMD_CMD_BUSY );
              ctrl_packet.encode();
              try {
                ctrl_sock.send(ctrl_packet, &master_addr);
              } catch(int e) {
                rt_syslog(ATMD_CRIT, "Failed to receive packet from master. Terminating.");
                // TODO: add cleanup of RT tasks
                ctrl_sock.close();
                data_sock.close();
                exit(-1);
              }
            }

            // Send ack to master
            break;

          case ATMD_ACTION_STOP:
            if(board.status() == ATMD_STATUS_RUNNING) {
              board.stop(true);
              // TODO: send back ACK
            } else {
              // TODO: send back error
            }
            break;

          default:
            // Undefined action... ignore
            rt_syslog(ATMD_WARN, "Received an ATMD_CMD_MEAS_CTR packet with unknown action (%d).", ctrl_packet.action());
            break;
        }
        break;

      default:
        // Ignore...
        break;
    }
  }

  // Wait on measurement thread termination
  rt_task_join(&meas_th);

  // Cleanup
  // TODO

  return 0;
}
