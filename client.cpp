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

static int32_t print_resp(const uint8_t *data, size_t size) {
    if (size < 1) {
        fprintf(stderr, "bad response\n");
        return -1;
    }
    
    switch (data[0]) {
    case TAG_NIL:
        printf("(nil)\n");
        return 1;
        
    case TAG_ERR:
        if (size < 1 + 2 * HEADER_SIZE) {
            fprintf(stderr, "bad response\n");
            return -1;
        }
        {
            int32_t code = 0;
            uint32_t len = 0;
            memcpy(&code, &data[1], HEADER_SIZE);
            memcpy(&len, &data[1 + HEADER_SIZE], HEADER_SIZE);
            if (size < 1 + 2 * HEADER_SIZE + len) {
                fprintf(stderr, "bad response\n");
                return -1;
            }
            printf("(err) %d %.*s\n", code, len, &data[1 + 2 * HEADER_SIZE]);
            return 1 + 8 + len;
        }
    case TAG_STR:
        if (size < 1 + HEADER_SIZE) {
            fprintf(stderr, "bad response\n");
            return -1;
        }
        {
            uint32_t len = 0;
            memcpy(&len, &data[1], 4);
            if (size < 1 + 4 + len) {
                fprintf(stderr, "bad response\n");
                return -1;
            }
            printf("(str) %.*s\n", len, &data[1 + 4]);
            return 1 + 4 + len;
        }
    case TAG_INT:
        if (size < 1 + 8) {
            fprintf(stderr, "bad response\n");
            return -1;
        }
        {
            int64_t val = 0;
            memcpy(&val, &data[1], 8);
            printf("(int) %ld\n", val);
            return 1 + 8;
        }
    case TAG_DBL:
        if (size < 1 + 8) {
            fprintf(stderr, "bad response\n");
            return -1;
        }
        {
            double val = 0;
            memcpy(&val, &data[1], 8);
            printf("(dbl) %g\n", val);
            return 1 + 8;
        }
    case TAG_ARR:
        if (size < 1 + 4) {
            fprintf(stderr, "bad response\n");
            return -1;
        }
        {
            uint32_t len = 0;
            memcpy(&len, &data[1], 4);
            printf("(arr) len=%u\n", len);
            size_t arr_bytes = 1 + 4;
            for (uint32_t i = 0; i < len; ++i) {
                int32_t rv = print_resp(&data[arr_bytes], size - arr_bytes);
                if (rv < 0) {
                    return rv;
                }
                arr_bytes += (size_t)rv;
            }
            printf("(arr) end\n");
            return (int32_t)arr_bytes;
        }
    default:
        fprintf(stderr, "bad response\n");
        return -1;
    }
}

int recv_resp(int sockfd) {
	//get server's reply
	uint8_t rbuf[HEADER_SIZE + MAX_MSG + 1];
	//reply's header
	int32_t rv;
	if ((rv = read_all(sockfd, rbuf, HEADER_SIZE)))
		return rv;
	
	size_t msg_len = 0;
	memmove(&msg_len, rbuf, HEADER_SIZE);
	if (msg_len > MAX_MSG) {
		fprintf(stderr, "message is too long, len: %lu, max_msg: %u\n", msg_len, MAX_MSG);
		return -1;
	}
	
	//reply's body
	if ((rv = read_all(sockfd, &rbuf[HEADER_SIZE], msg_len)))
		return rv;

	if (msg_len < HEADER_SIZE) {
		fprintf(stderr, "bad response\n");
		return -1;
	}
	
	//print response
	rv = print_resp((uint8_t *)&rbuf[HEADER_SIZE], msg_len);
	if (rv > 0 && (uint32_t)rv != msg_len) {
		fprintf(stderr, "bad response\n");
		rv = -1;
	}
	//uint32_t rc = 0;
	//memmove(&rc, &rbuf[HEADER_SIZE], HEADER_SIZE);
	//rbuf[HEADER_SIZE + msg_len] = '\0';
	//printf("server says: [%u] %s\n", rc, &rbuf[2 * HEADER_SIZE]);
	
	return rv;
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
