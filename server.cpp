#include <thread> //threads

#include "wrapper.hpp"
#include "custom_heap.hpp"

#define DEBUG_R 0 //debug for read-related functions
#define DEBUG_W 0 //debug for write-related functions

//using namespace hm;
//TODO:
//think of a different approach without a global variable use
//Hash Map for storing key-value pairs of the Data Base
static HashMap<std::string, Entry *> g_hmap(HMAP_BASE_CAPACITY);

static SortSet g_sset(HMAP_BASE_CAPACITY);

//map all client connection to fds, used as keys, to save the state for event loop
//vector is used since for every new fd (used as a key)
//the smallest possible number is allocated and keys won't be overlapping 
//and too widely distributed
static std::vector <Conn *> fd2conn;

//timers' FIFO
static std::vector<Timer> g_timers;

//TTL
//we use heap since ttl can be received in any order in contrast to Conn timers
static CustomHeap ttl_heap;


/** Current protocol:
	req:
		+------+------+------+-----+------+
		| len1 | msg1 | len2 | msg2 | ... |
		+------+------+------+-----+------+
	
	msg format:
		+------+------+------+------+-----+------+------+
		| nstr | len1 | str1 | len2 | ... | lenn | strn |
		+------+------+------+------+-----+------+------+
	
	resp:
		+--------+------+
		| status | data |
		+--------+------+
**/

//set fd to non-blocking mode(as a file)
static void fd_set_nb(int fd) {
	if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK) == -1) {
		if (errno & EINTR) {
			fd_set_nb(fd);
		}	
		else die("fcntl()");
	}
}

