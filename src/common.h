/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * common.h
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

#ifndef ATMD_COMMON_H
#define ATMD_COMMON_H

// ATMD reference time (25ns, 40MHz)
#define ATMD_TREF 25e-9

// ATMD autoretrigger timer
#define ATMD_AUTORETRIG 199

// Defines some useful syslog constants
#define ATMD_DEBUG (LOG_DAEMON | LOG_DEBUG)
#define ATMD_INFO (LOG_DAEMON | LOG_INFO)
#define ATMD_NOTICE (LOG_DAEMON | LOG_NOTICE)
#define ATMD_WARN (LOG_DAEMON | LOG_WARNING)
#define ATMD_ERR (LOG_DAEMON | LOG_ERR)
#define ATMD_CRIT (LOG_DAEMON | LOG_CRIT)


// Default listening parameters
#define ATMD_DEF_PORT 2606
#define ATMD_DEF_LISTEN "0.0.0.0"

// Default PID file
#define ATMD_PID_FILE "/var/run/atmd_server.pid"


// Defines of ATMD error codes
// General
#define ATMD_ERR_NONE         0 // No error
#define ATMD_ERR_TIMER        1 // Global timer error
#define ATMD_ERR_ALLOC        2 // Memory allocation error
#define ATMD_ERR_UNKNOWN_EX   3 // Catched an unhandled exception

// Measurements
#define ATMD_ERR_TERM         10 // Got termination interrupt
#define ATMD_ERR_START01      11 // Start01 is missing
#define ATMD_ERR_EMPTY        12 // The measure has zero start events

// Network errors
#define ATMD_ERR_SOCK         20 // Error creating socket
#define ATMD_ERR_LISTEN       21 // Error listening on the network interface
#define ATMD_ERR_RECV         22 // Error recieving data from the network
#define ATMD_ERR_SEND         23 // Error sending data to the network
#define ATMD_ERR_CLOSED       24 // Remote connection closed


// Defines of ATMD board statuses
#define ATMD_STATUS_IDLE      1
#define ATMD_STATUS_FINISHED  2
#define ATMD_STATUS_RUNNING   3
#define ATMD_STATUS_ERROR     4
#define ATMD_STATUS_STARTING  5

// Thread starting timeout
#define ATMD_THREAD_TIMEOUT   20


// Save file formats
#define ATMD_FORMAT_RAW       1
#define ATMD_FORMAT_PS        2
#define ATMD_FORMAT_US        3
#define ATMD_FORMAT_BINPS     4
#define ATMD_FORMAT_BINRAW    5
#define ATMD_FORMAT_DEBUG     6


// Network errors
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


// Configure defines
#include "../config.h"

// Standard includes
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

#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <pthread.h>
#include <sched.h>

#include <pcrecpp.h>

using namespace std;

#endif
