/*
 * ATMD Server version 3.0
 *
 * ATMD Server - Client communication class
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

// Network errors
#define ATMD_NETERR_NONE            0
#define ATMD_NETERR_BAD_CH          1
#define ATMD_NETERR_BAD_TIMESTR     2
#define ATMD_NETERR_BAD_PARAM       3
#define ATMD_NETERR_SAV             4
#define ATMD_NETERR_DEL             5
#define ATMD_NETERR_MEAS_RUN        6
#define ATMD_NETERR_LOCK            7
#define ATMD_NETERR_STAT            8
#define ATMD_NETERR_START           9
#define ATMD_NETERR_STOP           10
#define ATMD_NETERR_BOOT           11
#define ATMD_NETERR_BAD_STATUS     12

static const char *network_strerror[] = {
  "NONE",
  "BAD_CHANNEL",
  "BAD_TIMESTRING",
  "BAD_PARAMETER",
  "SAVE",
  "DELETE",
  "MEAS_RUNNING",
  "LOCK",
  "STAT",
  "START",
  "STOP",
  "BOOT",
  "BAD_STATUS"
};

#include "atmd_network.h"


/* @fn Network::Network()
 * Network object constructor
 */
Network::Network() {
  address = ATMD_DEF_LISTEN;
  port = ATMD_DEF_PORT;
  listen_socket = -1;
  client_socket = -1;
  valid_commands.clear();

  // Valid commands - ATMD protocol version 2.0
  valid_commands.push_back("SET");
  valid_commands.push_back("GET");
  valid_commands.push_back("MSR");
  valid_commands.push_back("EXT");
}


/* @fn Network::init()
 * This function initializes the server network interface. It creates the listening
 * socket, binds to it and begins to listen.
 *
 * @return Returns 0 on success, throws an exception on error.
 */
