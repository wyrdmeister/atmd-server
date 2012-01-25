/*
 * ATMD Server version 3.0
 *
 * ATMD Agent - Measurement function header
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

#ifndef ATMD_COMMON_H
#define ATMD_COMMON_H

// Server version
#ifndef VERSION
#define VERSION "3.0"
#endif

// ATMD reference time (25ns, 40MHz)
#define ATMD_TREF  25e-9

// ATMD autoretrigger timer
#define ATMD_AUTORETRIG  199

// ATMD board default start offset
#define ATMD_DEF_STARTOFFSET  2000

// ATMD board default reference clock exponent
#define ATMD_DEF_REFCLK  7

// ATMD board default divider
#define ATMD_DEF_HSDIV  183

// Default PID file
#define ATMD_PID_FILE "/var/run/atmd_server.pid"

// Default confiugration file
#define ATMD_CONF_FILE "/etc/atmd_server.conf"

// Syslog constants
#include <syslog.h>
#define ATMD_DEBUG (LOG_DAEMON | LOG_DEBUG)
#define ATMD_INFO (LOG_DAEMON | LOG_INFO)
#define ATMD_NOTICE (LOG_DAEMON | LOG_NOTICE)
#define ATMD_WARN (LOG_DAEMON | LOG_WARNING)
#define ATMD_ERR (LOG_DAEMON | LOG_ERR)
#define ATMD_CRIT (LOG_DAEMON | LOG_CRIT)

// Error constants
#define ATMD_ERR_NOSTART    1     // Timed out waiting for start event
#define ATMD_ERR_ALLOC      2     // Memory allocation error
#define ATMD_ERR_SEND       3     // Error in send syscall
#define ATMD_ERR_RECV       4     // Error in recv syscall
#define ATMD_ERR_CLOSED     5     // Client connection closed
#define ATMD_ERR_SOCK       6     // Error creating socket
#define ATMD_ERR_LISTEN     7     // Error listening on the network interface
#define ATMD_ERR_TERM       8     // Termination interrupt set
#define ATMD_ERR_QUEUE      9     // Error on a RT queue
#define ATMD_ERR_UNKNOWN_EX 0xFF  // Unknown error...

// RT object names
#define ATMD_RT_HEAP_NAME   "data_heap"
#define ATMD_RT_THREAD_NAME "atmd_measure"
#define ATMD_RT_CTRL_TASK   "ctrl_task"
#define ATMD_RT_DATA_TASK   "rt_data_task"
#define ATMD_NRT_DATA_TASK  "data_task"
#define ATMD_RT_CTRL_QUEUE  "ctrl_queue"
#define ATMD_RT_DATA_QUEUE  "data_queue"
#define ATMD_RT_MEAS_MUTEX  "meas_mutex"

// Board status
#define ATMD_STATUS_IDLE        0
#define ATMD_STATUS_RUNNING     1
#define ATMD_STATUS_ERR         2

// Save file formats
#define ATMD_FORMAT_RAW         1   // Save in a text file in raw format (stop counts and retriggers)
#define ATMD_FORMAT_PS          2   // Save in a text file with stop times in ps
#define ATMD_FORMAT_US          3   // Save in a text file with stop times in us
#define ATMD_FORMAT_BINPS       4   // NOT IMPLEMENTED
#define ATMD_FORMAT_BINRAW      5   // NOT IMPLEMENTED
#define ATMD_FORMAT_DEBUG       6   // Debug format in text mode
#define ATMD_FORMAT_MATPS1      7   // Matlab v5 file with all data in a single matrix
#define ATMD_FORMAT_MATPS2      8   // Matlab v5 file with separate variables for start, channel and stop (more compact!)
#define ATMD_FORMAT_MATRAW      9   // NOT IMPLEMENTED
#define ATMD_FORMAT_MATPS2_FTP 10   // Same as ATMD_FORMAT_MATPS2 but saved via FTP
#define ATMD_FORMAT_MATPS2_ALL 11   // Same as ATMD_FORMAT_MATPS2 but saved locally and via FTP
#define ATMD_FORMAT_MATPS3     12   // Same as ATMD_FORMAT_MATPS2 but with also the timestamp for each start and the effective window duration
#define ATMD_FORMAT_MATPS3_FTP 13   // Same as ATMD_FORMAT_MATPS3 but saved via FTP
#define ATMD_FORMAT_MATPS3_ALL 14   // Same as ATMD_FORMAT_MATPS3 but saved both locally and via FTP

#endif