Conn *handle_accept(int fd) {
	char ip[INET6_ADDRSTRLEN];
	struct sockaddr_storage client_addr;
	
	socklen_t client_len = sizeof(client_addr);
	int connfd = accept(fd, (struct sockaddr *)&client_addr, &client_len);
	if (connfd < 0) 
		return NULL; //error in accept
	
	inet_ntop(client_addr.ss_family, 
		get_in_addr((struct sockaddr *)&client_addr), ip, sizeof(ip));
	
	printf("server: got connection from %s\n", ip);
	
	//set the new connfd to non-blocking mode
	fd_set_nb(connfd);
	
	
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = IO_TIMEOUT;
	setsockopt(connfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
	setsockopt(connfd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);

	
	Conn *new_conn = new Conn(connfd);
	new_conn->want_read = true; // want to read first request
	g_timers.push_back(Timer(new_conn)); //start timer and add it to FIFO
	new_conn->timer = &g_timers.back();
	
	return new_conn;
}

static ssize_t read_header(const uint8_t *&data, const uint8_t *&end, 
													size_t &header) {
	if (data + HEADER_SIZE > end) 	{
		return -1;
	}
	
	memcpy(&header, data, HEADER_SIZE); 
	data += HEADER_SIZE;
	
	return 0;
}

static ssize_t read_str(const uint8_t *&data, const uint8_t *&end, 
										size_t len, std::string &str) {
	if (data + len > end) 	{
		return -1;
	}
	
	str.assign(data, data + len); 
	data += len;
	
	return 0;
}

static ssize_t parse_req(const std::vector<uint8_t> &request, size_t req_len, 
										std::vector<std::string> &cmd) {
	//TODO:
	//maybe receive request as uint8_t* to prevent unnecessary convertions
	const uint8_t *req_begin = request.data();
	const uint8_t *req_end = req_begin + req_len;
	size_t nstr = 0;
	std::string reqstr(request.begin(), request.end());
	
	if (read_header(req_begin, req_end, nstr) < 0) 	{
		std::cerr << "unexpected early end of request: " << reqstr << "\n";
		return -1;
	}
	
	while (cmd.size() < nstr) {
		size_t len = 0;
		if (read_header(req_begin, req_end, len) < 0) 	{
			std::cerr << "unexpected early end of request: " << reqstr << "\n";
			return -1;
		}
		
		cmd.push_back(std::string());
		if (read_str(req_begin, req_end, len, cmd.back()) < 0) 	{
			std::cerr << "the string is too long, len: " << len << "\n";
			return -1;
		}
	}
	
	if (req_begin != req_end) {
		std::cerr << "unexpected early end of request: " << reqstr << "\n";
		return -1;
	}
		
	return 0;
}

static void append_nil(RingBuffer<uint8_t> &buffer) {
	buffer.push_back(TAG_NIL);
}

static void append_int(RingBuffer<uint8_t> &buffer, int64_t val) {
	buffer.push_back(TAG_INT);
	append_helper(buffer, val, sizeof(uint64_t));
}

static void append_dbl(RingBuffer<uint8_t> &buffer, double val) {
	buffer.push_back(TAG_DBL);
	append_helper(buffer, val, sizeof(double));
}

static void append_str(RingBuffer<uint8_t> &buffer, const std::string &val) {
	buffer.push_back(TAG_STR);
	
	//size() returns the amount of bytes in str
	append_helper(buffer, (uint32_t)val.size(), sizeof(uint32_t));
	buffer.insert((uint8_t *)val.data(), val.size());
}

static void append_arr(RingBuffer<uint8_t> &buffer, uint32_t n) {
	buffer.push_back(TAG_ARR);
	append_helper(buffer, n, sizeof(uint32_t));
}

static void append_err(RingBuffer<uint8_t> &buffer, uint32_t code, 
											const std::string &msg) {
	buffer.push_back(TAG_ERR);
	append_helper(buffer, code, sizeof(uint32_t));
	append_helper(buffer, (uint32_t)msg.size(), sizeof(uint32_t));
	buffer.insert((uint8_t *)msg.data(), msg.size());
	
}

static void do_request(std::vector<std::string> &cmd, 
										RingBuffer<uint8_t> &buffer) {
	if (cmd.size() == 2 && cmd[0] == "get") {
		auto it = g_hmap.search(cmd[1]);
		if (it != g_hmap.end()) {
			auto val = it.second()->value;
			append_str(buffer, val);
		}
		else {
			append_nil(buffer);
		}
	}
	else if (cmd.size() == 3 && cmd[0] == "set") {
		g_hmap.insert(cmd[1], new Entry(cmd[1], cmd[2]));
		append_nil(buffer);
	}
	else if (cmd.size() == 2 && cmd[0] == "del") {
		auto val = g_hmap.erase(cmd[1]);
		int rc = (val != nullptr); //check is succeed in key deletion
		delete *val;
		append_int(buffer, rc);
	}
	else if (cmd.size() == 3 && cmd[0] == "zadd") {
		int score = stoi(cmd[2]); //TODO check that we received actual number
		int rc = g_sset.insert(cmd[1], score);
		//rc == 1: key was added
		//rc == 0: key was updated
		append_int(buffer, rc);
	}
	else if (cmd.size() == 2 && cmd[0] == "zrem") {
		int rc = g_sset.erase(cmd[1]);
		//rc == 1: key was removed
		//rc == 0: no key was found
		append_int(buffer, rc);
	}
	else if (cmd.size() == 3 && cmd[0] == "zrange") {
		auto v = g_sset.range(stoi(cmd[1]), stoi(cmd[2]));
		//rc == 1: key was removed
		//rc == 0: no key was found
		append_arr(buffer, v.size());
		for (auto it : v) {
			append_str(buffer, it);
		}
	}
	else if (cmd.size() == 3 && cmd[0] == "expire") {
		auto it = g_hmap.search(cmd[1]);
		if (it != g_hmap.end()) {
			int ttl = stoi(cmd[2]); //TODO check that we received actual number
			if (it.second()->get_heap_idx() == -1) {
				ttl_heap.insert(ttl, it.second());
			}
			else {
				ttl_heap.update_key(it.second()->get_heap_idx(), ttl);
			}
			append_int(buffer, OK); //success in setting expiration
		}
		else {
			append_int(buffer, EXPIRED); //the key is expired or doesn't exist
		}
	}
	else if (cmd.size() == 2 && cmd[0] == "persist") {
		auto it = g_hmap.search(cmd[1]);
		if (it != g_hmap.end()) {
			if (it.second()->get_heap_idx() != -1) {
				ttl_heap.delete_key(it.second()->get_heap_idx());
				append_int(buffer, OK);
			}			
		}
		else {
			append_int(buffer, EXPIRED); //the key is expired or doesn't exist
		}
	}
	else if (cmd.size() == 2 && cmd[0] == "ttl") {
		auto it = g_hmap.search(cmd[1]);
		if (it != g_hmap.end()) {
			if (it.second()->get_heap_idx() != -1) {
				append_int(buffer, it.second()->get_ttl());
			}
			else {
				append_int(buffer, NOTTL); //no ttl set for the key
			}
		}
		else {
			append_int(buffer, EXPIRED); //the key is expired or doesn't exist
		}
	}
	else {
		append_err(buffer, RES_NOCMD, "command doesn't exist");
	}
}

static void prepare_for_response(RingBuffer<uint8_t> &buffer, size_t *header) {
	assert(header);
	/* since we append response straight to outgoing 
	 * and the length of the response isn't fixed 
	* we want to reserve place for the header in advance*/
	*header = buffer.size();
	append_helper(buffer, 0, HEADER_SIZE);
}

static void complete_response(RingBuffer<uint8_t> &buffer, size_t header) {
	uint32_t resp_size = buffer.size() - header - HEADER_SIZE;
	buffer.memcpy(header, (uint8_t *)&resp_size, HEADER_SIZE);
}

bool handle_request(Conn *conn) {
	if (conn->incoming.size() < HEADER_SIZE)
		return false; //not ready yet
		
	size_t len = 0;
	//TODO:
	//make custom memcpy in RB to prevent unnecessary O(n) from to_vector_ptr
	memcpy(&len, conn->incoming.data(), HEADER_SIZE); 
	
	if (len > MAX_MSG) {
		std::cerr << "message is too long, len: " << len << "\n";
		conn->want_close = true;
		return false;
	}
	
	size_t packet_len = HEADER_SIZE + len;
	if (packet_len > conn->incoming.size())
		return false; //not ready yet
	
	std::cout << "client says: len: " << len << " request: ";
	
	for (auto& c : conn->incoming) {
		std::cout << c;
	}
	std::cout << std::endl;
	
	auto vector_buffer = conn->incoming.to_vector();
	std::vector<uint8_t> request(vector_buffer.begin() + HEADER_SIZE, vector_buffer.begin() + packet_len);
	std::vector<std::string> cmd;
	if (parse_req(request, len, cmd) < 0) {
		conn->want_close = true;
		return false;
	}
	
	//process the cmd by finding its arg in HashMap
	//create a response, serialize it and add to outgoing buff
	size_t header_pos = 0;
	prepare_for_response(conn->outgoing, &header_pos);
	try {
		do_request(cmd, conn->outgoing); 
	}//TODO clear already inserted parts from output in case of exception
	catch (const std::exception &e) {
		//std::cerr << "reponse: " << e.what() << std::endl;
		append_err(conn->outgoing, RES_TOOLONG, "response is too long");
		return false;
	}
	complete_response(conn->outgoing, header_pos);
	
	conn->append_to_outgoing(packet_len);
	conn->consume_from_incoming(packet_len);
		
	return true;	
}

void handle_write(Conn *conn) {
	assert(conn->outgoing.size() > 0);
	ssize_t rv = send(conn->fd, conn->outgoing.data(), conn->outgoing.size(), 0);
	if (rv < 0) {
		
		if (errno & EAGAIN)
			return; //not ready yet
		
		conn->want_close = true;
		return; //error
	}	
	conn->consume_from_outgoing((size_t)rv);
	
	if (conn->outgoing.size() == 0) {
		conn->want_read = true;
		conn->want_write = false;
	}
}

void handle_read(Conn *conn) {
	std::vector<uint8_t> rbuf(HEADER_SIZE + MAX_MSG + 1);
	ssize_t rv = recv(conn->fd, rbuf.data(), rbuf.size(), 0);
	if (rv <= 0) {
		/* if there was an error but not because of one of these:
		 * an unexpected signal(EINTR)
		 * no data received from the socket or timeout(EAGAIN, EWOULDBLOCK) */
		if (errno && ((errno & (EAGAIN | EWOULDBLOCK | EINTR)) == 0))
			perror("recv()");
		
		conn->want_close = true;
		return;
	}

	//conn->incoming.insert(conn->incoming.end(), rbuf.begin(), rbuf.begin() + (size_t)rv);
	conn->append_to_incoming(rbuf, (size_t)rv);
	//incoming.insert(rbuf.begin(), rbuf.begin() + (size_t)rv);
	
	//for a pipeline
	while (handle_request(conn)) {}
	
	if (conn->outgoing.size() > 0) {
		conn->want_read = false;
		conn->want_write = true;
		
		return handle_write(conn);
	}
}

static void kill_conn(Conn *conn) {
	close(conn->fd);
	fd2conn[conn->fd] = nullptr;
	delete conn;
	conn = nullptr;
}

static int get_next_timer_ms() {
	//Connections timers
	int now_ms = get_monotonic_ms();
	int next_timer_ms = -1;
	
	if (!g_timers.empty())
		next_timer_ms = g_timers.front().first() + CONN_TIMEOUT;
	
	//TTL keys timers
	if (!ttl_heap.empty() && ttl_heap.peek() < next_timer_ms)
		next_timer_ms = ttl_heap.peek();
	
	if (next_timer_ms == -1)
		return -1; //no timeout needed if there's no timers
		
	if (next_timer_ms <= now_ms) {
		//possible missed so no need to timeout
		//will cause poll() to return immediately
		return 0; 
	}
		
	return next_timer_ms - now_ms;
}

//remove all expired timers
static void process_timers() {
	int now_ms = get_monotonic_ms();
	std::cout << "process_timers()\n";
	auto it = g_timers.begin();
	while (it != g_timers.end()) {
		int next_timer_ms = it->first() + CONN_TIMEOUT;
		if (next_timer_ms >= now_ms)
			break; //the rest of the timers are still active
		
		std::cout << "removing connection " << it->second()->fd << "\n";
		kill_conn(it->second()); //destroy timeouted conn
		it = g_timers.erase(it); //erase its timer
		if (it != g_timers.end())
			it++;		
	}
	
	//limit the amount of cheks per iteration 
	//to prevent event-loop from stopping for too long
	const int max_checks = MAX_EVENTLOOP_JOBS_PER_ITERATION;
	int nchecks = 0;
	while (!ttl_heap.empty() && ttl_heap.peek() < now_ms 
											&& nchecks++ < max_checks) {
		Entry *to_remove = ttl_heap.delete_min();
		auto val = g_hmap.erase(to_remove->get_key());
		if (val)
			delete *val;	
	}
	
	std::cout << "exit process_timers()\n";
}

void update_timer(Conn *conn) {
	//erase Timer of the passed conn from g_timers
	g_timers.erase(std::remove(g_timers.begin(), g_timers.end(), *conn->timer), g_timers.end());
	//update conn's timer
	conn->timer->set_time(get_monotonic_ms());
	//and push it to queue
	g_timers.push_back(*conn->timer);
}

int main() {	
	struct addrinfo hints, *res;
	int listenfd, err;
	
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC; //IPv4 or IPv6
	hints.ai_socktype = SOCK_STREAM; //TCP
	hints.ai_flags = AI_PASSIVE; //use the IP of the host
	
	if ((err = getaddrinfo(NULL, PORT, &hints, &res)) != 0) {
		//since getaddrinfo() in case of error produces error code 
		//and not necessary sets errno we need to use here gai_strerror() 
		//which translates the error code instead of die()  
		fprintf(stderr, "getaddrinfo(): %s\n", gai_strerror(err));
		exit(1);
	}
	
	listenfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (listenfd < 0) {
		die("socket()");
	}
	
	/*
	* SO_REUSEADDR:
		Reports whether the rules used in validating addresses supplied to bind() 
		should allow reuse of local addresses, if this is supported by the protocol. 
		This option stores an int value. This is a boolean option. 
	* SOL_SOCKET:
		Specifies that options need to be retrieved at the socket level
	*/
	int yes = 1;
	setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
	
	if ((bind(listenfd, res->ai_addr, res->ai_addrlen)) != 0) {
		die("bind()");
	}
	
	freeaddrinfo(res);
	
	fd_set_nb(listenfd);
	
	//SOMAXCONN: max number of connections that can be queued 
	//in TCP/IP stack backlog per socket
	if ((listen(listenfd, SOMAXCONN)) != 0) {
		die("listen()");
	}
	
	//vector of fds to be examined by poll in event loop
	std::vector <struct pollfd> poll_args;
	
	//the event loop
	while (1) {
		poll_args.clear();
		
		//put the listening socket to poll_args
		struct pollfd plfd = {listenfd, POLLIN, 0};
		poll_args.push_back(plfd);
		
		//put all of the current connections to poll_args
		for (Conn *conn: fd2conn) {
			if (!conn)
				continue;
			
			struct pollfd pfd = {conn->fd, POLLERR, 0};
			if (conn->want_read)
				pfd.events |= POLLIN;
			
			if (conn->want_write)
				pfd.events |= POLLOUT;
			
			poll_args.push_back(pfd);
		}
		
		int timeout_ms = get_next_timer_ms();
		//poll every connection fds + listening socket to get those which are ready
		//set timeout to the closest timer value to give a last chance to it's connection
		int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), timeout_ms);
		
		//if an unexpected signal was received during polling just continue
		//(upon success rv is equal to the number of selected by poll() fds
		//if poll() timeouted before any fd is ready then rv = 0 
		//but in that case we want process_timers(), check if we need to remove conn)
		if ((rv < 0 && errno == EINTR))
			continue;
			
		if (rv < 0)
			die("poll()");
		
		//check if there is any connection request pending in the queue
		//readiness to accept() is treated as a readiness to read() from listenfd
		if (poll_args[0].revents) {
			if (Conn *conn = handle_accept(listenfd)) {
				if (conn->fd >= 0) {
					if (fd2conn.size() <= (size_t)conn->fd) {
						fd2conn.resize(conn->fd + 1);
					}
					//map the new connection with its allocated fd
					assert(!fd2conn[conn->fd]);
					fd2conn[conn->fd] = conn;
				}
			}
		}
		
		//we skip 0th idx since it's listenfd and was  previously checked
		for (size_t i = 1; i < poll_args.size(); i++) {
			short ready_flags = poll_args[i].revents;
			if (ready_flags == 0)
				continue;
				
			Conn *conn_i = fd2conn[poll_args[i].fd];
			//update conn's timer
			update_timer(conn_i);			
			
			if (ready_flags & POLLIN) {
				assert(conn_i->want_read);
				//printf("calling read for %d\n", conn_i->fd);
				handle_read(conn_i);
			}
				
			if (ready_flags & POLLOUT) {
				assert(conn_i->want_write);
				//printf("calling write for %d\n", conn_i->fd);
				handle_write(conn_i);
			}
				
			if ((ready_flags & POLLERR) || (ready_flags & POLLHUP) || conn_i->want_close) {
				//printf("closing  %d\n", conn_i->fd);
				g_timers.erase(std::remove(g_timers.begin(), g_timers.end(), *conn_i->timer), g_timers.end());
				kill_conn(conn_i);				
			}
		}
		
		//handle timers
		process_timers();
		
		/* while (1) {
			int32_t err = one_request(connfd); 
			if (err)
				break;
		}
		
		close(connfd); */
	}
	
	return 0;
}
