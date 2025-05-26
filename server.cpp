#include "server.hpp"

//private:
void Server::fd_set_nb(int fd) {
	//get the file access mode and the file status flags
	int f_flags = fcntl(fd, F_GETFL, 0);
	//add to the alredy set flags O_NONBLOCK
	if (fcntl(fd, F_SETFL,  f_flags | O_NONBLOCK) == -1) {
		die("fcntl()");
	}
}

void Server::die(const char * error_msg) {
	perror(error_msg);
	exit(1);
}

void Server::setup_listen_fd() {
	struct addrinfo hints{}, *res;
	int err;
	
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC; //IPv4 or IPv6
	hints.ai_socktype = SOCK_STREAM; //TCP
	hints.ai_flags = AI_PASSIVE; //use the IP of the host
	
	if ((err = getaddrinfo(NULL, PORT, &hints, &res)) != 0) {
		//since getaddrinfo() in case of error produces error code 
		//and not necessary sets errno we need to use here gai_strerror() 
		//which translates the error code instead of die()  
		std::cerr << "getaddrinfo(): " << gai_strerror(err) << "\n";
		exit(1);
	}
	
	listen_fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (listen_fd < 0) {
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
	setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
	
	if ((bind(listen_fd, res->ai_addr, res->ai_addrlen)) != 0) {
		die("bind()");
	}
	
	freeaddrinfo(res);
	
	fd_set_nb(listen_fd);
	
	//SOMAXCONN: max number of connections that can be queued 
	//in TCP/IP stack backlog per socket
	if ((listen(listen_fd, SOMAXCONN)) != 0) {
		die("listen()");
	}
}

void Server::prepare_poll_args() {
	poll_args.clear();
		
	//put the listening socket to poll_args
	struct pollfd plfd = {listen_fd, POLLIN, 0};
	poll_args.push_back(plfd);
	
	//put all of the current connections to poll_args
	//and set a requested action to poll() for each one
	for (int conn_fd: cm.get_all_connections()) {
		struct pollfd pfd = {conn_fd, POLLERR, 0};
		if (cm.is_readable(conn_fd))
			pfd.events |= POLLIN;
		
		if (cm.is_writable(conn_fd))
			pfd.events |= POLLOUT;
		
		poll_args.push_back(pfd);
	}
}

void Server::process_poll_results(int rv) {
	if (rv <= 0) {
		//either there was a timeout and then nothing to process(=0)
		//or an error occured which was handled(<0)
		return;
	}
	
	//check if there is any connection request pending in the queue
	//readiness to accept() is treated as a readiness to read() from listen_fd
	if (poll_args[0].revents) {
		int client_fd;
		if ((client_fd = cm.handle_accept(listen_fd)) >= 0) {
			//set the new client_fd to non-blocking mode
			fd_set_nb(client_fd);
		}
	}
	
	//we skip 0th idx since it's listen_fd and was  previously checked
	for (size_t i = 1; i < poll_args.size(); i++) {
		short ready_flags = poll_args[i].revents;
		if (ready_flags == 0)
			continue;
		
		int conn_fd = poll_args[i].fd;
		cm.update_timer(conn_fd);
		
		//TODO add exception handling
		if (ready_flags & POLLIN) {
			cm.handle_read(conn_fd);
		}
			
		if (ready_flags & POLLOUT) {
			cm.handle_write(conn_fd);
		}
			
		if ((ready_flags & POLLERR) || (ready_flags & POLLHUP) 
									|| cm.is_closing(conn_fd)) {
			cm.close_conn(conn_fd);
		}
	}
}

//public
void Server::run() {
	setup_listen_fd();
	
	//the event loop
	while (true) {
		prepare_poll_args();
		
		int timeout_ms = cm.get_next_timer();
		//poll every connection fds + listening socket to get those which are ready
		//set timeout to the closest timer value to give a last chance to it's connection
		int rv = poll(poll_args.data(), (nfds_t)poll_args.size(), timeout_ms);
						
		if (rv < 0) {
			//if an unexpected signal was received during polling just continue
			//(upon success rv is equal to the number of selected by poll() fds
			//if poll() timeouted before any fd is ready then rv = 0 
			//but in that case we want process_timers(), check if we need to remove conn)
			if (errno == EINTR)
				continue;
				
			die("poll()");
		}
		
		process_poll_results(rv);
		
		//check if anything has timeouted
		cm.check_timers();
	}
}
