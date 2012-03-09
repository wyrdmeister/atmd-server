/*
 * ATMD Server version 3.0
 *
 * ATMD Server - Main
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

#include "atmd_server.h"


// Array of reasons to get SIGDEBUG (or SIGXCPU)
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
  unsigned int reason = 0;
  switch(signal) {
    case SIGHUP:
      terminate_interrupt = true;
      return;

    case SIGDEBUG:
      // These two signal should be ignored as they are sent
      // every time a task switch to secondary mode
      reason = si->si_value.sival_int;
      syslog(ATMD_WARN, "\nSIGDEBUG received, reason %d: %s\n", reason, reason <= SIGDEBUG_WATCHDOG ? reason_str[reason] : "<unknown>");
      break;

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

  // Getting backtrace
  void *bt[32];
  int nentries;
  ucontext_t *uc = (ucontext_t *)context;

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

  if(signal != SIGDEBUG)
    exit(signal);
}


/* @fn int main(int argc, char * const argv[])
 * Main entry point
 */
int main(int argc, char * const argv[])
{

  // The first thing to do is to initialize syslog
  openlog("atmd-server", 0, LOG_DAEMON);
  syslog(LOG_DAEMON | LOG_INFO, "Starting atmd-server version %s.", VERSION);

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
  sigaction(SIGINT, &handler, NULL);
  sigaction(SIGFPE, &handler, NULL);
  sigaction(SIGSEGV, &handler, NULL);
  sigaction(SIGPIPE, &handler, NULL);


  // Handling of command line parameters through getopt
  /* Accepted options:
   * -d: debug flag.
   * -n <tcp_port>: listening port, default to 2606.
   * -i <ip_address>: ip address to listen to, default to ANY_IP.
   * -p <pid_file>: file in which store the pid number, default to /var/run/atmd_server.pid.
   * -s: enable board simulation.
   */

  uint16_t listen_port = ATMD_DEF_PORT;
  std::string ip_address = ATMD_DEF_LISTEN;

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
  while( (c = getopt(argc, argv, "dn:i:p:c:")) != -1 ) {
    switch(c) {
      case 'd':
#ifdef DEBUG
        enable_debug = true;
#else
        syslog(ATMD_WARN, "Debug option is not supported. Ignoring.");
#endif
        break;

      case 'n':
        re = "(\\d+)";
        if(!re.FullMatch(optarg, &listen_port)) {
          syslog(ATMD_WARN, "Supplied an invalid port number (%s), using default.", optarg);
          listen_port = ATMD_DEF_PORT;
        }
        if(listen_port <= 1024) {
          syslog(ATMD_ERR, "Listening port could not be below 1024, using default.");
          listen_port = ATMD_DEF_PORT;
        }
        break;

      case 'i':
        re = "(\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3})";
        if(!re.FullMatch(optarg, &ip_address)) {
          syslog(ATMD_WARN, "Supplied an invalid ip address (%s), listening to all address.", optarg);
          ip_address = ATMD_DEF_LISTEN;
        }
        if(ip_address.empty())
          ip_address = ATMD_DEF_LISTEN;
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
      return -1;
    } else {
      syslog(ATMD_ERR, "Could not open user supplied pid file %s. Trying default.", pid_filename.c_str());
      pid_filename = ATMD_PID_FILE;
      pid_file.open(pid_filename.c_str());
      if(!pid_file.is_open()) {
        syslog(ATMD_CRIT, "Could not open pid file %s. Exiting.", pid_filename.c_str());
        return -1;
      }
    }
  }


  // Read configuration
  AtmdConfig server_conf;
  if(server_conf.read(conf_filename)) {
    syslog(ATMD_CRIT, "Cannot open configuration file '%s'. Exiting.", conf_filename.c_str());
    return -1;
  }


  // Board found. We can fork.
  pid_t child_pid = fork();
  if(child_pid == -1) {
    // Fork failed.
    syslog(ATMD_CRIT, "Terminating atmd-server version %s because fork failed with error \"%m\".", VERSION);
    return -1;

  } else if(child_pid != 0) {
    // We are still in the parent. We save the child pid and then exit.
    pid_file << child_pid;
    pid_file.close();
    syslog(ATMD_INFO, "Fork to background successful. Child pid %d saved in %s.", child_pid, pid_filename.c_str());
    return 0;
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

  // Increase base process priority
  errno = 0;
  if(setpriority(PRIO_PROCESS, 0, -10) == -1) {
    if(errno != 0) {
      syslog(ATMD_ERR, "Failed to increase base process priority (Error: \"%m\").");
      munlockall();
      return -1;
    }
  }

  // Sleep...
  struct timespec sleeptime;
  sleeptime.tv_sec = 1;
  sleeptime.tv_nsec = 0;
  nanosleep(&sleeptime, NULL);

  // Shadow task to Xenomai domain
  RT_TASK main_task;
  int retval = rt_task_shadow(&main_task, "atmd_server", 0, T_FPU);
  if(retval) {
    switch(retval) {
      case -EBUSY:
        syslog(ATMD_CRIT, "Failed to shadow main task. Already in Xenomai context.");
        break;

      case -ENOMEM:
        syslog(ATMD_CRIT, "Failed to shadow main task. Not enough memory available.");
        break;

      case -EEXIST:
        syslog(ATMD_CRIT, "Failed to shadow main task. Name already in use.");
        break;

      case -EPERM:
        syslog(ATMD_CRIT, "Failed to shadow main task. Invalid context.");
        break;

      default:
        syslog(ATMD_CRIT, "Failed to shadow main task. Unexpected error (%d).", retval);
        break;
    }
    return -1;
  }


  // Initialize libcurl
  if(curl_global_init(CURL_GLOBAL_ALL)) {
    rt_syslog(ATMD_CRIT, "Failed to initialize libcurl. Exiting.");
    return -1;
  }


  // Create network IF and board
  Network netif;
  bool netif_init_done = false;
  VirtualBoard board(server_conf);
  bool board_init_done = false;

  // Init VirtualBoard
  board.status(ATMD_STATUS_BOOT);
  if(board.init()) {
    rt_syslog(ATMD_CRIT, "Failed to initialize VirtualBoard.");
    goto server_cleanup;
  } else {
    board_init_done = true;
  }

  // Now we initialize network interface
  netif.set_address(ip_address);
  netif.set_port(listen_port);
  try {
    netif.init();

  } catch (int e) {
    switch(e) {
      case ATMD_ERR_SOCK:
        rt_syslog(ATMD_CRIT, "Failed to initialize Network. Socket error.");
        break;
      case ATMD_ERR_LISTEN:
        rt_syslog(ATMD_CRIT, "Failed to initialize Network. Listen error.");
        break;
      default:
        rt_syslog(ATMD_CRIT, "Failed to initialize Network. Got unexpected exception (%d).", e);
        break;
    }
    goto server_cleanup;
  }
  netif_init_done = true;

  // Define the time for which the main cycle sleep between subsequent calls to get_command
  sleeptime.tv_sec = 0;
  sleeptime.tv_nsec = 25000000; // 25ms sleep

  // Cycle accepting client connections
  while(netif.accept_client() == 0) {
    // We have a connection from a client, so we start the command processing cycle

    try {

      string command;
      do {
        // When we Receive the terminate_interrupt we close the connection and exit
        if(terminate_interrupt) {
          throw(ATMD_ERR_TERM);
        }

        if(netif.get_command(command) == 0) {
          // Received a command

#ifdef DEBUG
          if(enable_debug)
            // If debug is eanbled, we log all commands received
            rt_syslog(ATMD_DEBUG, "Received command \"%s\"", command.c_str());
#endif

          // Command execution
          netif.exec_command(command, board);
        }

        // Sleep
        nanosleep(&sleeptime, NULL);

      } while(true);

    } catch(int e) {
      // We catched an exception indicating an error on client connection so we close it.
      netif.close_client();
      if(e == ATMD_ERR_TERM)
        break;

    } catch(std::exception& e) {
      rt_syslog(ATMD_CRIT, "atmd_server.cpp: caught an unexpected exception (%s).", e.what());
      break;

    } catch(...) {
      rt_syslog(ATMD_CRIT, "Network [get_command]: unknown exception.");
      break;
    }
  }

  // Final cleanup
  server_cleanup:

  // Set termination flag
  terminate_interrupt = true;

  // Explicitely close network
  if(netif_init_done)
    netif.close_client();

  // Explicitely call VirtualBoard destructor
  if(board_init_done)
    if(board.close())
      rt_syslog(ATMD_ERR, "Failed to properly release all RT resources.");

  // CURL library cleanup
  curl_global_cleanup();

  rt_syslog(ATMD_INFO, "Terminating atmd-server version %s.", VERSION);

  closelog();
  return 0;
}
