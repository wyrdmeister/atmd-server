/*
 * ATMD Server version 3.0
 *
 * ATMD Agent - Main header
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

#ifndef ATMD_AGENT_MAIN_H
#define ATMD_AGENT_MAIN_H

// Global
#include <stdlib.h>
#include <stdint.h>
#include <signal.h>
#include <sys/mman.h>
#include <execinfo.h>
#include <string>
#include <fstream>

#include <pcrecpp.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/ether.h>

// Xenomai
#include <rtdk.h>
#include <native/task.h>
#include <native/heap.h>

// Local
#include "common.h"
#include "atmd_rtnet.h"
#include "atmd_netagent.h"
#include "atmd_hardware.h"
#include "atmd_agentmeasure.h"
#include "atmd_config.h"
#include "atmd_rtcomm.h"

#endif
