/*
 * ATMD Server version 3.0
 *
 * ATMD Server - Configuration options
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

#ifdef DEBUG
extern bool enable_debug;
#endif

#include "atmd_config.h"


/* @fn AtmdConfig::read(std::string filename)
 * Read a configuration file and stores it's content into the class.
 */
int AtmdConfig::read(const std::string& filename) {
  FILE *conf_file;
  char * line = NULL;
  size_t len = 0;
  ssize_t rlen = 0;

  // Open configfile
  conf_file = fopen(filename.c_str(), "r");
  if(conf_file == NULL) {
    return -1;
  }

  //Clear config
  clear();

  // RE object
  pcrecpp::RE conf_re("");

  // Service var
  std::string txt;
  bool good_sec = false;

  while((rlen = getline(&line, &len, conf_file)) != -1) {
    // Got a line... let's parse it!

    // Discard comments
    conf_re = "^#.*";
    if(conf_re.PartialMatch(line))
      continue;

    // Discard empty lines
    conf_re = "^\\s+$";
    if(conf_re.PartialMatch(line))
      continue;

    // Check for section
    conf_re = "^\\[(\\S+)\\]";
    if(conf_re.PartialMatch(line, &txt)) {
#ifdef ATMD_SERVER
      if(txt == "server") {
#else
      if(txt == "agent") {
#endif
        good_sec = true;

      } else {
        // Ignore
        good_sec = false;
#ifdef DEBUG
        if(enable_debug)
          syslog(ATMD_DEBUG, "Config [read]: ignoring section '%s'.", txt.c_str());
#endif
      }
      continue;
    }

    if(good_sec) {

// NOTE: the agent command is defined only for the server part
#ifdef ATMD_SERVER
      // Agent command
      conf_re = "^agent ([a-fA-f0-9]{2}:[a-fA-f0-9]{2}:[a-fA-f0-9]{2}:[a-fA-f0-9]{2}:[a-fA-f0-9]{2}:[a-fA-f0-9]{2})";
      if(conf_re.PartialMatch(line, &txt)) {
#ifdef DEBUG
        if(enable_debug)
          syslog(ATMD_DEBUG, "Config [read]: found agent with address '%s'.", txt.c_str());
#endif
        struct ether_addr addr;
        memset(&addr, 0, sizeof(struct ether_addr));
        if(ether_aton_r(txt.c_str(), &addr) == NULL)
          syslog(ATMD_WARN, "Config [read]: ignoring agent with invalid mac address '%s'.", txt.c_str());
        else
          agent_addr.push_back(addr);
        continue;
      }
#endif

      // Number of RTSKBS
      conf_re = "^rtskbs (\\d+)";
      if(conf_re.PartialMatch(line, &_rtskbs)) {
#ifdef DEBUG
        if(enable_debug)
          syslog(ATMD_DEBUG, "Config [read]: configured number of RTSKBS as %u.", _rtskbs);
#endif
        continue;
      }

      // RT ethernet IF
      conf_re = "^rtif ([a-z0-9]*)";
      if(conf_re.PartialMatch(line, &txt)) {
#ifdef DEBUG
        if(enable_debug)
          syslog(ATMD_DEBUG, "Config [read]: configured RT interface as '%s'.", txt.c_str());
#endif
        strncpy(_rtif, txt.c_str(), IFNAMSIZ);
        _rtif[IFNAMSIZ-1] = '\0';
        continue;
      }

      // TDMA device
      conf_re = "^tdma ([a-zA-Z0-9]*)";
      if(conf_re.PartialMatch(line, &txt)) {
        #ifdef DEBUG
        if(enable_debug)
          syslog(ATMD_DEBUG, "Config [read]: configured TDMA device as '%s'.", txt.c_str());
        #endif
          strncpy(_tdma_dev, txt.c_str(), IFNAMSIZ);
          _tdma_dev[IFNAMSIZ-1] = '\0';
          continue;
      }

#ifdef EN_TANGO
      conf_re = "^tango (\\d+)";
      if(conf_re.PartialMatch(line, &_tango_ch)) {
#ifdef DEBUG
        if(enable_debug)
          syslog(ATMD_DEBUG, "Config [read]: configured TANGO trigger channel as %u.", _tango_ch);
#endif
        continue;
      }
#endif

    } else {
      continue;
    }

    syslog(ATMD_WARN, "Config [read]: found an unknown configuration command (The command was '%s')", line);
  }

  fclose(conf_file);
  return 0;
}
