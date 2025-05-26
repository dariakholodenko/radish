#ifndef __CONN_MANAGER_HPP__
#define __CONN_MANAGER_HPP__

//c++
#include <algorithm> //std::find()
#include <cassert> //assert()
#include <cstring> //std::memcpy
#include <iostream>
#include <memory> //unique_ptr
#include <unordered_map>
#include <vector>

//networking
#include <arpa/inet.h> //inet_ntop converts network address to string

//c
#include <errno.h>
#include <time.h> //clock_gettime() for better performance and poll() compatibility
#include <unistd.h> //close()

//custom
#include "buffer.hpp" //RingBuffer
#include "commands.hpp"
#include "io_shared_library.hpp" //MAX_MSG_LEN, get_in_addr
#include "protocol.hpp" //RequestParser

constexpr size_t CONN_TIMEOUT_MS = 5000; //5000 ms
constexpr size_t IO_TIMEOUT_MS = 500;
constexpr size_t BUFF_CAPACITY = 2 * (HEADER_SIZE + MAX_MSG_LEN);

class Timer {
private:
	int time;
	int conn_fd;

public:
	Timer(size_t fd);
	int get_time();
	void set_time(int new_time);
	int get_connection_fd();
	bool operator==(const Timer &second) const;
	bool operator!=(const Timer &second) const;
};

/* TimerManager
 * Handles timers for connections to detect expired ones */
class TimerManager {
private:
	//connection timers' queue for checking timeouts
	std::vector<Timer> timers_q;
	
	int get_monotonic_ms();
	
public:
	int get_next_timer();
	
	//removes all expired timers 
	//and returns all related expired connections to remove
	std::vector<size_t> process_timers();
	Timer *add_timer(size_t conn_fd);
	void remove_timer(Timer *timer);
};

class Conn {
private:
	int socket_fd;
	//app's intention for the event loop
	bool want_read = false;
	bool want_write = false;
	bool want_close = false;
	
	//buffered input and output
	RingBuffer<uint8_t> incoming; //request to be parsed from the app
	RingBuffer<uint8_t> outgoing; //response for the app
	
	//timer
	Timer *timer = nullptr;
	
	RequestParser parser; //parses clients requests
	
public:
	Conn(int fd);
	
	//Getters
	int get_fd() const;
	bool is_readable() const;
	bool is_writable() const;
	bool is_closing() const;
	Timer *get_timer() const;
	
	//Setters
	void set_want_read(bool isRead);
	void set_want_write(bool isWrite);
	void mark_as_closing();
	
	void set_timer(Timer *t);
	
	void append_to_outgoing(size_t len);
	void append_to_incoming(std::vector<uint8_t>& buff, size_t len);
	void consume_from_incoming(size_t len);
	void consume_from_outgoing(size_t len);
	
	void prepare_for_response(size_t *header);
	void complete_response(size_t header);
	
	bool handle_request(CommandExecutor &command_exec);
	void handle_write();
	void handle_read(CommandExecutor &command_exec);
};

/* ConnectionManager
 * it's a connections interface to encapsulate them from server
 * it stores all active connections as a map(fd<->conn),
 * accepts new connections(clients) and do a cleanup afterwards,
 * tells the event loop when a conn is ready to read/write/close */
class ConnectionManager {
private:
	//map all client connection to fds, used as keys, to save the state for event loop
	std::unordered_map<size_t, std::unique_ptr<Conn>> fd2conn;
	TimerManager tm;
	CommandExecutor command_exec;

public:
	int handle_accept(int listen_fd);
	void close_conn(size_t conn_fd);
	
	bool is_closing(size_t conn_fd);
	bool is_readable(size_t conn_fd);
	bool is_writable(size_t conn_fd);
	std::vector<size_t> get_all_connections();
	
	void handle_read(size_t conn_fd);
	void handle_write(size_t conn_fd);
	
	void check_timers();
	void update_timer(size_t conn_fd);
	int get_next_timer();
};

#endif
