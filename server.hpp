#ifndef __SERVER_HPP__
#define __SERVER_HPP__

//c++
#include <errno.h>
#include <vector>


//networking
#include <fcntl.h> //fcntl
#include <netdb.h> //getaddrinfo, freeaddrinfo
#include <poll.h> //poll()
#include <sys/types.h> //getaddrinfo

//custom
#include "conn_manager.hpp"
#include "io_shared_library.hpp" //PORT

class Server {
public:
	void run();
	
private:
	int listen_fd = -1;
	//vector of fds to be examined by poll in event loop
	std::vector <struct pollfd> poll_args;
	ConnectionManager cm;
	
	void fd_set_nb(int fd); //set fd to non-blocking mode(as a file)
	void die(const char * error_msg);
	void setup_listen_fd();
	void prepare_poll_args();
	void process_poll_results(int rv);
};

#endif
