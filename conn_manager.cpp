#include "conn_manager.hpp"

/* Timer */
Timer::Timer(size_t fd) : conn_fd(fd) {
	struct timespec tv = {0, 0};
	clock_gettime(CLOCK_MONOTONIC, &tv);
	time = int(tv.tv_sec) * 1000 + tv.tv_nsec / 1000 / 1000;
}

int Timer::get_time() {
	return time;
}

void Timer::set_time(int new_time) {
	time = new_time;
}

int Timer::get_connection_fd() {
	return conn_fd;
}

bool Timer::operator==(const Timer &second) const {
	return (time == second.time) && (conn_fd == second.conn_fd);
}

bool Timer::operator!=(const Timer &second) const {
	return (time != second.time) || (conn_fd != second.conn_fd);
}

/* TimerManager */
int TimerManager::get_monotonic_ms() {
	struct timespec tv = {0, 0};
	clock_gettime(CLOCK_MONOTONIC, &tv);
	return int(tv.tv_sec) * 1000 + tv.tv_nsec / 1000 / 1000;
}
	
int TimerManager::get_next_timer() {
	//Connections timers
	int now_ms = get_monotonic_ms();
	int next_timer_ms = -1;
	
	if (!timers_q.empty())
		next_timer_ms = timers_q.front().get_time() + CONN_TIMEOUT_MS;
	
	if (next_timer_ms == -1)
		return -1; //no timeout needed if there's no timers
		
	if (next_timer_ms <= now_ms) {
		//possible missed so no need to timeout
		//will cause poll() to return immediately
		return 0; 
	}
		
	return next_timer_ms - now_ms;
}

//removes all expired timers 
//and returns all related expired connections to remove
std::vector<size_t> TimerManager::process_timers() {
	std::vector<size_t> expired_conns;
	int now_ms = get_monotonic_ms();
	
	auto it = timers_q.begin();
	while (it != timers_q.end()) {
		int next_timer_ms = it->get_time() + CONN_TIMEOUT_MS;
		if (next_timer_ms >= now_ms)
			break; //the rest of the timers are still active
		
		expired_conns.push_back(it->get_connection_fd());
		
		//erase its timer from the queue
		it = timers_q.erase(it);
		if (it != timers_q.end())
			it++;
			
		//TODO remove from map	
	}
	
	return expired_conns;
}

Timer *TimerManager::add_timer(size_t conn_fd) {
	//start timer and add it to the queue
	timers_q.push_back(Timer(conn_fd));
	
	return &timers_q.back();
}

void TimerManager::remove_timer(Timer *timer) {
	auto it = std::find(timers_q.begin(), timers_q.end(), *timer);
	if (it != timers_q.end()) {
		timers_q.erase(it);
	}
}

/* Conn */
Conn::Conn(int fd) : socket_fd(fd), 
		incoming(BUFF_CAPACITY), outgoing(BUFF_CAPACITY) {}

//Getters
int Conn::get_fd() const {
	return socket_fd;
}

bool Conn::is_readable() const {
	return want_read;
}

bool Conn::is_writable() const {
	return want_write;
}

bool Conn::is_closing() const {
	return want_close;
}

Timer *Conn::get_timer() const {
	return timer;
}

//Setters
void Conn::set_want_read(bool isRead) {
	want_read = isRead;
}

void Conn::set_want_write(bool isWrite) {
	want_write = isWrite;
}

void Conn::mark_as_closing() {
	want_close = true;
}

void Conn::set_timer(Timer *t) {
	timer = t;
}

void Conn::append_to_outgoing(size_t len) {
	outgoing.insert(incoming.begin(), incoming.begin() + len);
}

void Conn::append_to_incoming(std::vector<uint8_t>& buff, size_t len) {
	incoming.insert(buff.begin(), buff.begin() + len);
}

void Conn::consume_from_incoming(size_t len) {
	incoming.erase_front(len);
}

