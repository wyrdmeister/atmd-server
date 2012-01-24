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

#ifndef ATMD_COMMON_H
#define ATMD_COMMON_H

/* ATMD reference time (25ns, 40MHz) */
#define ATMD_TREF 25e-9

/* ATMD autoretrigger timer */
#define ATMD_AUTORETRIG 199

/* Defines some useful syslog constants */
#define ATMD_DEBUG (LOG_DAEMON | LOG_DEBUG)
#define ATMD_INFO (LOG_DAEMON | LOG_INFO)
#define ATMD_NOTICE (LOG_DAEMON | LOG_NOTICE)
#define ATMD_WARN (LOG_DAEMON | LOG_WARNING)
#define ATMD_ERR (LOG_DAEMON | LOG_ERR)
#define ATMD_CRIT (LOG_DAEMON | LOG_CRIT)


/* Default listening parameters */
#define ATMD_DEF_PORT 2606
#define ATMD_DEF_LISTEN "0.0.0.0"

/* Default PID file */
#define ATMD_PID_FILE "/var/run/atmd_server.pid"

/* Default maximum number of events per start */
#define ATMD_MAX_EV 50000

/* Defines of ATMD error codes
   General */
#define ATMD_ERR_NONE        100 // No error
#define ATMD_ERR_TIMER       101 // Global timer error
#define ATMD_ERR_ALLOC       102 // Memory allocation error
#define ATMD_ERR_UNKNOWN_EX  103 // Catched an unhandled exception
#define ATMD_ERR_TH_SPAWN    104 // Error starting RT thread
#define ATMD_ERR_TH_JOIN     105 // Error joining RT thread

/* Measurements */
#define ATMD_ERR_TERM        110 // Got termination interrupt
#define ATMD_ERR_START01     111 // Start01 is missing
#define ATMD_ERR_EMPTY       112 // The measure has zero start events
#define ATMD_ERR_NOSTART     113 // The ACAM board stopped to get start events
#define ATMD_ERR_ALARM       114 // Error configuring the watchdog alarm

/* Network errors */
#define ATMD_ERR_SOCK        120 // Error creating socket
#define ATMD_ERR_LISTEN      121 // Error listening on the network interface
#define ATMD_ERR_RECV        122 // Error recieving data from the network
#define ATMD_ERR_SEND        123 // Error sending data to the network
#define ATMD_ERR_CLOSED      124 // Remote connection closed


/* Defines of ATMD board statuses */
#define ATMD_STATUS_IDLE      1
#define ATMD_STATUS_FINISHED  2
#define ATMD_STATUS_RUNNING   3
#define ATMD_STATUS_ERROR     4
#define ATMD_STATUS_STARTING  5

/* Thread starting timeout */
#define ATMD_THREAD_TIMEOUT   20

/* Autostart status */
#define ATMD_AUTOSTART_DISABLED 0
#define ATMD_AUTOSTART_ENABLED  1
#define ATMD_AUTOSTART_STOP     2

/* Save file formats */
#define ATMD_FORMAT_RAW         1	// Save in a text file in raw format (stop counts and retriggers)
#define ATMD_FORMAT_PS          2	// Save in a text file with stop times in ps
#define ATMD_FORMAT_US          3	// Save in a text file with stop times in us
#define ATMD_FORMAT_BINPS       4	// NOT IMPLEMENTED
#define ATMD_FORMAT_BINRAW      5	// NOT IMPLEMENTED
#define ATMD_FORMAT_DEBUG       6	// Debug format in text mode
#define ATMD_FORMAT_MATPS1      7	// Matlab v5 file with all data in a single matrix
#define ATMD_FORMAT_MATPS2      8	// Matlab v5 file with separate variables for start, channel and stop (more compact!)
#define ATMD_FORMAT_MATRAW      9	// NOT IMPLEMENTED
#define ATMD_FORMAT_MATPS2_FTP 10	// Same as ATMD_FORMAT_MATPS2 but saved via FTP
#define ATMD_FORMAT_MATPS2_ALL 11	// Same as ATMD_FORMAT_MATPS2 but saved locally and via FTP
#define ATMD_FORMAT_MATPS3     12	// Same as ATMD_FORMAT_MATPS2 but with also the timestamp for each start and the effective window duration
#define ATMD_FORMAT_MATPS3_FTP 13	// Same as ATMD_FORMAT_MATPS3 but saved via FTP
#define ATMD_FORMAT_MATPS3_ALL 14	// Same as ATMD_FORMAT_MATPS3 but saved both locally and via FTP


/* Network errors */
#define ATMD_NETERR_NONE            0
#define ATMD_NETERR_BAD_CH          1
#define ATMD_NETERR_BAD_TIMESTR     2
#define ATMD_NETERR_BAD_PARAM       3
#define ATMD_NETERR_THJOIN          4
#define ATMD_NETERR_START           5
#define ATMD_NETERR_STOP            6
#define ATMD_NETERR_ABORT           7
#define ATMD_NETERR_SAV             8
#define ATMD_NETERR_DEL             9
#define ATMD_NETERR_STATUS_IDLE    10
#define ATMD_NETERR_STATUS_ERR     11
#define ATMD_NETERR_STATUS_RUN     12
#define ATMD_NETERR_STATUS_FINISH  13
#define ATMD_NETERR_STATUS_UNKN    14
#define ATMD_NETERR_NOPREFIX       15
#define ATMD_NETERR_AUTORANGE      16
#define ATMD_NETERR_AUTOSTART      17

/* Standard includes */
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#include <string>
#include <vector>
#include <exception>

#include <stdint.h>
#include <stdarg.h>
#include <syslog.h>
#include <errno.h>

#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <pwd.h>
#include <execinfo.h>
#include <ucontext.h>

#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <pcrecpp.h>

#include <curl/curl.h>

// Xenomai headers
#include <native/task.h>
#include <native/timer.h>
#include <native/alarm.h>

using namespace std;

#endif
