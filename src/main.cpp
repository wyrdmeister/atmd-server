/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * main.cpp
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

#include "common.h"
#include "atmd_hardware.h"
#include "atmd_timings.h"
#include "atmd_network.h"


// Global termination flag
bool terminate_interrupt;

// Global debug flag
bool enable_debug = false;


/* @fn signal_handler(int signal)
 * Signal handler for SIGHUP for server termination.
 *
 * @param signal Signal number. Should be always SIGHUP.
 */
void signal_handler(int signal) {
	struct sigaction handler;
	handler.sa_flags = 0;
	sigemptyset(&handler.sa_mask);
	handler.sa_handler = &(signal_handler);
	if(signal == SIGHUP) {
		sigaction(SIGHUP, &handler, NULL);
		terminate_interrupt = true;
	}
}

/* @fn bad_handler(int signal)
 * Signal handler for all signals that can be catched that termiante the process.
 *
 * @param signal Signal number.
 */
void bad_handler(int signal) {
	switch(signal) {
		case SIGINT:
			syslog(ATMD_CRIT, "Received signal SIGINT. Terminating.");
			break;
		case SIGQUIT:
			syslog(ATMD_CRIT, "Received signal SIGQUIT. Terminating.");
			break;
		case SIGILL:
			syslog(ATMD_CRIT, "Received signal SIGILL. Terminating.");
			break;
		case SIGABRT:
			syslog(ATMD_CRIT, "Received signal SIGABRT. Terminating.");
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
		case SIGTERM:
			syslog(ATMD_CRIT, "Received signal SIGTERM. Terminating.");
			break;
		case SIGUSR1:
			syslog(ATMD_CRIT, "Received signal SIGUSR1. Terminating.");
			break;
		case SIGUSR2:
			syslog(ATMD_CRIT, "Received signal SIGUSR2. Terminating.");
			break;
		default:
			syslog(ATMD_CRIT, "Received signal %d. Terminating.", signal);
			break;
	}
	exit(signal);
}


int main(int argc, char * const argv[])
{
	// The first thing to do is to initialize syslog
	openlog("atmd-server", 0, LOG_DAEMON);
	syslog(LOG_DAEMON | LOG_INFO, "Starting atmd-server version %s.", VERSION);

	// Reset interrupt flag
	terminate_interrupt = false;

	// Configuration of signal handler for SIGHUP (server termination)
	struct sigaction handler;
	handler.sa_flags = 0;
	sigemptyset(&handler.sa_mask);
	handler.sa_handler = &(signal_handler);
	// Signal for the termination of the process
	sigaction(SIGHUP, &handler, NULL);

	// We catch some bad signals to log them...
	struct sigaction bad_h;
	bad_h.sa_flags = 0;
	sigemptyset(&bad_h.sa_mask);
	bad_h.sa_handler = &(bad_handler);
	sigaction(SIGINT, &bad_h, NULL);
	sigaction(SIGQUIT, &bad_h, NULL);
	sigaction(SIGILL, &bad_h, NULL);
	sigaction(SIGABRT, &bad_h, NULL);
	sigaction(SIGFPE, &bad_h, NULL);
	sigaction(SIGSEGV, &bad_h, NULL);
	sigaction(SIGPIPE, &bad_h, NULL);
	sigaction(SIGTERM, &bad_h, NULL);
	sigaction(SIGUSR1, &bad_h, NULL);
	sigaction(SIGUSR2, &bad_h, NULL);
	sigaction(SIGALRM, &bad_h, NULL);


	// Handling of command line parameters through getopt
	/* Accepted options:
	 * -d: debug flag.
	 * -n <tcp_port>: listening port, default to 2606.
	 * -i <ip_address>: ip address to listen to, default to ANY_IP.
	 * -p <pid_file>: file in which store the pid number, default to /var/run/atmd_server.pid.
	 * -s: enable board simulation.
	 */
	bool simulate = false;
	uint16_t listen_port = ATMD_DEF_PORT;
	string ip_address = ATMD_DEF_LISTEN;

	// Pid file
	string pid_filename = ATMD_PID_FILE;
	ofstream pid_file;

	int c;
	opterr = 0;
	pcrecpp::RE re("");
	while( (c = getopt(argc, argv, "sdn:i:p:")) != -1 ) {
		switch(c) {
			case 'd':
				enable_debug = true;
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

			case 's':
				simulate = true;
				break;

			case '?':
				syslog(ATMD_WARN, "Supplied unknown command line option \"%s\".", argv[optind-1]);
				break;
		}
	}

	// Initialize libcurl
	if(curl_global_init(CURL_GLOBAL_ALL)) {
		syslog(ATMD_INFO, "Failed to initialize libcurl. Exiting.");
		exit(-1);
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

	// Initializing board object (and curl interface)
	ATMDboard board;
	if(board.init(simulate)) {
		// ATMD board not found. Terminating.
		syslog(ATMD_CRIT, "Terminating atmd-server version %s because no board was found and simulate mode is disabled.", VERSION);
		exit(-1);
	};

	// Board found (or simulation mode enabled). We can fork.
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

	struct timespec sleeptime;
	sleeptime.tv_sec = 1;
	sleeptime.tv_nsec = 0;
	nanosleep(&sleeptime, NULL);

	// Now we initialize network interface
	Network netif;
	try {
		netif.set_address(ip_address);
		netif.set_port(listen_port);
		netif.init();

	} catch (int e) {
		// TODO
	}

	// Define the time for which the main cycle sleep between subsequent calls to get_command
	sleeptime.tv_sec = 0;
	sleeptime.tv_nsec = 25000; // 25ms sleep

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
					// We have received a command

					if(enable_debug)
						// We log commands for debugging purposes
						syslog(ATMD_DEBUG, "Received command \"%s\"", command.c_str());

					// Execution of the command
					netif.exec_command(command, board);
				}

				if(board.get_autostart() != ATMD_AUTOSTART_DISABLED) {
					// If autostart is enable we shoud do some more things...
					int retval = 0;
					switch(board.get_status()) {
						case ATMD_STATUS_IDLE:
						case ATMD_STATUS_FINISHED:
							board.measure_autorestart(ATMD_ERR_NONE);
							break;

						case ATMD_STATUS_ERROR:
							board.collect_measure(retval);
							if(retval == ATMD_ERR_EMPTY) {
								// In case of empty measurement we can continue (of course we cannot the previous measurement)
								if(board.measure_autorestart(retval)) {
									// Autorestart failed... nothing to do because everything should have been done inside measure_autorestart.
								}

							} else {
								// What we do in case of other error??? Maybe we should stop the autorestart process!

							}
							break;

						case ATMD_STATUS_RUNNING:
						case ATMD_STATUS_STARTING:
						default:
							break;
					}
				}

				nanosleep(&sleeptime, NULL);
			} while(true);

			// Useless...
			if(terminate_interrupt)
				break;

		} catch(int e) {
			// We catched an exception indicating an error on client connection so we close it.
			netif.close_client();
			if(e == ATMD_ERR_TERM)
				break;

		} catch(exception& e) {
			syslog(ATMD_ERR, "main.cpp: caught an unexpected exception (%s)", e.what());
			break;

		} catch(...) {
			syslog(ATMD_ERR, "Network [get_command]: unhandled unknown exception");
			break;
		}
	}

	curl_global_cleanup();

	syslog(ATMD_INFO, "Terminating atmd-server version %s.", VERSION);
	closelog();
	return 0;
}
