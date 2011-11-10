/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * atmd_network.cpp
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

#include "atmd_network.h"

// Global termination interrupt
extern bool terminate_interrupt;

// Global debug flag
extern bool enable_debug;


Network::Network() {
	address = ATMD_DEF_LISTEN;
	port = ATMD_DEF_PORT;
	listen_socket = NULL;
	client_socket = NULL;
	valid_commands.clear();
	
	// Valid commands - ATMD protocol version 2.0
	valid_commands.push_back("SET");
	valid_commands.push_back("GET");
	valid_commands.push_back("MSR");
	valid_commands.push_back("EXT");
}


/* @fn Network::init()
 * This function initialize the server network interface. It create the listening
 * socket, bind to it and begin to listen.
 *
 * @return Return 0 on success, throw an exception on error.
 */
int Network::init() {

	// Create AF_INET socket
	if( (this->listen_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		// Failed to create the socket
		syslog(ATMD_ERR, "Network [init]: failed to create listening socket (Error: %m).");
		throw(ATMD_ERR_SOCK);
	}

	// We configure the listening socket for non blocking I/O
	if(fcntl(this->listen_socket, F_SETFL, O_NONBLOCK) == -1) {
		// Failed to set socket flags
		syslog(ATMD_ERR, "Network [init]: failed to set socket flag O_NONBLOCK (Error: %m).");
		close(this->listen_socket);
		this->listen_socket = NULL;
		throw(ATMD_ERR_SOCK);
	}

	// Initialize local address structure
	struct sockaddr_in bind_address;
	memset(&bind_address, 0, sizeof(struct sockaddr_in));
	bind_address.sin_family = AF_INET;
	bind_address.sin_port = htons(this->port);
	if( !inet_aton(this->address.c_str(), &(bind_address.sin_addr)) ) {
		syslog(ATMD_ERR, "Network [init]: failed to convert ip address (%s).", this->address.c_str());
		close(this->listen_socket);
		this->listen_socket = NULL;
		throw(ATMD_ERR_LISTEN);
	}

	// Binding socket to local address
	if(bind(this->listen_socket, (struct sockaddr *) &bind_address, sizeof(bind_address)) == -1) {
		// Binding failed
		syslog(ATMD_ERR, "Network [init]: failed binding to address %s (Error: %m).", this->address.c_str());
		close(this->listen_socket);
		this->listen_socket = NULL;
		throw(ATMD_ERR_LISTEN);
	}

	// Setting socket in listening state
	if(listen(this->listen_socket, 1) == -1) {
		// Listen failed
		syslog(ATMD_ERR, "Network [init]: failed listening on address %s (Error: %m).", this->address.c_str());
		close(this->listen_socket);
		this->listen_socket = NULL;
		throw(ATMD_ERR_LISTEN);
	}

	syslog(ATMD_INFO, "Network [init]: begin listening on address %s:%d.", this->address.c_str(), this->port);
	return 0;
}


/* @fn Network::accept_client()
 * This function loops waiting for a connection. In the meantime check for the
 * terminate interrupt.
 *
 * @return Return 0 on success, -1 on error (or terminate interrupt).
 */
int Network::accept_client() {
	bool connection_ok = false;
	struct sockaddr_in client_addr;
	socklen_t client_addr_length = sizeof(client_addr);

	// Set 25ms sleep between accept tries
	struct timespec sleeptime;
	sleeptime.tv_sec = 0;
	sleeptime.tv_nsec = 25000000;

	// Cycle until a valid connection is recieved or the terminate_interrupt is set.
	while(!connection_ok && !terminate_interrupt) {
		// Accept a connection
		this->client_socket = accept(this->listen_socket, (struct sockaddr *)&client_addr, &client_addr_length);

		if(this->client_socket == -1) {
			// If errno is EAGAIN, ECONNABORTED or EINTR we try again after 25ms
			if(errno == EAGAIN || errno == ECONNABORTED || errno == EINTR) {
				nanosleep(&sleeptime, NULL);
				continue;

			// An error occurred
			} else {
				syslog(ATMD_ERR, "Network [accept]: accept failed. (Error: %m).");
				this->client_socket = 0;
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
			this->client_socket = 0;
		}
		return -1;
	}

	syslog(ATMD_INFO, "Network [accept]: successfully accepted a connection from %s:%d.", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
	return 0;
}


/* @fn Network::get_command(string& command)
 * This function loops waiting for a valid command.
 *
 * @param command A reference to a string variable for returning the recieved command.
 * @return Return 0 on success, throw an exception on error or on terminate interrupt.
 */
int Network::get_command(string& command) {
	
	// Set a sleeptime of 1ms between attempts to fill the buffer
	struct timespec sleeptime;
	sleeptime.tv_sec = 0;
	sleeptime.tv_nsec = 1000000;

	try {
		
		do {
			// If the buffer is not empty try to find a command
			if(this->recv_buffer.size() > 0)
				if(this->check_buffer(command) == 0)
					// We have the command!
					return 0;
			
			// Otherwise we try to fill the buffer
			if(this->fill_buffer())
				// There's no data on the network. We sleep for a while...
				nanosleep(&sleeptime, NULL);

			if(terminate_interrupt)
				throw(ATMD_ERR_TERM);

		} while(true);

	} catch(int e) {
		// Handle exceptions... the only thing that we can do here is to log what happened.
		switch(e) {
			case ATMD_ERR_RECV:
				// A recv call failed... errno should be still valid...
				syslog(ATMD_ERR, "Network [get_command]: recv failed (Error: %m).");
				break;
			case ATMD_ERR_CLOSED:
				syslog(ATMD_ERR, "Network [get_command]: client closed the connection.");
				break;
			case ATMD_ERR_TERM:
				syslog(ATMD_ERR, "Network [get_command]: command recieving interrupted by HUP signal.");
				break;
			default:
				// Unexpected exception... this should never happen!
				syslog(ATMD_CRIT, "Network [get_command]: unexpected exception %d.", e);
		}
		// We pass the exception to the parent
		throw(e);

	} catch(...) {
		syslog(ATMD_ERR, "Network [get_command]: unhandled exception");
		throw(ATMD_ERR_UNKNOWN_EX);
	}
}


/* @fn Network::send_command(string command)
 * This function send a command string to the network.
 *
 * @param command The command string.
 * @return Return 0 on success, -1 on error.
 */
int Network::send_command(string command) {
	int retval;

	// We keep a copy of the original command for the logs
	string orig_command = command;
	
	// We add at the end of the command the \r\n termination characters
	command.append("\r\n");
	int sent = 0;
	int remaining = command.length();

	do {
		// We send the command on the network
		retval = send(this->client_socket, command.c_str()+sent, remaining, MSG_NOSIGNAL);

		// The command was sent successfully
		if(retval == remaining) {
			if(enable_debug)
				syslog(ATMD_DEBUG, "Network [send_command]: sent command \"%s\".", orig_command.c_str());
			return 0;

		// The command was partially sent, we try to send what is left
		} else if(retval != -1 && retval < remaining) {
			syslog(ATMD_WARN, "Network [send_command]: partially sent command \"%s\". Sent out %d bytes out of %d.", orig_command.c_str(), sent, (unsigned int) command.length());
			sent = retval;
			remaining = remaining - retval;
			continue;

		// The send call failed
		} else {
			if(retval == -1) {
				if(errno == ECONNRESET || errno == EPIPE) {
					syslog(ATMD_ERR, "Network [send_command]: remote connection closed.");
					throw(ATMD_ERR_CLOSED);
				} else if (errno == EAGAIN || errno == EINTR) {
					continue;
				}
				syslog(ATMD_ERR, "Network [send_command]: send failed with error \"%m\".");
			} else {
				syslog(ATMD_ERR, "Network [send_command]: send failed with a return value of %d (lenght of sent data %d).", retval, remaining);
			}
			throw(ATMD_ERR_SEND);
		}

		if(terminate_interrupt)
			throw(ATMD_ERR_TERM);

	} while(true);
}


/* @fn Network::check_buffer(string& command)
 * Private utility function for checking the receive buffer for a valid command.
 *
 * @param command A reference to the output variable.
 * @return Return 0 on success, -1 if there's no command.
 */
int Network::check_buffer(string& command) {
	std::vector<string>::iterator iter;
	size_t begin, end;

	try {
		// Check for each command if there is an occurrence in the buffer
		for(iter = this->valid_commands.begin(); iter != this->valid_commands.end(); ++iter) {
			// We search the string for the command prefix
			begin = this->recv_buffer.find(*iter);

			if(begin != string::npos) {
				// If we have found the prefix we search for the termination character
				end = this->recv_buffer.find("\n", begin);
				if(end != string::npos) {
					// If we have found a complete command we extract it from the buffer
					command = this->recv_buffer.substr(begin, end-begin);
					this->recv_buffer = recv_buffer.substr(end+1);

					// If there's a carriage retrun we remove it
					if(command.find("\r") != string::npos)
						command.erase(command.end()-1);
					return 0;
				}
			}
		}
	} catch(exception& e) {
		syslog(ATMD_ERR, "Network [check_buffer]: caught an exception (%s)", e.what());
	} catch(...) {
		syslog(ATMD_ERR, "Network [check_buffer]: unhandled exception");
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
	
	retval = recv(this->client_socket, &buffer, sizeof(char)*256, 0);
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


/* @fn Network::format_command(string format, ...)
 * This function takes a format string and a variable number of other inputs and
 * returns a formatted string object.
 *
 * @param format The format string.
 * @param ... A variable list of arguments corresponding to specifiers in format string.
 * @return Return a string. Should never fail.
 */
string Network::format_command(string format, ...) {
	char *buffer;
	string output;
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


/* @fn Network::exec_command(string command, ATMDboard& board)
 * This function get a command string and a reference to the board object on which the command should act.
 *
 * @param command The command string.
 * @param board A reference to the board object.
 * @return Return 0 on success or throw an exception on error.
 */
int Network::exec_command(string command, ATMDboard& board) {
	string main_command, parameters;
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
	uint32_t channel, rising, falling, refclkdiv, hsdiv, offset, measure_num, format;
	string window_time, measure_time, filename, win_start, win_ampl;

	// Configuration SET commands
	if(main_command == "SET") {

		// Catching channel setup command
		cmd_re = "CH (\\d+) (\\d+) (\\d+)";
		if(cmd_re.FullMatch(parameters)) {
			cmd_re.FullMatch(parameters, &channel, &rising, &falling);
			if(enable_debug)
				syslog(ATMD_DEBUG, "Network [exec_command]: setting channel %d (rising: %d, falling: %d).", channel, rising, falling);
			if(channel == 0) {
				board.enable_start_rising((bool)rising);
				board.enable_start_falling((bool)falling);
				this->send_command("ACK");

			} else if(channel > 0 && channel <= 8) {
				board.enable_rising(channel, (bool)rising);
				board.enable_falling(channel, (bool)falling);
				this->send_command("ACK");

			} else {
				syslog(ATMD_WARN, "Network [exec_command]: trying to set non-existent channel %d.", channel);
				this->send_command(this->format_command("ERR %d", ATMD_NETERR_BAD_CH));
			}if(terminate_interrupt)
				throw(ATMD_ERR_TERM);
			return 0;
		}

		// Catching window time setup command
		cmd_re = "ST ([0-9\\.\\,]+[umsMh]{1})";

		// Replace commas with dots (commas are not recognized as decimal point)
		pcrecpp::RE("\\,").GlobalReplace(".", &parameters);

		if(cmd_re.FullMatch(parameters)) {
			cmd_re.FullMatch(parameters, &window_time);
			if(enable_debug)
				syslog(ATMD_DEBUG, "Network [exec_command]: setting window time to \"%s\".", window_time.c_str());

			if(board.set_window(window_time)) {
				syslog(ATMD_WARN, "Network [exec_command]: the supplied time string is not valid as a measurement window (\"%s\")", window_time.c_str());
				this->send_command(this->format_command("ERR %d", ATMD_NETERR_BAD_TIMESTR));
			} else {
				this->send_command("ACK");
			}
			return 0;
		}

		// Catching total measure time setup command
		cmd_re = "TT ([0-9\\.\\,]+[umsMh]{0,1})";

		// Replace commas with dots (commas are not recognized as decimal point)
		pcrecpp::RE("\\,").GlobalReplace(".", &parameters);

		if(cmd_re.FullMatch(parameters)) {
			cmd_re.FullMatch(parameters, &measure_time);
			if(enable_debug)
				syslog(ATMD_DEBUG, "Network [exec_command]: setting total measure time to \"%s\".", measure_time.c_str());

			if(board.set_tottime(measure_time)) {
				syslog(ATMD_WARN, "Network [exec_command]: the supplied time string is not valid as the total measurement time (\"%s\")", measure_time.c_str());
				this->send_command(this->format_command("ERR %d", ATMD_NETERR_BAD_TIMESTR));
			} else {
				this->send_command("ACK");
			}
			return 0;
		}

		// Catching resolution setup command
		cmd_re = "RS (\\d+) (\\d+)";
		if(cmd_re.FullMatch(parameters)) {
			cmd_re.FullMatch(parameters, &refclkdiv, &hsdiv);
			if(enable_debug)
				syslog(ATMD_DEBUG, "Network [exec_command]: setting resolution (ref: %d, hs: %d).", refclkdiv, hsdiv);
			
			// We check that the parameters are meaningful. We cannot change the resolution too much!
			if(refclkdiv < 6 || hsdiv < 60) {
				syslog(ATMD_ERR, "Network [exec_command]: the resolution parameters passed are meaningless (ref: %d, hs: %d).", refclkdiv, hsdiv);
				this->send_command(this->format_command("ERR %d", ATMD_NETERR_BAD_PARAM));
				return 0;
			}
			board.set_resolution(refclkdiv, hsdiv);
			this->send_command("ACK");
			return 0;
		}

		// Catching offset setup command
		cmd_re = "OF (\\d+)";
		if(cmd_re.FullMatch(parameters)) {
			cmd_re.FullMatch(parameters, &offset);
			if(enable_debug)
				syslog(ATMD_DEBUG, "Network [exec_command]: setting offset to %d.", offset);
			
			// Start offset is truncated at a length of 18 bits
			board.set_start_offset(offset & 0x0003FFFF);
			this->send_command("ACK");
			return 0;
		}

		this->send_command(this->format_command("ERR %d", ATMD_NETERR_BAD_PARAM));
		return 0;

	// Configuration GET commands
	} else if(main_command == "GET") {

		// Get channel configuration
		cmd_re = "CH (\\d+)";
		if(cmd_re.FullMatch(parameters)) {
			cmd_re.FullMatch(parameters, &channel);
			if(enable_debug)
				syslog(ATMD_DEBUG, "Network [exec_command]: client requested configuration of channel %d.", channel);

			string answer;
			// If channel is zero we should send the configuration of the start channel
			if(channel == 0) {
				answer = this->format_command("VAL CH %d %d %d", channel, board.get_start_rising(), board.get_start_falling());

			// Here we send information on stop channels
			} else if(channel > 0 && channel <= 8) {
				answer = this->format_command("VAL CH %d %d %d", channel, ((board.get_rising_mask() >> (channel-1)) & 0x01), ((board.get_falling_mask() >> (channel-1)) & 0x01));

			// Here we send back an error because the requested channel is non-existent
			} else {
				answer = this->format_command("ERR %d", ATMD_NETERR_BAD_CH);
			}
			this->send_command(answer);
			return 0;
		}

		// Get window time_bin
		if(parameters == "ST") {
			if(enable_debug)
				syslog(ATMD_DEBUG, "Network [exec_command]: client requested configured window time.");

			string answer = "VAL ST ";
			answer += board.get_window();
			this->send_command(answer);
			return 0;
		}

		// Get measure total time
		if(parameters == "TT") {
			if(enable_debug)
				syslog(ATMD_DEBUG, "Network [exec_command]: client requested configured total measure time.");
			
			string answer = "VAL TT ";
			answer += board.get_tottime();
			this->send_command(answer);
			return 0;
		}

		// Get resolution
		if(parameters == "RS") {
			if(enable_debug)
				syslog(ATMD_DEBUG, "Network [exec_command]: client requested configured resolution.");

			uint16_t refclk, hs;
			board.get_resolution(refclk, hs);
			string answer = this->format_command("VAL RS %d %d", refclk, hs);
			this->send_command(answer);
			return 0;
		}

		// Get offset
		if(parameters == "OF") {
			if(enable_debug)
				syslog(ATMD_DEBUG, "Network [exec_command]: client requested configured offset.");

			string answer = this->format_command("VAL OF %d", board.get_start_offset());
			this->send_command(answer);
			return 0;
		}

		this->send_command(this->format_command("ERR %d", ATMD_NETERR_BAD_PARAM));
		return 0;

	// Measurement MSR commands
	} else if(main_command == "MSR") {

		// Start measure command
		if(parameters == "START") {
			if(enable_debug)
				syslog(ATMD_DEBUG, "Network [exec_command]: client requested to start a measure.");

			int retval;
			switch(board.get_status()) {
				case ATMD_STATUS_FINISHED:
					if(board.collect_measure(retval)) {
						this->send_command(this->format_command("ERR %d", ATMD_NETERR_THJOIN));
						break;
					}

				case ATMD_STATUS_IDLE:
					if(board.start_measure()) {
						// Error starting measurement
						this->send_command(this->format_command("ERR %d", ATMD_NETERR_START));

					} else {
						// Measure started correctly
						this->send_command("ACK");
					}
					break;

				case ATMD_STATUS_ERROR:
					// TODO: What we do in this situation? 
					// We can start the measure but we loose information about the last measurement error!
					// Maybe we can send the thread error as an answer and then reset the status.
					this->send_command(this->format_command("ERR %d", ATMD_NETERR_STATUS_ERR));
					break;

				case ATMD_STATUS_RUNNING:
				case ATMD_STATUS_STARTING:
					// A measure is already running. ATMD_STATUS_RUNNING should be never seen but who knows...
					this->send_command(this->format_command("ERR %d", ATMD_NETERR_STATUS_RUN));
					break;

				default:
					// We got an unkown status? This should not happen so we log a warning.
					syslog(ATMD_WARN, "Network [exec_command]: found the board in an unknown status (or not impemented here!).");
					this->send_command(this->format_command("ERR %d", ATMD_NETERR_STATUS_UNKN));
					break;
			}

			return 0;
		}

		// Stop measure command
		if(parameters == "STOP") {
			if(enable_debug)
				syslog(ATMD_DEBUG, "Network [exec_command]: client requested to stop current measure.");

			int retval;
			switch(board.get_status()) {
				// There is no measure to stop
				case ATMD_STATUS_FINISHED:
					if(board.collect_measure(retval)) {
						this->send_command(this->format_command("ERR %d", ATMD_NETERR_THJOIN));
						break;
					}
				case ATMD_STATUS_IDLE:
					this->send_command(this->format_command("ERR %d", ATMD_NETERR_STATUS_IDLE));
					break;

				// There was an error in the previous measure
				case ATMD_STATUS_ERROR:
					// TODO: What we do in this situation?
					this->send_command(this->format_command("ERR %d", ATMD_NETERR_STATUS_ERR));
					break;

				case ATMD_STATUS_RUNNING:
				case ATMD_STATUS_STARTING:
					// Stopping measurement
					syslog(ATMD_INFO, "Network [exec_command]: stopping measurement...");
					if(board.stop_measure()) {
						this->send_command(this->format_command("ERR %d", ATMD_NETERR_STOP));
					} else {
						this->send_command("ACK");
					}
					break;

				default:
					// We got an unknown status?
					syslog(ATMD_WARN, "Network [exec_command]: found the board in an unknown status (or not impemented here!).");
					this->send_command(this->format_command("ERR %d", ATMD_NETERR_STATUS_UNKN));
					break;
			}
			return 0;
		}

		// Abort measure command
		if(parameters == "ABORT") {
			if(enable_debug)
				syslog(ATMD_DEBUG, "Network [exec_command]: client requested to abort current measure.");

			int retval;
			switch(board.get_status()) {
				// There is no measure to abort
				case ATMD_STATUS_FINISHED:
					if(board.collect_measure(retval)) {
						this->send_command(this->format_command("ERR %d", ATMD_NETERR_THJOIN));
						break;
					}
				case ATMD_STATUS_IDLE:
					this->send_command(this->format_command("ERR %d", ATMD_NETERR_STATUS_IDLE));
					break;

				// There was an error in the previous measure
				case ATMD_STATUS_ERROR:
					// TODO: What we do in this situation? 
					this->send_command(this->format_command("ERR %d", ATMD_NETERR_STATUS_ERR));
					break;

				case ATMD_STATUS_RUNNING:
				case ATMD_STATUS_STARTING:
					// Aborting measurement
					syslog(ATMD_INFO, "Network [exec_command]: aborting measurement...");
					if(board.abort_measure()) {
						this->send_command(this->format_command("ERR %d", ATMD_NETERR_ABORT));
					} else {
						this->send_command("ACK");
					}
					break;

				default:
					// We got an unknown status?
					syslog(ATMD_WARN, "Network [exec_command]: found the board in an unknown status (or not impemented here!).");
					this->send_command(this->format_command("ERR %d", ATMD_NETERR_STATUS_UNKN));
					break;
			}
			return 0;
		}

		// Get measure status
		if(parameters == "STATUS") {
			string command = "MSR STATUS ";
			int retval;
			switch(board.get_status()) {
				case ATMD_STATUS_STARTING:
				case ATMD_STATUS_RUNNING:
					this->send_command(command.append("RUNNING"));
					break;

				case ATMD_STATUS_IDLE:
					this->send_command(command.append("IDLE"));
					break;

				case ATMD_STATUS_ERROR:
					if(board.collect_measure(retval)) {
						this->send_command(command.append("ERR 0"));
					} else {
						this->send_command(this->format_command(command.append("ERR %d"), retval));
					}
					break;

				case ATMD_STATUS_FINISHED:
					if(board.collect_measure(retval)) {
						this->send_command(command.append("ERR 0"));
					} else {
						this->send_command(command.append("FINISHED"));
					}
					break;

				// We found the board in an unkown status.
				default:
					syslog(ATMD_WARN, "Network [exec_command]: found the board in an unknown status (or not impemented here!).");
					this->send_command(command.append("UNKN"));
					break;
			}
			return 0;
		}

		// List measures command
		if(parameters == "LST") {
			if(enable_debug)
				syslog(ATMD_DEBUG, "Network [exec_command]: client requested the list of unsaved measures. List contained %u measures.", board.num_measures());
			if(board.num_measures() > 0) {
				this->send_command(this->format_command("MSR LST NUM %u", board.num_measures()));
				int i;
				for(i = 0; i < board.num_measures(); i++) {
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
			return 0;
		}

		// Save measure command
		cmd_re = "SAV (\\d+) (\\d+) (\\/[a-zA-Z0-9\\.\\_\\-\\/]+)";
		if(cmd_re.FullMatch(parameters)) {
			cmd_re.FullMatch(parameters, &measure_num, &format, &filename);
			if(enable_debug)
				syslog(ATMD_DEBUG, "Network [exec_command]: client requested to save measure %u to file \"%s\"", measure_num, filename.c_str());

			if(board.save_measure(measure_num, filename, format)) {
				this->send_command(this->format_command("ERR %d", ATMD_NETERR_SAV));
			} else {
				this->send_command("ACK");
			}
			return 0;
		}

		// Send to client measure statistics
		vector< vector<uint32_t> > stopcount;
		string modifier = "";
		cmd_re = "STAT (\\-?)(\\d+) ([0-9\\.\\,]+[umsMh]{1}) ([0-9\\.\\,]+[umsMh]{1})";
		if(cmd_re.FullMatch(parameters)) {
			cmd_re.FullMatch(parameters, &modifier, &measure_num, &win_start, &win_ampl);
			if(enable_debug)
				syslog(ATMD_DEBUG, "Network [exec_command]: client requested stats of measure %u with window (s = %s, a = %s).", measure_num, win_start.c_str(), win_ampl.c_str());

			if(board.stat_stops(measure_num, stopcount, win_start, win_ampl)) {
				this->send_command("MSR STAT ERR");
			} else {
				uint32_t starts, index;
				if(modifier == "-") {
					vector<uint32_t> stopsum(8,0);
					uint64_t mean_window = 0;
					for(vector< vector<uint32_t> >::iterator iter = stopcount.begin(); iter!=stopcount.end(); ++iter) {
						for(index = 0; index < 8; index++)
							stopsum[index] += iter->at(index+1);
						mean_window += iter->at(0);
					}
					this->send_command(this->format_command("MSR STAT %u %u %u %u %u %u %u %u %u %u", stopcount.size(), mean_window / stopcount.size(), stopsum.at(0), stopsum.at(1), stopsum.at(2), stopsum.at(3), stopsum.at(4), stopsum.at(5), stopsum.at(6), stopsum.at(7)));

				} else {
					board.stat_measure(measure_num, starts);
					this->send_command(this->format_command("MSR STAT NUM %u", starts));

					for(vector< vector<uint32_t> >::iterator iter = stopcount.begin(); iter!=stopcount.end(); ++iter) {
						this->send_command(this->format_command("MSR STAT %u %u %u %u %u %u %u %u %u %u", iter - stopcount.begin() + 1, iter->at(0), iter->at(1), iter->at(2), iter->at(3), iter->at(4), iter->at(5), iter->at(6), iter->at(7), iter->at(8)));
					}
				}
			}
			return 0;
		}

		cmd_re = "STAT (\\-?)(\\d+)";
		if(cmd_re.FullMatch(parameters)) {
			cmd_re.FullMatch(parameters, &modifier, &measure_num);
			if(enable_debug)
				syslog(ATMD_DEBUG, "Network [exec_command]: client requested stats of measure %u.", measure_num);

			if(board.stat_stops(measure_num, stopcount)) {
				this->send_command("MSR STAT ERR");
			} else {
				uint32_t starts, index;
				if(modifier == "-") {
					vector<uint32_t> stopsum(8,0);
					uint64_t mean_window = 0;
					for(vector< vector<uint32_t> >::iterator iter = stopcount.begin(); iter!=stopcount.end(); ++iter) {
						for(index = 0; index < 8; index++)
							stopsum[index] += iter->at(index+1);
						mean_window += iter->at(0);
					}
					this->send_command(this->format_command("MSR STAT %u %u %u %u %u %u %u %u %u %u", stopcount.size(), mean_window / stopcount.size(), stopsum.at(0), stopsum.at(1), stopsum.at(2), stopsum.at(3), stopsum.at(4), stopsum.at(5), stopsum.at(6), stopsum.at(7)));

				} else {
					board.stat_measure(measure_num, starts);
					this->send_command(this->format_command("MSR STAT NUM %u", starts));

					for(vector< vector<uint32_t> >::iterator iter = stopcount.begin(); iter!=stopcount.end(); ++iter) {
						this->send_command(this->format_command("MSR STAT %u %u %u %u %u %u %u %u %u %u", iter - stopcount.begin() + 1, iter->at(0), iter->at(1), iter->at(2), iter->at(3), iter->at(4), iter->at(5), iter->at(6), iter->at(7), iter->at(8)));
					}
				}
			}
			return 0;
		}


		// Delete measure command
		cmd_re = "DEL (\\d+)";
		if(cmd_re.FullMatch(parameters)) {
			cmd_re.FullMatch(parameters, &measure_num);
			if(enable_debug)
				syslog(ATMD_DEBUG, "Network [exec_command]: client requested to delete measure %u.", measure_num);

			if(board.del_measure(measure_num)) {
				this->send_command(this->format_command("ERR %d", ATMD_NETERR_DEL));
			} else {
				this->send_command("ACK");
			}
			return 0;
		}

		// Clear measures command
		if(parameters == "CLR") {
			if(enable_debug)
				syslog(ATMD_DEBUG, "Network [exec_command]: client requested to clear all measures.");
			board.clear_measures();
			this->send_command("ACK");
			return 0;
		}

		this->send_command(this->format_command("ERR %d", ATMD_NETERR_BAD_PARAM));
		return 0;

	} else if(main_command == "EXT") {
		// Terminate client session
		// As first thing, we need to check the board status and eventually stop a running measure.
		int retval;
		switch(board.get_status()) {
			// There is no measure to abort
			case ATMD_STATUS_FINISHED:
			case ATMD_STATUS_ERROR:
				board.collect_measure(retval);
				break;

			case ATMD_STATUS_RUNNING:
			case ATMD_STATUS_STARTING:
				board.abort_measure();
				break;

			case ATMD_STATUS_IDLE:
			default:
				break;
		}

		// Then we clear all measures...
		board.clear_measures();

		// ... and we reset board configuration
		board.reset_config();

		// We close the connection throwing an exception
		throw(ATMD_ERR_CLOSED);

	} else {
		// This case should never be reached. We log a critical message.
		syslog(ATMD_CRIT, "Network [exec_command]: got an unknown command and this should never happen! Command was \"%s\".", command.c_str());
		return 0;
	}
}
