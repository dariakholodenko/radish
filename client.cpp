#include "wrapper.hpp"

/** Current protocol:
	general msg:
		+-------+------+-------+------+------+
		| mlen1 | msg1 | mlen2 | msg2 | ... |
		+-------+------+-------+------+------+
	
	inner msg format for req:
		+------+-------+------+-------+-----+-------+------+
		| nstr | slen1 | str1 | slen2 | ... | slenn | strn |
		+------+-------+------+-------+-----+-------+------+
	
	inner msg format for resp:
		+--------+------+
		| status | data |
		+--------+------+
**/

int send_req(int sockfd, const std::vector<std::string> &cmd) {
	//attach len of the whole msg(mlen)
	uint32_t msg_len = HEADER_SIZE; 
	for (const std::string &s: cmd) {
		msg_len += HEADER_SIZE + s.size();
	}
	
	if (msg_len > MAX_MSG)
		return -1;
		
	uint8_t wbuf[HEADER_SIZE + MAX_MSG];
	memmove(wbuf, &msg_len, HEADER_SIZE); 
	
	//attach number of strings in cmd(nstr)
	uint32_t nstr = cmd.size();
	memmove(&wbuf[HEADER_SIZE], &nstr, HEADER_SIZE);	
	
	//attach len of strings and strings themselves(slen + str)
	size_t cur = 2 * HEADER_SIZE;
	for (const std::string &s: cmd) {
		uint32_t slen = (uint32_t)s.size();
		memmove(&wbuf[cur], &slen, HEADER_SIZE);
		memmove(&wbuf[cur + HEADER_SIZE], s.data(), s.size());
		cur += HEADER_SIZE + s.size();
	}
	
	return write_all(sockfd, wbuf, HEADER_SIZE + msg_len);
}

int recv_resp(int sockfd) {
	//get server's reply
	uint8_t rbuf[HEADER_SIZE + MAX_MSG + 1];
	int rv;
	if ((rv = read_all(sockfd, rbuf, HEADER_SIZE)))
		return rv;
	
	size_t msg_len = 0;
	memmove(&msg_len, rbuf, HEADER_SIZE);
	if (msg_len > MAX_MSG) {
		fprintf(stderr, "message is too long, len: %lu, max_msg: %u\n", msg_len, MAX_MSG);
		return -1;
	}

	if ((rv = read_all(sockfd, &rbuf[HEADER_SIZE], msg_len)))
		return rv;
	
	uint32_t rc = 0;
	if (msg_len < HEADER_SIZE) {
		fprintf(stderr, "bad response\n");
		return -1;
	}
	
	memmove(&rc, &rbuf[HEADER_SIZE], HEADER_SIZE);
	rbuf[HEADER_SIZE + msg_len] = '\0';
	printf("server says: [%u] %s\n", rc, &rbuf[2 * HEADER_SIZE]);
	
	return 0;
}

int main(int argc, char **argv) {
	struct addrinfo hints, *servinfo, *adi;
	int sockfd, err;
	char s[INET6_ADDRSTRLEN];
	
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	
	if ((err = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo(): %s\n", gai_strerror(err));
		exit(1);
	}
	
	for (adi = servinfo; adi != NULL; adi = adi->ai_next) {
		if ((sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, 
										servinfo->ai_protocol)) < 0) {
			perror("client: socket()");
			continue;
		}
	
		if ((err = connect(sockfd, servinfo->ai_addr, 
										servinfo->ai_addrlen)) != 0) {
			perror("client: connect()");
			continue;
		}
		
		break;
	}
	
	if (adi == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 1;
	}
	
	inet_ntop(adi->ai_family, get_in_addr((struct sockaddr *)adi->ai_addr),
            s, sizeof(s));
    printf("client: connecting to %s\n", s);
    
	freeaddrinfo(servinfo);
	
	std::vector<std::string> cmd;
	for (int i = 1; i < argc; i++) {
		cmd.push_back(argv[i]);
	}
	
	err = send_req(sockfd, cmd);
	if (err)
		goto DONE;
	
	err = recv_resp(sockfd);
	if (err)
		goto DONE;

DONE:
	close(sockfd);
	return 0;
}