void Conn::consume_from_outgoing(size_t len) {
	outgoing.erase_front(len);
}

void Conn::prepare_for_response(size_t *header) {
	assert(header);
	/* since we append response straight to outgoing 
	* and the length of the response isn't fixed 
	* we want to reserve place for the header in advance*/
	*header = outgoing.size();
	outgoing.insert((char)0, (size_t)HEADER_SIZE);
}

void Conn::complete_response(size_t header) {
	uint32_t resp_size = outgoing.size() - header - HEADER_SIZE;
	outgoing.memcpy(header, (uint8_t *)&resp_size, HEADER_SIZE);
}

bool Conn::handle_request(CommandExecutor &command_exec) {

	if (incoming.size() < HEADER_SIZE)
		return false; //not ready yet
		
	size_t len = 0;
	//TODO think how to replace memcpy
	std::memcpy(&len, incoming.data(), HEADER_SIZE); 
	
	if (len > MAX_MSG_LEN) {
		//shouldn't get here since client checks the msg's len
		//and the read buff size is limmited by the max_msg_len as well
		mark_as_closing();
		return false;
	}
	
	size_t packet_len = HEADER_SIZE + len;
	if (packet_len > incoming.size())
		return false; //not ready yet
	
	auto vector_buffer = incoming.to_vector();
	std::vector<uint8_t> payload(vector_buffer.begin() + HEADER_SIZE, 
								vector_buffer.begin() + packet_len);
	
	//result = {bool success, vector<str> cmd, str error_mas}
	auto result = parser.parse(payload, len);
	if (!result.success) {
		outgoing.append_err(RES_CANTREAD, result.error_msg);
		mark_as_closing();
		return false;
	}
	
	//process the cmd by finding its arg in HashMap
	//create a response, serialize it and add to outgoing buff
	size_t header_pos = 0;
	prepare_for_response(&header_pos);
	try {
		command_exec.do_query(result.cmd, outgoing); 
	}//TODO clear already inserted parts from output in case of exception
	catch (const std::exception &e) {
		outgoing.append_err(RES_TOOLONG, "response is too long");
		return false;
	}
	complete_response(header_pos);
	
	append_to_outgoing(packet_len);
	consume_from_incoming(packet_len);
		
	return true;	
}

void Conn::handle_write() {
	assert(outgoing.size() > 0);
	ssize_t rv = send(socket_fd, outgoing.data(), outgoing.size(), 0);
	if (rv < 0) {
		
		if (errno & EAGAIN)
			return; //not ready yet
		
		mark_as_closing();
		return; //error
	}	
	consume_from_outgoing((size_t)rv);
	
	if (outgoing.size() == 0) {
		want_read = true;
		want_write = false;
	}
}

void Conn::handle_read(CommandExecutor &command_exec) {
	std::vector<uint8_t> rbuf(HEADER_SIZE + MAX_MSG_LEN + 1);
	ssize_t rv = recv(socket_fd, rbuf.data(), rbuf.size(), 0);
	if (rv <= 0) {
		/* if there was an error but not because of one of these:
		* an unexpected signal(EINTR)
		* no data received from the socket or timeout(EAGAIN, EWOULDBLOCK) */
		if (errno && ((errno & (EAGAIN | EWOULDBLOCK | EINTR)) == 0))
			perror("recv()");
		
		mark_as_closing();
		return;
	}

	//incoming.insert(incoming.end(), rbuf.begin(), rbuf.begin() + (size_t)rv);
	append_to_incoming(rbuf, (size_t)rv);
	//incoming.insert(rbuf.begin(), rbuf.begin() + (size_t)rv);
	
	//for a pipeline
	while (handle_request(command_exec)) {}
	
	if (outgoing.size() > 0) {
		want_read = false;
		want_write = true;
		
		return handle_write();
	}
}

