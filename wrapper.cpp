#include "wrapper.hpp"
	
void die(const char * error_msg) {
	perror(error_msg);
	exit(1);
}

//returns pointer to struct in_addr or in6_addr
void *get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in *)sa)->sin_addr);
	}
	
	return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

int32_t read_all(int fd, uint8_t *buf, size_t n) {
	while (n > 0) {
		ssize_t rv = recv(fd, buf, n, 0);
		if (rv <= 0) {
			if (errno)
				perror("recv()");
			else
				fprintf(stderr, "recv(): unexpected early EOF\n");
			return -1;
		}
		
		if ((size_t)rv > n) {
			fprintf(stderr, "recv() overflow\n");
			return -1;
		}
		
		n -= (size_t)rv;
		buf += rv;
	}
	
	return 0;
}

int32_t write_all(int fd, uint8_t *buf, size_t n) {
	while (n > 0) {
		ssize_t rv = send(fd, buf, n, 0);
		if (rv <= 0) {
			if (errno)
				perror("send()");
			else
				fprintf(stderr, "send(): unexpected early EOF\n");
			return -1;
		}
		
		if ((size_t)rv > n) {
			fprintf(stderr, "send() overflow\n");
			return -1;
		}
		n -= (size_t)rv;
		buf += rv;
	}
	
	return 0;
}
