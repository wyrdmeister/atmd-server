/*
 * ATMD Server version 3.0
 *
 * ATMD Server - Virtual board header file
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

#ifndef ATMD_VIRTUAL_BOARD
#define ATMD_VIRTUAL_BOARD

// Global
#include <string.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <vector>
#include <string>
#include <sstream>
#include <curl/curl.h>

// Xenomai
#include <native/task.h>
#include <native/queue.h>
#include <native/mutex.h>

// Local
#include "atmd_config.h"
#include "atmd_rtnet.h"
#include "atmd_netagent.h"
#include "atmd_timings.h"
#include "atmd_measure.h"
#include "MatFile.h"


/* @class AgentDescriptor
 *
 */
class AgentDescriptor {
public:
  AgentDescriptor() : _id(0) { memset(&_agent_addr, 0, sizeof(struct ether_addr)); };
  AgentDescriptor(ssize_t val) : _id(val) { memset(&_agent_addr, 0, sizeof(struct ether_addr)); };
  ~AgentDescriptor() {};

  // Manage ID
  ssize_t id()const { return _id; };
  void id(ssize_t val) { _id = val; };

  // Get pointer to the address struct
  struct ether_addr* agent_addr() { return &_agent_addr; };
  const struct ether_addr* agent_addr()const { return &_agent_addr; };

private:
  ssize_t _id;
  struct ether_addr _agent_addr;
};


/* @class VirtualBoard
 * This class manages the interface with the agents on the real-time network
 */
class VirtualBoard {
public:
  // Constructor and destructor
  VirtualBoard(AtmdConfig &obj) : _config(obj) { clear_config(); };
  ~VirtualBoard() { curl_easy_cleanup(this->easy_handle); }; // TODO: destructor should join threads...

  // Start all the relevant RT tasks and sends broadcasts to find agents
  int init();

  // Manage sockets
  RTnet& ctrl_sock() { return _ctrl_sock; };
  RTnet& data_sock() { return _data_sock; };
  const RTnet& ctrl_sock()const { return _ctrl_sock; };
  const RTnet& data_sock()const { return _data_sock; };

  // Add new AgentDescriptor
  void add_agent(ssize_t id, const struct ether_addr* addr) {
    _agents.push_back(AgentDescriptor(id));
    memcpy(_agents.back().agent_addr(), addr, sizeof(struct ether_addr));
  };

  // Get number of AgentDescriptor saved
  size_t agents()const { return _agents.size(); };

  // Retrieve an AgentDescriptor
  AgentDescriptor& get_agent(size_t i) { return _agents[i]; };
  const AgentDescriptor& get_agent(size_t i)const { return _agents[i]; };

  // Expose configuration
  const AtmdConfig& config()const { return _config; };

  // RT control thread code
  static void control_task(void *arg);

  // RT data thread code
  static void rt_data_task(void *arg);

  // Non-RT data thread code
  static void data_task(void *arg);

  // == Configuration ==

  // Clear configuration
  void clear_config();

  // Setup start channel
  void enable_start_rising(bool val) { _start_rising = val; };
  bool get_start_rising()const { return _start_rising; };
  void enable_start_falling(bool val) { _start_falling = val; };
  bool get_start_falling()const { return _start_falling; };

  // Setup channels
  size_t maxch()const { return _ch_rising.size(); };
  void enable_rising(size_t ch, bool val) { _ch_rising[ch] = val; };
  bool get_rising(size_t ch) { return _ch_rising[ch]; };
  void enable_falling(size_t ch, bool val) { _ch_falling[ch] = val; };
  bool get_falling(size_t ch) { return _ch_falling[ch]; };

  // Setup timings
  bool set_window(const std::string& val) { return _window_time.set(val); };
  bool set_tottime(const std::string& val) { return _measure_time.set(val); };
  bool set_timeout(const std::string& val) { return _timeout_time.set(val); };
  bool set_deadtime(const std::string& val) { return _deadtime.set(val); };
  const Timings& get_window()const { return _window_time; };
  const Timings& get_tottime()const { return _measure_time; };
  const Timings& get_timeout()const { return _timeout_time; };
  const Timings& get_deadtime()const { return _deadtime; };

  // Setup resolution
  void set_resolution(uint16_t clk, uint16_t hs) { _refclk = clk; _hsdiv = hs; };
  void get_resolution(uint16_t& clk, uint16_t& hs)const { clk = _refclk; hs = _hsdiv; };
  double get_tbin()const { return (ATMD_TREF * pow(2.0, _refclk) / (216 * _hsdiv) * 1e12); };

