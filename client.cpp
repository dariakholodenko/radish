#include "wrapper.h"

static int32_t query(int sockfd, const char *msg) {
	uint32_t msg_len = (uint32_t)strlen(msg);
	if (msg_len > MAX_MSG)
		return -1;
	
	char wbuf[HEADER_SIZE + MAX_MSG];
	memmove(wbuf, &msg_len, HEADER_SIZE);
	memmove(&wbuf[HEADER_SIZE], msg, msg_len);	
	
	int32_t ret;
	if ((ret = write_all(sockfd, wbuf, HEADER_SIZE + msg_len)))
		return ret;
	
	//get server's reply
	char rbuf[HEADER_SIZE + MAX_MSG + 1];
	if ((ret = read_full(sockfd, rbuf, HEADER_SIZE)))
		return ret;
	
	memcpy(&msg_len, rbuf, HEADER_SIZE);
	if (msg_len > MAX_MSG) {
		fprintf(stderr, "message is too long\n");
		return -1;
	}

	if ((ret = read_full(sockfd, &rbuf[HEADER_SIZE], msg_len)))
		return ret;
	
	rbuf[HEADER_SIZE + msg_len] = '\0';
	printf("server says: %s\n", &rbuf[HEADER_SIZE]);
	
	return 0;
}

int main() {
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
	
	err = query(sockfd, "hello1");
	if (err)
		goto DONE;
		
	err = query(sockfd, "hello2");
	if (err)
		goto DONE;
	
	err = query(sockfd, "hello3");
	if (err)
		goto DONE;
		
DONE:
	close(sockfd);
	return 0;
}
