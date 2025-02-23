#include "wrapper.hpp"

#define DEBUG_R 0 //debug for read-related functions
#define DEBUG_W 0 //debug for write-related functions

using namespace hm;
//TODO:
//think of a different approach without a global variable use
static HMap g_htable(HMAP_BASE_CAPACITY);

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

struct Conn {
	int fd;
	//app's intention for the event loop
	bool want_read;
	bool want_write;
	bool want_close;
	
	//buffered input and output
	RingBuffer<uint8_t> incoming; //request to be parsed from the app
	RingBuffer<uint8_t> outgoing; //response for the app	
	
	Conn(int fd) : fd(fd), want_read(false), 
					want_write(false), want_close(false),
					incoming(BUFF_CAPACITY), outgoing(BUFF_CAPACITY) {}
	
	void append_to_outgoing(size_t len) {
		outgoing.insert(incoming.begin(), incoming.begin() + len);
	}
	
	void append_to_incoming(std::vector<uint8_t>& buff, size_t len) {
		incoming.insert(buff.begin(), buff.begin() + len);
	}
	
	void consume_from_incoming(size_t len) {
		incoming.erase_front(len);
	}
	
	void consume_from_outgoing(size_t len) {
		outgoing.erase_front(len);
	}
};

enum Status : uint32_t {
	RES_OK = 0,
	RES_NODATA = 1, //no existing data
	RES_NOCMD = 2 //command doesn't exist
};

struct Response {
	Status status = RES_OK;
	std::vector<uint8_t> data;
};

// TODO: Try to reimplement with buffer-i/o to prevent unnecessary syscalls
// aka remove double read_all
/*
int32_t one_request(int connfd) {
	char rbuf[HEADER_SIZE + MAX_MSG + 1];
	int32_t rv = read_all(connfd, rbuf, HEADER_SIZE);
	if (rv) 
		return rv;
	
	uint32_t msg_len = 0;
	memcpy(&msg_len, rbuf, HEADER_SIZE); //little-endian
	
	//request body
	rv = read_all(connfd, &rbuf[HEADER_SIZE], msg_len);
	if (rv)
		return rv;
	
	//print client's message
	rbuf[HEADER_SIZE + msg_len] = '\0';
	printf("client says: %s\n", &rbuf[HEADER_SIZE]);
	
	//reply
	char* reply = "world";
	char wbuf[HEADER_SIZE + sizeof(reply)];
	msg_len = (uint32_t)strlen(reply);
	memcpy(wbuf, &msg_len, HEADER_SIZE);
	memcpy(&wbuf[HEADER_SIZE], reply, msg_len);
	
	return write_all(connfd, wbuf, HEADER_SIZE + msg_len);
}
*/

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
	
	//set the new connfd to non-blocking mode
	fd_set_nb(connfd);
	
	Conn *new_conn = new Conn(connfd);
	new_conn->want_read = true; // want to read first request
	
	inet_ntop(client_addr.ss_family, 
		get_in_addr((struct sockaddr *)&client_addr), ip, sizeof(ip));
	
	printf("server: got connection from %s\n", ip);
	
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

static void do_request(std::vector<std::string> &cmd, Response &response) {
	if (cmd.size() == 2 && cmd[0] == "get") {
		/*auto it = g_htable.find(cmd[1]);
		if (it != g_htable.end()) {
			auto val = it->second;
			response.data.assign(val.begin(), val.end());
		}*/
		auto it = g_htable.find(cmd[1]);
		if (it) {
			auto val = it->get_value();
			response.data.assign(val.begin(), val.end());
		}
		else {
			response.status = RES_NODATA;
		}
	}
	else if (cmd.size() == 3 && cmd[0] == "set") {
		//g_htable[cmd[1]] = cmd[2];
		HNode *node = new HNode(cmd[1], cmd[2]);
		g_htable.insert(node);
	}
	else if (cmd.size() == 2 && cmd[0] == "del") {
		HNode *node = g_htable.erase(cmd[1]);
		if (node)
			delete node;
	}
	else {
		response.status = RES_NOCMD; 
	}
}

static void make_response(Response &response, Conn *conn) {
	uint32_t resp_len = HEADER_SIZE + (uint32_t)response.data.size();
	conn->outgoing.insert((const uint8_t *)&resp_len, HEADER_SIZE);
	conn->outgoing.insert((const uint8_t *)&response.status, HEADER_SIZE);
	conn->outgoing.insert(response.data.begin(), response.data.end());
}

bool handle_request(Conn *conn) {
	if (conn->incoming.size() < HEADER_SIZE)
		return false; //not ready yet
		
	size_t len = 0;
	//TODO:
	//make custom memcpy in RB to prevent unnecessary O(n) from to_vector_ptr
	memcpy(&len, conn->incoming.data(), HEADER_SIZE); 
	#if DEBUG_R
	std::cout << "\nin handle_req: v size " << conn->incoming.size() << " len is " << len << "\n";
	#endif
	
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
	
	//process the cmd by finding its arg in HTable and create a response
	Response resp;
	do_request(cmd, resp); 
	//serialize a response and add to outgoing buff
	make_response(resp, conn); 
	
	conn->append_to_outgoing(packet_len);
	conn->consume_from_incoming(packet_len);
	
	#if DEBUG_R
	std::cout << "after read: input buff len " << conn->incoming.size() << " data ";
	for (auto& c : conn->incoming) {
		std::cout << c;
	}
	std::cout << std::endl;
	#endif
	
	#if DEBUG_W
	std::cout << "out buff len " << conn->outgoing.size() << " data ";
	for (auto& c : conn->outgoing) {
		std::cout << c;
	}
	std::cout << std::endl;
	#endif
	
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
	
	#if DEBUG_W
	std::cout << "\nin write out buff ";
	for (auto& c : conn->outgoing) {
		std::cout << c;
	}
	
	std::cout << "\nsend buff ";
	for (int i = 0; i < conn->outgoing.size(); i++) {
		std::cout << (conn->outgoing.data())[i];
	}
	std::cout << std::endl;
	//std::cout << "in handle_write before comsume : output len is " << conn->outgoing.size() << " to consume " << (size_t)rv << "\n";
	#endif
	
	conn->consume_from_outgoing((size_t)rv);
	
	#if DEBUG_W
	//std::cout << "in handle_write: after write output len is " << conn->outgoing.size() << "\n";
	#endif
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
	#if DEBUG_R
	std::cout << "after read:\n" << conn->incoming;
	#endif
	//for a pipeline
	while (handle_request(conn)) {}
	
	if (conn->outgoing.size() > 0) {
		conn->want_read = false;
		conn->want_write = true;
		
		return handle_write(conn);
	}
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
	
	//map all client connection to fds, used as keys, to save the state for event loop
	//vector is used since for every new fd (used as a key)
	//the smallest possible number is allocated and keys won't be overlapping 
	//and too widely distributed
	std::vector <Conn *> fd2conn;
	
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
		
		//poll every connection fds + listening socket to get those which are ready
		int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), -1);
		//if an unexpected signal was received during polling
		//or there's no fd ready just continue
		//(upon success rv is equal to the number of selectedd by poll() fds)
		if ((rv < 0 && errno == EINTR) || (rv == 0)) 
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
				close(conn_i->fd);
				fd2conn[conn_i->fd] = NULL;
				delete conn_i;
			}
		}
		
		/* while (1) {
			int32_t err = one_request(connfd); 
			if (err)
				break;
		}
		
		close(connfd); */
	}
	
	return 0;
}