  // Setup start offset
  void set_start_offset(uint32_t val) { _offset = val; };
  uint32_t get_start_offset()const { return _offset; };

  // Setup FTP
  void set_host(std::string& host) { _hostname = host; };
  void set_user(std::string& user) { _username = user; };
  void set_password(std::string& pass) { _password = pass; };
  const std::string& get_host()const { return _hostname; };
  const std::string& get_user()const { return _username; };
  bool psw_set()const { if(_password != "") return true; else return false; };

  // Setup prefix
  void set_prefix(const std::string& val) { _prefix = val; };
  const std::string& get_prefix()const { return _prefix; };

  // Setup format
  void set_format(uint32_t val) { _format = val; };
  uint32_t get_format()const { return _format; };

  // Setup autosave
  void set_autosave(uint32_t val) { _autosave = val; };
  uint32_t get_autosave()const { return _autosave; };


  // == Measure handling ==

  // Count measures
  size_t measures()const { return _measures.size(); };

  // Measures vector mutex
  int acquire_lock() {
    return rt_mutex_acquire(&_meas_mutex, TM_INFINITE);
  };
  int release_lock() {
    return rt_mutex_release(&_meas_mutex);
  };

  // Add a measure
  void add_measure(Measure *obj) {
    _measures.push_back(obj);
  };

  // Delete a measure
  int delete_measure(size_t id) {
    if(id >= _measures.size())
      return -1;
    delete _measures[id];
    _measures.erase(_measures.begin()+id);
    return 0;
  };

  // Clear all measures
  void clear_measures() {
    _measures.clear();
  };

  // Stat a measure
  int stat_stops(uint32_t measure_number, std::vector< std::vector<uint32_t> >& stop_counts)const;
  int stat_stops(uint32_t measure_number, std::vector< std::vector<uint32_t> >& stop_counts, std::string win_start, std::string win_ampl)const;

  int stat_measure(size_t id, uint32_t& count)const {
    if(id >= _measures.size())
      return -1;
    count = _measures[id]->count_starts();
    return 0;
  };

  // Save measure
  int save_measure(size_t measure_number, std::string filename);

  // CURL callback
  static size_t curl_read(void *ptr, size_t size, size_t count, void *data);

  // Start measure
  int start_measure();

  // Stop measure
  int stop_measure();

  // Manage board status
  void status(int val) { _status = val; };
  int status()const { return _status; };

  // Manage command queue
  void recv_command(GenMsg& packet) throw(int);
  void send_command(const GenMsg& packet) throw(int);

  // Autosave counter
  void reset_counter() { _auto_counter = 0; };
  size_t get_counter()const { return _auto_counter; };
  void increment_counter() { _auto_counter++; };

private:
  // Config object reference
  AtmdConfig &_config;

  // Handle of the RT control task
  RT_TASK _ctrl_task;

  // Control RT socket
  RTnet _ctrl_sock;

  // Control queue
  RT_QUEUE _ctrl_queue;

  // Handle of the RT data task
  RT_TASK _rt_data_task;

  // Handle of the non-RT data task
  RT_TASK _data_task;

  // Vector of DataTask structures
  std::vector<AgentDescriptor> _agents;

  // Data socket
  RTnet _data_sock;

  // == Board configuration variables ==

  // Start channel configuration
  bool _start_rising;
  bool _start_falling;

  // Channel configuration
  std::vector<bool> _ch_rising;
  std::vector<bool> _ch_falling;

  // Window time
  Timings _window_time;

  // Measure time
  Timings _measure_time;

  // Timeout time
  Timings _timeout_time;

  // Deadtime
  Timings _deadtime;

  // Start offset
  uint32_t _offset;

  // Resolution
  uint32_t _refclk;
  uint32_t _hsdiv;

  // CURL easy handle
  CURL* easy_handle;
  char curl_error[CURL_ERROR_SIZE];

  // FTP data
  std::string _hostname;
  std::string _username;
  std::string _password;

  // Prefix for autosave
  std::string _prefix;

  // Autosave after numofstarts
  uint32_t _autosave;

  // Autosave counter
  size_t _auto_counter;

  // Save format
  uint32_t _format;


  // == Measures handling ==

  // Vector of measures
  std::vector<Measure*> _measures;

  // Measures vector mutex
  RT_MUTEX _meas_mutex;

  // Board status
  int _status;
};

#endif