int Network::init() {

  // Creation of an AF_INET socket
  if( (this->listen_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    // Failed to create the socket
    rt_syslog(ATMD_ERR, "Network [init]: failed to create listening socket (Error: %s).", strerror(errno));
    throw(ATMD_ERR_SOCK);
  }

  // Configuration the listening socket for non blocking I/O
  if(fcntl(this->listen_socket, F_SETFL, O_NONBLOCK) == -1) {
    // Failed to set socket flags
    rt_syslog(ATMD_ERR, "Network [init]: failed to set socket flag O_NONBLOCK (Error: %s).", strerror(errno));
    close(this->listen_socket);
    this->listen_socket = -1;
    throw(ATMD_ERR_SOCK);
  }

  // Initialization of the local address structure
  struct sockaddr_in bind_address;
  memset(&bind_address, 0, sizeof(struct sockaddr_in));
  bind_address.sin_family = AF_INET;
  bind_address.sin_port = htons(this->port);
  if( !inet_aton(this->address.c_str(), &(bind_address.sin_addr)) ) {
    rt_syslog(ATMD_ERR, "Network [init]: failed to convert ip address (%s).", this->address.c_str());
    close(this->listen_socket);
    this->listen_socket = -1;
    throw(ATMD_ERR_LISTEN);
  }

  // Binding of the socket to the local address
  if(bind(this->listen_socket, (struct sockaddr *) &bind_address, sizeof(bind_address)) == -1) {
    // Binding failed
    rt_syslog(ATMD_ERR, "Network [init]: failed binding to address %s (Error: %s).", this->address.c_str(), strerror(errno));
    close(this->listen_socket);
    this->listen_socket = -1;
    throw(ATMD_ERR_LISTEN);
  }

  // Start to listen
  if(listen(this->listen_socket, 1) == -1) {
    // Listen failed
    rt_syslog(ATMD_ERR, "Network [init]: failed listening on address %s (Error: %s).", this->address.c_str(), strerror(errno));
    close(this->listen_socket);
    this->listen_socket = -1;
    throw(ATMD_ERR_LISTEN);
  }

  rt_syslog(ATMD_INFO, "Network [init]: begin listening on address %s:%d.", this->address.c_str(), this->port);
  return 0;
}


/* @fn Network::accept_client()
 * This function loops waiting for a connection. In the meantime checks for the
 * terminate interrupt.
 *
 * @return Return 0 on success, -1 on error (or terminate interrupt).
 */
int Network::accept_client() {
  bool connection_ok = false;
  struct sockaddr_in client_addr;
  socklen_t client_addr_length = sizeof(client_addr);

  // We set 25ms sleep between accept tries
  struct timespec sleeptime;
  sleeptime.tv_sec = 0;
  sleeptime.tv_nsec = 25000000;

  // We cycle until a valid connection is recieved or the terminate_interrupt is set.
  while(!connection_ok && !terminate_interrupt) {
    // We accept a connection
    this->client_socket = accept(this->listen_socket, (struct sockaddr *)&client_addr, &client_addr_length);

    if(this->client_socket == -1) {
      // If errno is EAGAIN, ECONNABORTED or EINTR we try again after 25ms
      if(errno == EAGAIN || errno == ECONNABORTED || errno == EINTR) {
        nanosleep(&sleeptime, NULL);
        continue;

      // An error occurred
      } else {
        rt_syslog(ATMD_ERR, "Network [accept]: accept failed (Error: %s).", strerror(errno));
        this->client_socket = -1;
        return -1;
      }
    } else {
      // We have a connection!
      connection_ok = true;
    }
  }

  // If we exited the cycle for a terminate_interrupt we should close the client_socket and return -1;
  if(terminate_interrupt) {
    if(this->client_socket > 0) {
      close(client_socket);
      this->client_socket = -1;
    }
    return -1;
  }

  rt_syslog(ATMD_INFO, "Network [accept]: successfully accepted a connection from %s:%d.", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
  return 0;
}


/* @fn Network::get_command(std::string& command)
 * This function search for a valid command.
 *
 * @param command A reference to a string variable for returning the recieved command.
 * @return Return 0 on success, -1 on empty recv buffer, throw an exception on error or on terminate interrupt.
 */
int Network::get_command(std::string& command) {
  try {

    int retval;
    do {
      // First of all we check if there is something on the network
      retval = this->fill_buffer();

      // If the buffer is not empty try to find a command
      if(this->recv_buffer.size() > 0)
        if(this->check_buffer(command) == 0)
          // We have the command!
          return 0;

      if(terminate_interrupt)
        throw(ATMD_ERR_TERM);

    } while(retval != -1);

    // There's no data on the network, so we return to the main cycle...
    return -1;

  } catch(int e) {
    // Handle exceptions... the only thing that we can do here is to log what happened.
    switch(e) {
      case ATMD_ERR_RECV:
        // A recv call failed... errno should be still valid...
        rt_syslog(ATMD_ERR, "Network [get_command]: recv failed (Error: %s).", strerror(errno));
        break;
      case ATMD_ERR_CLOSED:
        rt_syslog(ATMD_ERR, "Network [get_command]: client closed the connection.");
        break;
      case ATMD_ERR_TERM:
        rt_syslog(ATMD_ERR, "Network [get_command]: command recieving interrupted by HUP signal.");
        break;
      default:
        // Unexpected exception... this should never happen!
        rt_syslog(ATMD_CRIT, "Network [get_command]: unexpected exception %d.", e);
    }
    // We pass the exception to the parent
    throw(e);

  } catch(...) {
      rt_syslog(ATMD_ERR, "Network [get_command]: unhandled exception");
      throw(ATMD_ERR_UNKNOWN_EX);
  }
}


/* @fn Network::send_command(std::string command)
 * This function send a command string to the network.
 *
 * @param command The command string.
 * @return Return 0 on success, -1 on error.
 */
int Network::send_command(std::string command) {
  int retval;

  // We keep a copy of the original command for the logs
  std::string orig_command = command;

  // We add at the end of the command the \r\n termination characters
  command.append("\r\n");
  int sent = 0;
  int remaining = command.length();

  do {
    // We send the command on the network
    retval = send(this->client_socket, command.c_str()+sent, remaining, MSG_NOSIGNAL);

    // The command was sent successfully
    if(retval == remaining) {
#ifdef DEBUG
      if(enable_debug)
        rt_syslog(ATMD_DEBUG, "Network [send_command]: sent command \"%s\".", orig_command.c_str());
#endif
      return 0;

    // The command was partially sent, we try to send what is left
    } else if(retval != -1 && retval < remaining) {
      rt_syslog(ATMD_WARN, "Network [send_command]: partially sent command \"%s\". Sent out %d bytes out of %d.", orig_command.c_str(), sent, (unsigned int) command.length());
      sent = retval;
      remaining = remaining - retval;
      continue;

    // The send call failed
    } else {
      if(retval == -1) {
        if(errno == ECONNRESET || errno == EPIPE) {
          rt_syslog(ATMD_ERR, "Network [send_command]: remote connection closed.");
          throw(ATMD_ERR_CLOSED);
        } else if (errno == EAGAIN || errno == EINTR) {
          continue;
        }
        rt_syslog(ATMD_ERR, "Network [send_command]: 'send' failed with error \"%s\".", strerror(errno));
      } else {
        rt_syslog(ATMD_ERR, "Network [send_command]: 'send' failed with a return value of %d (lenght of data %d).", retval, remaining);
      }
      throw(ATMD_ERR_SEND);
    }

    if(terminate_interrupt)
      throw(ATMD_ERR_TERM);

  } while(true);
}


/* @fn Network::check_buffer(std::string& command)
 * Private utility function for checking the receive buffer for a valid command.
 *
 * @param command A reference to the output variable.
 * @return Return 0 on success, -1 if there's no command.
 */
int Network::check_buffer(std::string& command) {
  std::vector<std::string>::iterator iter;
  size_t begin, end;

  try {
    // Check for each command if there is an occurrence in the buffer
    for(iter = this->valid_commands.begin(); iter != this->valid_commands.end(); ++iter) {
      // We search the string for the command prefix
      begin = this->recv_buffer.find(*iter);

      if(begin != std::string::npos) {
        // If we have found the prefix we search for the termination character
        end = this->recv_buffer.find("\n", begin);
        if(end != std::string::npos) {
          // If we have found a complete command we extract it from the buffer
          command = this->recv_buffer.substr(begin, end-begin);
          this->recv_buffer = recv_buffer.substr(end+1);

          // If there's a carriage retrun we remove it
          if(command.find("\r") != std::string::npos)
            command.erase(command.end()-1);
          return 0;
        }
      }
    }
  } catch(std::exception& e) {
    rt_syslog(ATMD_ERR, "Network [check_buffer]: caught an exception (%s)", e.what());
  } catch(...) {
    rt_syslog(ATMD_ERR, "Network [check_buffer]: unhandled exception");
  }

  // No command found in the buffer, so we return -1
  return -1;
}


/* @fn Network::fill_buffer()
 * Private utility function for filling the recv buffer
 *
 * @return Return 0 on success, -1 if there's no data. On error throw an exception.
 */
int Network::fill_buffer() {
  char buffer[256];
  int retval;

  retval = recv(this->client_socket, &buffer, sizeof(char)*256, MSG_DONTWAIT);
  if(retval == -1 && (errno == EAGAIN || errno == EINTR)) {
    // There is no data to receive...
    return -1;

  } else if(retval > 0) {
    // We have received some data
    this->recv_buffer.append(buffer, retval);
    return 0;

  } else if(retval == 0) {
    // The connection has been closed. We throw an exception.
    throw(ATMD_ERR_CLOSED);

  } else {
    // An error occurred in the recv_buffer
    throw(ATMD_ERR_RECV);
  }
}


/* @fn Network::format_command(std::string format, ...)
 * This function takes a format string and a variable number of other inputs and
 * returns a formatted string object.
 *
 * @param format The format string.
 * @param ... A variable list of arguments corresponding to specifiers in format string.
 * @return Return a string. Should never fail.
 */
std::string Network::format_command(std::string format, ...) {
  char *buffer;
  std::string output;
  va_list ap;

  // Initialize the variable input list
  va_start(ap, format);

  // Format a string accordig to format parameter
  if(vasprintf(&buffer, format.c_str(), ap) == -1) {
    // Some memory allocation error occurs
    output = "";
  } else {
    // Convert output to a string object
    output.assign(buffer);
    free(buffer);
  }

  // Terminate the variable input list
  va_end(ap);

  return output;
}


/* @fn Network::exec_command(std::string command, ATMDboard& board)
 * This function get a command string and a reference to the board object on which the command should act.
 *
 * @param command The command string.
 * @param board A reference to the board object.
 * @return Return 0 on success or throw an exception on error.
 */
int Network::exec_command(std::string command, VirtualBoard& board) {
  std::string main_command, parameters;
  if(command.size() < 5) {
    main_command = command.substr(0, 3);
    parameters = "";
  } else {
    main_command = command.substr(0, 3);
    parameters = command.substr(4);
  }

  // Regular expression object
  pcrecpp::RE cmd_re("");

  // Data variables
  std::string txt;
  uint32_t val1, val2, val3;

#ifdef DEBUG
  if(enable_debug)
    rt_syslog(ATMD_DEBUG, "Network [exec_command]: got command '%s'.", command.c_str());
#endif

  // Configuration SET commands
  if(main_command == "SET") {

    // If board is booting we refuse to set parameters
    if(board.status() == ATMD_STATUS_BOOT) {
      rt_syslog(ATMD_WARN, "Network [exec_command]: trying to set parameters with the system still booting.");
      this->send_command(this->format_command("ERR %d:%s", ATMD_NETERR_BOOT, network_strerror[ATMD_NETERR_BOOT]));
      return 0;
    }

    // If a measure is running refuse to set parameters
    if(board.status() == ATMD_STATUS_RUNNING) {
      rt_syslog(ATMD_ERR, "Network [exec_command]: parameters cannot be changed while a measure is running.");
      this->send_command(this->format_command("ERR %d:%s", ATMD_NETERR_MEAS_RUN, network_strerror[ATMD_NETERR_MEAS_RUN]));
      return 0;
    }

    // Replace commas with dots (commas are not recognized as decimal point)
    pcrecpp::RE("\\,").GlobalReplace(".", &parameters);

    // Catching channel setup command
    cmd_re = "CH (\\d+) (\\d+) (\\d+)";
    if(cmd_re.FullMatch(parameters)) {
      //                           ch     rising falling
      cmd_re.FullMatch(parameters, &val1, &val2, &val3);
#ifdef DEBUG
      if(enable_debug)
        rt_syslog(ATMD_DEBUG, "Network [exec_command]: setting channel %d (rising: %d, falling: %d).", val1, val2, val3);
#endif

      if(val1 == 0) {
        board.enable_start_rising((bool)val2);
        board.enable_start_falling((bool)val3);
        this->send_command("ACK");

      } else if(val1 > 0 && val1 <= board.maxch()) {
        board.enable_rising(val1, (bool)val2);
        board.enable_falling(val1, (bool)val3);
        this->send_command("ACK");

      } else {
        rt_syslog(ATMD_WARN, "Network [exec_command]: trying to set non-existent channel %d.", val1);
        this->send_command(this->format_command("ERR %d:%s", ATMD_NETERR_BAD_CH, network_strerror[ATMD_NETERR_BAD_CH]));
      }
      return 0;
    }

    // Catching window time setup command
    cmd_re = "ST ([0-9\\.]+[umsMh]{1})";
    if(cmd_re.FullMatch(parameters)) {
      cmd_re.FullMatch(parameters, &txt);
#ifdef DEBUG
      if(enable_debug)
        rt_syslog(ATMD_DEBUG, "Network [exec_command]: setting window time to \"%s\".", txt.c_str());
#endif

      if(board.set_window(txt)) {
        rt_syslog(ATMD_WARN, "Network [exec_command]: the supplied time string is not valid as a measurement window (\"%s\")", txt.c_str());
        this->send_command(this->format_command("ERR %d:%s", ATMD_NETERR_BAD_TIMESTR, network_strerror[ATMD_NETERR_BAD_TIMESTR]));
      } else {
        this->send_command("ACK");
      }
      return 0;
    }

    // Catching total measure time setup command
    cmd_re = "TT ([0-9\\.]+[umsMh]{0,1})";

    if(cmd_re.FullMatch(parameters)) {
      cmd_re.FullMatch(parameters, &txt);
#ifdef DEBUG
      if(enable_debug)
        rt_syslog(ATMD_DEBUG, "Network [exec_command]: setting total measure time to \"%s\".", txt.c_str());
#endif

      if(board.set_tottime(txt)) {
        rt_syslog(ATMD_WARN, "Network [exec_command]: the supplied time string is not valid as the total measurement time (\"%s\")", txt.c_str());
        this->send_command(this->format_command("ERR %d:%s", ATMD_NETERR_BAD_TIMESTR, network_strerror[ATMD_NETERR_BAD_TIMESTR]));
      } else {
        this->send_command("ACK");
      }
      return 0;
    }

    // Catching deadtime time setup command
    cmd_re = "TD ([0-9\\.]+[umsMh]{0,1})";

    if(cmd_re.FullMatch(parameters)) {
      cmd_re.FullMatch(parameters, &txt);
#ifdef DEBUG
      if(enable_debug)
        rt_syslog(ATMD_DEBUG, "Network [exec_command]: setting total measure time to \"%s\".", txt.c_str());
#endif

      if(board.set_deadtime(txt)) {
        rt_syslog(ATMD_WARN, "Network [exec_command]: the supplied time string is not valid as deadtime (\"%s\")", txt.c_str());
        this->send_command(this->format_command("ERR %d:%s", ATMD_NETERR_BAD_TIMESTR, network_strerror[ATMD_NETERR_BAD_TIMESTR]));
      } else {
        this->send_command("ACK");
      }
      return 0;
    }

    // Catching resolution setup command
    cmd_re = "RS (\\d+) (\\d+)";
    if(cmd_re.FullMatch(parameters)) {
      //                           refclk hsdiv
      cmd_re.FullMatch(parameters, &val1, &val2);
#ifdef DEBUG
      if(enable_debug)
        rt_syslog(ATMD_DEBUG, "Network [exec_command]: setting resolution (ref: %d, hs: %d).", val1, val2);
#endif

      // We check that the parameters are meaningful. We cannot change the resolution too much!
      if(val1 < 6 || val2 < 60) {
        rt_syslog(ATMD_ERR, "Network [exec_command]: the resolution parameters passed are meaningless (ref: %d, hs: %d).", val1, val2);
        this->send_command(this->format_command("ERR %d:%s", ATMD_NETERR_BAD_PARAM, network_strerror[ATMD_NETERR_BAD_PARAM]));
        return 0;
      }
      board.set_resolution(val1, val2);
      this->send_command("ACK");
      return 0;
    }

    // Catching offset setup command
    cmd_re = "OF (\\d+)";
    if(cmd_re.FullMatch(parameters)) {
      cmd_re.FullMatch(parameters, &val1);
#ifdef DEBUG
      if(enable_debug)
        rt_syslog(ATMD_DEBUG, "Network [exec_command]: setting offset to %d.", val1);
#endif

      // Start offset is truncated at a length of 18 bits
      board.set_start_offset(val1 & 0x0003FFFF);
      this->send_command("ACK");
      return 0;
    }

    // Set host for FTP transfers
    cmd_re = "HOST ([a-zA-Z0-9\\.\\-]+)";
    if(cmd_re.FullMatch(parameters)) {
      cmd_re.FullMatch(parameters, &txt);
#ifdef DEBUG
      if(enable_debug)
          rt_syslog(ATMD_DEBUG, "Network [exec_command]: setting FTP host to \"%s\".", txt.c_str());
#endif

      board.set_host(txt);
      this->send_command("ACK");
      return 0;
    }

    // Set user for FTP transfers
    cmd_re = "USER ([a-zA-Z0-9\\.]+)";
    if(cmd_re.FullMatch(parameters)) {
      cmd_re.FullMatch(parameters, &txt);
#ifdef DEBUG
      if(enable_debug)
          rt_syslog(ATMD_DEBUG, "Network [exec_command]: setting FTP username to \"%s\".", txt.c_str());
#endif

      board.set_user(txt);
      this->send_command("ACK");
      return 0;
    }

    // Set password for FTP transfers
    cmd_re = "PSW ([a-zA-Z0-9]+)";
    if(cmd_re.FullMatch(parameters)) {
      cmd_re.FullMatch(parameters, &txt);
#ifdef DEBUG
      if(enable_debug)
        rt_syslog(ATMD_DEBUG, "Network [exec_command]: setting FTP password.");
#endif

      board.set_password(txt);
      this->send_command("ACK");
      return 0;
    }

    // Set filename prefix for measure autorestart (with relative path...)
    cmd_re = "PREFIX ([a-zA-Z0-9\\.\\_\\-\\/]+)";
    if(cmd_re.FullMatch(parameters)) {
      cmd_re.FullMatch(parameters, &txt);
#ifdef DEBUG
      if(enable_debug)
        rt_syslog(ATMD_DEBUG, "Network [exec_command]: setting autostart file prefix \"%s\".", txt.c_str());
#endif

      board.set_prefix(txt);
      board.reset_counter();
      this->send_command("ACK");
      return 0;
    }

    // Set save format
    cmd_re = "FORMAT (\\d+)";
    if(cmd_re.FullMatch(parameters)) {
      cmd_re.FullMatch(parameters, &val1);
#ifdef DEBUG
      if(enable_debug)
        rt_syslog(ATMD_DEBUG, "Network [exec_command]: setting autorestart save format to %d.", val1);
#endif

      board.set_format(val1);
      this->send_command("ACK");
      return 0;
    }

    // Set autosave
    cmd_re = "AUTOSAVE (\\d+)";
    if(cmd_re.FullMatch(parameters)) {
      cmd_re.FullMatch(parameters, &val1);
#ifdef DEBUG
      if(enable_debug)
        rt_syslog(ATMD_DEBUG, "Network [exec_command]: setting autosave to %d starts.", val1);
#endif

      board.set_autosave(val1);
      this->send_command("ACK");
      return 0;
    }

    // Set monitor
    cmd_re = "MONITOR (\\d+) (\\d+) ([a-zA-Z0-9\\.\\_\\-\\/]+)";
    if(cmd_re.FullMatch(parameters)) {
      cmd_re.FullMatch(parameters, &val1, &val2, &txt);
#ifdef DEBUG
      if(enable_debug)
        rt_syslog(ATMD_DEBUG, "Network [exec_command]: setting monitor parameters to saving %d starts, every %d starts, to %s.", val1, val2, txt.c_str());
#endif

      board.set_monitor(val1, val2, txt);
      this->send_command("ACK");
      return 0;
    }

    // Reset monitor
    if(parameters == "NOMONITOR") {
#ifdef DEBUG
      if(enable_debug)
        rt_syslog(ATMD_DEBUG, "Network [exec_command]: client requested to disable monitor.");
#endif

      val1 = 0;
      val2 = 0;
      txt = "";
      board.set_monitor(val1, val2, txt);
      this->send_command("ACK");
      return 0;
    }

    this->send_command(this->format_command("ERR %d:%s", ATMD_NETERR_BAD_PARAM, network_strerror[ATMD_NETERR_BAD_PARAM]));
    return 0;


  // Configuration GET commands
  } else if(main_command == "GET") {

    // Get agent configuration
    if(parameters == "AGENTS") {
#ifdef DEBUG
      if(enable_debug)
        rt_syslog(ATMD_DEBUG, "Network [exec_command]: client requested configuration of agents.");
#endif

      this->send_command(this->format_command("VAL AGENTS %d", board.agents()));
      for(size_t i = 0; i < board.agents(); i++) {
        this->send_command(this->format_command("VAL AGENT %d %s", i, ether_ntoa(board.get_agent(i).agent_addr())));
      }
      return 0;
    }

    // Get the total number of channels
    if(parameters == "CHS") {
#ifdef DEBUG
      if(enable_debug)
        rt_syslog(ATMD_DEBUG, "Network [exec_command]: client requested number of available channels.");
#endif

      this->send_command(this->format_command("VAL CHS %d", 8*board.agents()));
    }

    // Get channel configuration
    cmd_re = "CH (\\d+)";
    if(cmd_re.FullMatch(parameters)) {
      cmd_re.FullMatch(parameters, &val1);
#ifdef DEBUG
      if(enable_debug)
        rt_syslog(ATMD_DEBUG, "Network [exec_command]: client requested configuration of channel %d.", val1);
#endif

      std::string answer;
      // If channel is zero we should send the configuration of the start channel
      if(val1 == 0) {
        answer = this->format_command("VAL CH %d %d %d", val1, board.get_start_rising(), board.get_start_falling());

      // Here we send information on stop channels
      } else if(val1 > 0 && val1 <= board.maxch()) {
        answer = this->format_command("VAL CH %d %d %d", val1, board.get_rising(val1), board.get_falling(val1));

      // Here we send back an error because the requested channel is non-existent
      } else {
        answer = this->format_command("ERR %d:%s", ATMD_NETERR_BAD_CH, network_strerror[ATMD_NETERR_BAD_CH]);
      }
      this->send_command(answer);
      return 0;
    }

    // Get window time_bin
    if(parameters == "ST") {
#ifdef DEBUG
      if(enable_debug)
        rt_syslog(ATMD_DEBUG, "Network [exec_command]: client requested configured window time.");
#endif

      std::string answer = "VAL ST ";
      answer += board.get_window().get();
      this->send_command(answer);
      return 0;
    }

    // Get measure total time
    if(parameters == "TT") {
#ifdef DEBUG
      if(enable_debug)
        rt_syslog(ATMD_DEBUG, "Network [exec_command]: client requested configured total measure time.");
#endif

      std::string answer = "VAL TT ";
      answer += board.get_tottime().get();
      this->send_command(answer);
      return 0;
    }

    // Get deadtime
    if(parameters == "TD") {
#ifdef DEBUG
      if(enable_debug)
        rt_syslog(ATMD_DEBUG, "Network [exec_command]: client requested configured window deadtime.");
#endif

      std::string answer = "VAL TD ";
      answer += board.get_deadtime().get();
      this->send_command(answer);
      return 0;
    }

    // Get resolution
    if(parameters == "RS") {
#ifdef DEBUG
      if(enable_debug)
        rt_syslog(ATMD_DEBUG, "Network [exec_command]: client requested configured resolution.");
#endif

      uint16_t refclk, hs;
      board.get_resolution(refclk, hs);
      std::string answer = this->format_command("VAL RS %d %d", refclk, hs);
      this->send_command(answer);
      return 0;
    }

    // Get offset
    if(parameters == "OF") {
#ifdef DEBUG
      if(enable_debug)
        rt_syslog(ATMD_DEBUG, "Network [exec_command]: client requested configured offset.");
#endif

      std::string answer = this->format_command("VAL OF %d", board.get_start_offset());
      this->send_command(answer);
      return 0;
    }

    // Get host for FTP transfers
    if(parameters == "HOST") {
#ifdef DEBUG
      if(enable_debug)
        rt_syslog(ATMD_DEBUG, "Network [exec_command]: client requested configured FTP host.");
#endif

      txt = board.get_host();
      this->send_command(this->format_command("VAL HOST %s", (txt == "") ? "NONE" : txt.c_str()));
      return 0;
    }

    // Get user for FTP transfers
    if(parameters == "USER") {
#ifdef DEBUG
        if(enable_debug)
            rt_syslog(ATMD_DEBUG, "Network [exec_command]: client requested configured FTP host.");
#endif

        txt = board.get_user();
        this->send_command(this->format_command("VAL USER %s", (txt == "") ? "NONE" : txt.c_str()));
        return 0;
    }

    // Get password for FTP transfers (we do not send actual password, but just inform if it is set or not)
    if(parameters == "PSW") {
#ifdef DEBUG
      if(enable_debug)
        rt_syslog(ATMD_DEBUG, "Network [exec_command]: client requested configured FTP host.");
#endif

      this->send_command(this->format_command("VAL PSW %s", (board.psw_set()) ? "OK" : "NONE"));
      return 0;
    }

    // Get filename prefix for measure autorestart (with relative path...)
    if(parameters == "PREFIX") {
#ifdef DEBUG
      if(enable_debug)
        rt_syslog(ATMD_DEBUG, "Network [exec_command]: client requested configured FTP host.");
#endif

      txt = board.get_prefix();
      this->send_command(this->format_command("VAL PREFIX %s", (txt == "") ? "NONE" : txt.c_str()));
      return 0;
    }

    // Get save format
    if(parameters == "FORMAT") {
#ifdef DEBUG
      if(enable_debug)
        rt_syslog(ATMD_DEBUG, "Network [exec_command]: client requested configured save format.");
#endif

      this->send_command(this->format_command("VAL FORMAT %d", board.get_format()));
      return 0;
    }

    // Get number of starts for autosave
    if(parameters == "AUTOSAVE") {
#ifdef DEBUG
      if(enable_debug)
        rt_syslog(ATMD_DEBUG, "Network [exec_command]: client requested configured number of starts for autosave.");
#endif

      this->send_command(this->format_command("VAL AUTOSAVE %d", board.get_autosave()));
      return 0;
    }

    // Get monitor  save format
    if(parameters == "MONITOR") {
#ifdef DEBUG
      if(enable_debug)
        rt_syslog(ATMD_DEBUG, "Network [exec_command]: client requested configured monitor parameters.");
#endif

      // Get monitor parameters
      board.get_monitor(val1, val2, txt);

      this->send_command(this->format_command("VAL MONITOR %d %d %s", val1, val2, txt.c_str()));
      return 0;
    }


  // Measurement MSR commands
  } else if(main_command == "MSR") {

    // Start measure command
    if(parameters == "START") {
#ifdef DEBUG
      if(enable_debug)
          rt_syslog(ATMD_DEBUG, "Network [exec_command]: client requested to start a measure.");
#endif

      // Check board status
      if(board.status() != ATMD_STATUS_IDLE) {
        // Board status does not permit to start a measure
        this->send_command(this->format_command("ERR %d:%s", ATMD_NETERR_BAD_STATUS, network_strerror[ATMD_NETERR_BAD_STATUS]));
        return 0;
      }

      // Check that we have defined a measure time
      if(board.get_tottime().get_nsec() == 0) {
        this->send_command(this->format_command("ERR %d:%s", ATMD_NETERR_BAD_TIMESTR, network_strerror[ATMD_NETERR_BAD_TIMESTR]));
        return 0;
      }

      // Check that we have defined a window time
      if(board.get_window().get_nsec() == 0) {
        // You must specify a window time!
        this->send_command(this->format_command("ERR %d:%s", ATMD_NETERR_BAD_TIMESTR, network_strerror[ATMD_NETERR_BAD_TIMESTR]));
        return 0;
      }

      // Check that window time is not more that measure time
      if(board.get_window().get_nsec() > board.get_tottime().get_nsec()) {
        // Window time cannot be greater that measure time
        this->send_command(this->format_command("ERR %d:%s", ATMD_NETERR_BAD_TIMESTR, network_strerror[ATMD_NETERR_BAD_TIMESTR]));
        return 0;
      }

      // Start measure
      if(board.start_measure()) {
        this->send_command(this->format_command("ERR %d:%s", ATMD_NETERR_START, network_strerror[ATMD_NETERR_START]));
      } else {
        this->send_command("ACK");
      }

      return 0;
    }

    // Stop measure command
    if(parameters == "STOP") {
#ifdef DEBUG
      if(enable_debug)
        rt_syslog(ATMD_DEBUG, "Network [exec_command]: client requested to stop current measure.");
#endif

      // Check board status
      if(board.status() != ATMD_STATUS_RUNNING) {
        // No measure to stop
        this->send_command(this->format_command("ERR %d:%s", ATMD_NETERR_BAD_STATUS, network_strerror[ATMD_NETERR_BAD_STATUS]));
        return 0;
      }

      // Start measure
      if(board.stop_measure()) {
        this->send_command(this->format_command("ERR %d:%s", ATMD_NETERR_STOP, network_strerror[ATMD_NETERR_STOP]));
      } else {
        this->send_command("ACK");
      }

      return 0;
    }

    // Get measure status
    if(parameters == "STATUS") {
#ifdef DEBUG
      if(enable_debug)
        rt_syslog(ATMD_DEBUG, "Network [exec_command]: client requested board status.");
#endif

      std::string command = "MSR STATUS ";
      switch(board.status()) {
        case ATMD_STATUS_IDLE:
          command += "IDLE";
          break;

        case ATMD_STATUS_RUNNING:
          command += "RUNNING";
          break;

        case ATMD_STATUS_ERR:
          command += "ERR";
          // Reset status once notified error
          board.status(ATMD_STATUS_IDLE);
          break;

        case ATMD_STATUS_BOOT:
          command += "BOOT";
          break;

        default:
          command += "UNKNOWN";
          break;
      }

      // Send answer
      this->send_command(command);
      return 0;
    }

    // List measures command
    if(parameters == "LST" || parameters == "LIST") {
#ifdef DEBUG
      if(enable_debug)
        rt_syslog(ATMD_DEBUG, "Network [exec_command]: client requested the list of unsaved measures. List contained %lu measures.", board.measures());
#endif

      // Acquire measure lock
      if(board.acquire_lock()) {
        rt_syslog(ATMD_ERR, "Network [exec_command]: error acquiring lock of measure struct.");
        this->send_command(this->format_command("ERR %d:%s", ATMD_NETERR_LOCK, network_strerror[ATMD_NETERR_LOCK]));
        return 0;
      }

      if(board.measures() > 0) {
        this->send_command(this->format_command("MSR LST NUM %u", board.measures()));
        for(size_t i = 0; i < board.measures(); i++) {
          uint32_t starts;
          if(board.stat_measure(i, starts)) {
            this->send_command(this->format_command("MSR LST %u ERR", i));
          } else {
            this->send_command(this->format_command("MSR LST %u %u", i, starts));
          }
        }
      } else {
        this->send_command("MSR LST NUM 0");
      }

      // Release lock
      if(board.release_lock()) {
        rt_syslog(ATMD_ERR, "Network [exec_command]: error releasing lock of measure struct.");
        this->send_command(this->format_command("ERR %d:%s", ATMD_NETERR_LOCK, network_strerror[ATMD_NETERR_LOCK]));
        return 0;
      }

      return 0;
    }

    // Save measure command
    cmd_re = "SAV[E]? (\\d+) ([a-zA-Z0-9\\.\\_\\-\\/]+)";
    if(cmd_re.FullMatch(parameters)) {
      cmd_re.FullMatch(parameters, &val1, &txt);
#ifdef DEBUG
      if(enable_debug)
        rt_syslog(ATMD_DEBUG, "Network [exec_command]: client requested to save measure %u to file \"%s\"", val1, txt.c_str());
#endif

      // Acquire measure lock
      if(board.acquire_lock()) {
        rt_syslog(ATMD_ERR, "Network [exec_command]: error acquiring lock of measure struct.");
        this->send_command(this->format_command("ERR %d:%s", ATMD_NETERR_LOCK, network_strerror[ATMD_NETERR_LOCK]));
        return 0;
      }

      if(board.save_measure(val1, txt)) {
        this->send_command(this->format_command("ERR %d:%s", ATMD_NETERR_SAV, network_strerror[ATMD_NETERR_SAV]));
      } else {
        this->send_command("ACK");
      }

      // Release lock
      if(board.release_lock()) {
        rt_syslog(ATMD_ERR, "Network [exec_command]: error releasing lock of measure struct.");
        this->send_command(this->format_command("ERR %d:%s", ATMD_NETERR_LOCK, network_strerror[ATMD_NETERR_LOCK]));
        return 0;
      }

      return 0;
    }

    // Send to client measure statistics
    std::vector< std::vector<uint32_t> > stopcount;
    std::string modifier = "", win_start = "", win_ampl = "";
    cmd_re = "STAT (\\-?)(\\d+) ([0-9\\.\\,]+[umsMh]{1}) ([0-9\\.\\,]+[umsMh]{1})";
    if(cmd_re.FullMatch(parameters)) {
      cmd_re.FullMatch(parameters, &modifier, &val1, &win_start, &win_ampl);
#ifdef DEBUG
      if(enable_debug)
        rt_syslog(ATMD_DEBUG, "Network [exec_command]: client requested stats of measure %u with window (s = %s, a = %s).", val1, win_start.c_str(), win_ampl.c_str());
#endif

      // Acquire measure lock
      if(board.acquire_lock()) {
        rt_syslog(ATMD_ERR, "Network [exec_command]: error acquiring lock of measure struct.");
        this->send_command(this->format_command("ERR %d:%s", ATMD_NETERR_LOCK, network_strerror[ATMD_NETERR_LOCK]));
        return 0;
      }

      // Get stats of stops
      if(board.stat_stops(val1, stopcount, win_start, win_ampl)) {
        this->send_command(this->format_command("ERR %d:%s", ATMD_NETERR_STAT, network_strerror[ATMD_NETERR_STAT]));

      } else {
        if(modifier == "-") {
          // Asked for cumulative stats, sum up all starts!
          std::vector<uint32_t> stopsum(8*board.agents(),0);
          uint64_t mean_window = 0;
          for(std::vector< std::vector<uint32_t> >::iterator iter = stopcount.begin(); iter!=stopcount.end(); ++iter) {
            for(size_t index = 0; index < 8*board.agents(); index++)
              stopsum[index] += iter->at(index+1);
            mean_window += iter->at(0);
          }
          std::stringstream command(std::stringstream::out);
          command << "MSR STAT " << stopcount.size() << " " << mean_window / stopcount.size();
          for(size_t index = 0; index < 8*board.agents(); index++)
            command << " " << stopsum.at(index);
          this->send_command(command.str());

        } else {
          // Asked for separate stats for each start
          this->send_command(this->format_command("MSR STAT NUM %u", stopcount.size()));
          for(std::vector< std::vector<uint32_t> >::iterator iter = stopcount.begin(); iter!=stopcount.end(); ++iter) {
            std::stringstream command(std::stringstream::out);
            command << "MSR STAT " << iter - stopcount.begin() + 1 << " " << iter->at(0);
            for(size_t index = 0; index < 8*board.agents(); index++)
              command << " " << iter->at(index+1);
            this->send_command(command.str());
          }
        }
      }

      // Release lock
      if(board.release_lock()) {
        rt_syslog(ATMD_ERR, "Network [exec_command]: error releasing lock of measure struct.");
        this->send_command(this->format_command("ERR %d:%s", ATMD_NETERR_LOCK, network_strerror[ATMD_NETERR_LOCK]));
        return 0;
      }

      return 0;
    }

    cmd_re = "STAT (\\-?)(\\d+)";
    if(cmd_re.FullMatch(parameters)) {
      cmd_re.FullMatch(parameters, &modifier, &val1);
#ifdef DEBUG
      if(enable_debug)
        rt_syslog(ATMD_DEBUG, "Network [exec_command]: client requested stats of measure %u.", val1);
#endif

      // Acquire measure lock
      if(board.acquire_lock()) {
        rt_syslog(ATMD_ERR, "Network [exec_command]: error acquiring lock of measure struct.");
        this->send_command(this->format_command("ERR %d:%s", ATMD_NETERR_LOCK, network_strerror[ATMD_NETERR_LOCK]));
        return 0;
      }

      // Get stats of stops
      if(board.stat_stops(val1, stopcount)) {
        this->send_command(this->format_command("ERR %d:%s", ATMD_NETERR_STAT, network_strerror[ATMD_NETERR_STAT]));

      } else {
        if(modifier == "-") {
          std::vector<uint32_t> stopsum(8*board.agents(),0);
          uint64_t mean_window = 0;
          for(std::vector< std::vector<uint32_t> >::iterator iter = stopcount.begin(); iter!=stopcount.end(); ++iter) {
            for(size_t index = 0; index < 8*board.agents(); index++)
              stopsum[index] += iter->at(index+1);
            mean_window += iter->at(0);
          }
          std::stringstream command(std::stringstream::out);
          command << "MSR STAT " << stopcount.size() << " " << mean_window / stopcount.size();
          for(size_t index = 0; index < 8*board.agents(); index++)
            command << " " << stopsum.at(index);
          this->send_command(command.str());

        } else {
          this->send_command(this->format_command("MSR STAT NUM %u", stopcount.size()));

          for(std::vector< std::vector<uint32_t> >::iterator iter = stopcount.begin(); iter!=stopcount.end(); ++iter) {
            std::stringstream command(std::stringstream::out);
            command << "MSR STAT " << iter - stopcount.begin() + 1 << " " << iter->at(0);
            for(size_t index = 0; index < 8*board.agents(); index++)
              command << " " << iter->at(index+1);
            this->send_command(command.str());
          }
        }
      }

      // Release lock
      if(board.release_lock()) {
        rt_syslog(ATMD_ERR, "Network [exec_command]: error releasing lock of measure struct.");
        this->send_command(this->format_command("ERR %d:%s", ATMD_NETERR_LOCK, network_strerror[ATMD_NETERR_LOCK]));
        return 0;
      }

      return 0;
    }

    // Delete measure command
    cmd_re = "DEL (\\d+)";
    if(cmd_re.FullMatch(parameters)) {
      cmd_re.FullMatch(parameters, &val1);
#ifdef DEBUG
      if(enable_debug)
        rt_syslog(ATMD_DEBUG, "Network [exec_command]: client requested to delete measure %u.", val1);
#endif

      // Acquire measure lock
      if(board.acquire_lock()) {
        rt_syslog(ATMD_ERR, "Network [exec_command]: error acquiring lock of measure struct.");
        this->send_command(this->format_command("ERR %d:%s", ATMD_NETERR_LOCK, network_strerror[ATMD_NETERR_LOCK]));
        return 0;
      }

      if(board.delete_measure(val1)) {
        this->send_command(this->format_command("ERR %d:%s", ATMD_NETERR_DEL, network_strerror[ATMD_NETERR_DEL]));
      } else {
        this->send_command("ACK");
      }

      // Release lock
      if(board.release_lock()) {
        rt_syslog(ATMD_ERR, "Network [exec_command]: error releasing lock of measure struct.");
        this->send_command(this->format_command("ERR %d:%s", ATMD_NETERR_LOCK, network_strerror[ATMD_NETERR_LOCK]));
        return 0;
      }

      return 0;
    }

    // Clear measures command
    if(parameters == "CLR") {
#ifdef DEBUG
      if(enable_debug)
        rt_syslog(ATMD_DEBUG, "Network [exec_command]: client requested to clear all measures.");
#endif

      // Acquire measure lock
      if(board.acquire_lock()) {
        rt_syslog(ATMD_ERR, "Network [exec_command]: error acquiring lock of measure struct.");
        this->send_command(this->format_command("ERR %d:%s", ATMD_NETERR_LOCK, network_strerror[ATMD_NETERR_LOCK]));
        return 0;
      }

      board.clear_measures();
      this->send_command("ACK");

      // Release lock
      if(board.release_lock()) {
        rt_syslog(ATMD_ERR, "Network [exec_command]: error releasing lock of measure struct.");
        this->send_command(this->format_command("ERR %d:%s", ATMD_NETERR_LOCK, network_strerror[ATMD_NETERR_LOCK]));
        return 0;
      }

      return 0;
    }

    this->send_command(this->format_command("ERR %d:%s", ATMD_NETERR_BAD_PARAM, network_strerror[ATMD_NETERR_BAD_PARAM]));
    return 0;


  } else if(main_command == "EXT") {
    // Terminate client session
    // As first thing, we need to check the board status and eventually stop a running measure.
    switch(board.status()) {
      case ATMD_STATUS_RUNNING:
        board.stop_measure();
        break;

      case ATMD_STATUS_IDLE:
      case ATMD_STATUS_ERR:
      case ATMD_STATUS_BOOT:
        break;

      default:
        break;
    }

    // Then we clear all measures...
    board.clear_measures();

    // ... and we reset board configuration
    board.clear_config();

    // We close the connection throwing an exception
    throw(ATMD_ERR_CLOSED);

  } else {
    // This case should never be reached. We log a critical message.
    rt_syslog(ATMD_CRIT, "Network [exec_command]: got an unknown command and this should never happen! Command was \"%s\".", command.c_str());
    return 0;
  }

  return 0;
}