/* ConnectionManager */
int ConnectionManager::handle_accept(int listen_fd) {
	char ip[INET6_ADDRSTRLEN];
	struct sockaddr_storage client_addr;
	
	socklen_t client_len = sizeof(client_addr);
	int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);
	if (client_fd < 0)
		return -1; //error in accept
	
	inet_ntop(client_addr.ss_family, 
		get_in_addr((struct sockaddr *)&client_addr), ip, sizeof(ip));
	
	std::cout << "server: got connection from " << ip << "\n";
	
	//set timers for default connection timeout
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = IO_TIMEOUT_MS;
	//set read and write timeouts:
	setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
	setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);

	
	auto new_conn = std::make_unique<Conn>(Conn(client_fd));
	// want to read first request
	new_conn->set_want_read(true);
	new_conn->set_timer(tm.add_timer(client_fd));
	
	fd2conn[client_fd] = std::move(new_conn);
	
	return client_fd;
}

void ConnectionManager::close_conn(size_t conn_fd) {
	//TODO what close return upon failure and throw exception
	auto it = fd2conn.find(conn_fd);
	if (it != fd2conn.end()) {
		std::cout << "removing connection " << conn_fd << "\n";
		const auto &conn_ptr = it->second;
		tm.remove_timer(conn_ptr->get_timer());
		while ((close(conn_fd) == -1) && (errno & (EINTR | EIO)));
		fd2conn[conn_fd].reset(); //clean unique_ptr
	}
}

bool ConnectionManager::is_closing(size_t conn_fd) {
	//TODO what close return upon failure and throw exception
	auto it = fd2conn.find(conn_fd);
	if (it != fd2conn.end()) {
		const auto &conn_ptr = it->second;
		return conn_ptr->is_closing();
	}
	
	return false; //if conn_fd is invalid there's nothing to close
}

bool ConnectionManager::is_readable(size_t conn_fd) {
	//TODO what close return upon failure and throw exception
	auto it = fd2conn.find(conn_fd);
	if (it != fd2conn.end()) {
		const auto &conn_ptr = it->second;
		return conn_ptr->is_readable();
	}
	
	return false;
}

bool ConnectionManager::is_writable(size_t conn_fd) {
	//TODO what close return upon failure and throw exception
	auto it = fd2conn.find(conn_fd);
	if (it != fd2conn.end()) {
		const auto &conn_ptr = it->second;
		return conn_ptr->is_writable();
	}
	
	return false;
}

std::vector<size_t> ConnectionManager::get_all_connections() {
	std::vector<size_t> v;
	
	for (const auto &pair : fd2conn) {
		const auto &conn_ptr = pair.second;
		if (conn_ptr) {
			v.push_back(conn_ptr->get_fd());
		}
	}
	
	return v;
}

void ConnectionManager::handle_read(size_t conn_fd) {
	//TODO what close return upon failure and throw exception
	auto it = fd2conn.find(conn_fd);
	if (it != fd2conn.end()) {
		const auto &conn_ptr = it->second;
		if (conn_ptr->is_readable())
			conn_ptr->handle_read(command_exec);
	}
}

void ConnectionManager::handle_write(size_t conn_fd) {
	//TODO what close return upon failure and throw exception
	auto it = fd2conn.find(conn_fd);
	if (it != fd2conn.end()) {
		const auto &conn_ptr = it->second;
		if (conn_ptr->is_writable())
			conn_ptr->handle_write();
	}
}

void ConnectionManager::check_timers() {
	auto conns_to_close = tm.process_timers();
	
	for (size_t fd : conns_to_close) {
		close_conn(fd);
	}
}

void ConnectionManager::update_timer(size_t conn_fd) {
	auto it = fd2conn.find(conn_fd);
	if (it == fd2conn.end())
		return; //TODO throw exception
	
	const auto &conn_ptr = it->second;
	tm.remove_timer(conn_ptr->get_timer());
	conn_ptr->set_timer(tm.add_timer(conn_fd));
}

int ConnectionManager::get_next_timer() {
	return tm.get_next_timer();
}

