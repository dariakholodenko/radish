#include "wrapper.h"

/** Current protocol:
	+-----+------+-----+------+--------
	| len | msg1 | len | msg2 | more...
	+-----+------+-----+------+--------
**/

// TODO: Try to reimplement with buffer-i/o to prevent unnecessary syscalls
// aka remove double read_full

static int32_t one_request(int connfd) {
	char rbuf[HEADER_SIZE + MAX_MSG + 1];
	int32_t ret = read_full(connfd, rbuf, HEADER_SIZE);
	if (ret) 
		return ret;
	
	uint32_t msg_len = 0;
	memcpy(&msg_len, rbuf, HEADER_SIZE); //little-endian
	
	//request body
	ret = read_full(connfd, &rbuf[HEADER_SIZE], msg_len);
	if (ret)
		return ret;
	
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

int main() {
	struct addrinfo hints, *res;
	int sockfd, err;
	char s[INET6_ADDRSTRLEN];
	
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC; //IPv4 or IPv6
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE; //use the IP of the host
	
	if ((err = getaddrinfo(NULL, PORT, &hints, &res)) != 0) {
		fprintf(stderr, "getaddrinfo(): %s\n", gai_strerror(err));
		exit(1);
	}
	
	sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (sockfd < 0) {
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
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
	
	if ((bind(sockfd, res->ai_addr, res->ai_addrlen)) != 0) {
		die("bind()");
	}
	
	freeaddrinfo(res);
	
	//SOMAXCONN: max number of connections that can be queued 
	//in TCP/IP stack backlog per socket
	if ((listen(sockfd, SOMAXCONN)) != 0) {
		die("listen()");
	}
	
	while (1) {
		struct sockaddr_storage client_addr;
		socklen_t client_len = sizeof(client_addr);
		int connfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);
		if (connfd < 0) 
			continue;
		
		inet_ntop(client_addr.ss_family, 
			get_in_addr((struct sockaddr *)&client_addr), s, sizeof(s));
		
		printf("server: got connection from %s\n", s);
		
		while (1) {
			int32_t err = one_request(connfd); 
			if (err)
				break;
		}
		
		close(connfd);
	}
}
