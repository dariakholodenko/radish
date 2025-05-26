//C
#include <stdio.h>
#include <string.h>
#include <unistd.h> //close()

//c++
#include <iostream>
#include <string>
#include <vector>

//networking
#include <sys/types.h> //getaddrinfo
#include <netdb.h> //getaddrinfo, freeaddrinfo
#include <arpa/inet.h> //inet_ntop

//custom
#include "io_shared_library.hpp" //PORT, HEADER_SIZE, MAX_MSG_LEN, Tag, get_in_addr

constexpr size_t TAG_SIZE = sizeof(Tag);

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

static int32_t read_all(int fd, uint8_t *buf, size_t n) {
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

static int32_t write_all(int fd, uint8_t *buf, size_t n) {
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

static int32_t send_req(int sockfd, const std::vector<std::string> &cmd) {
	//attach len of the whole msg(mlen)
	uint32_t msg_len = HEADER_SIZE; 
	for (const std::string &s: cmd) {
		msg_len += HEADER_SIZE + s.size();
	}
	
	if (msg_len > MAX_MSG_LEN) {
		fprintf(stderr, "message is too long, len: %u, max_msg: %lu\n", msg_len, MAX_MSG_LEN);
		return -1;
	}
		
	uint8_t wbuf[HEADER_SIZE + MAX_MSG_LEN];
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
    if (size < TAG_SIZE) {
        fprintf(stderr, "bad response: size %lu\n", size);
        return -1;
    }
    
    switch (data[0]) {
    case TAG_NIL:
        printf("(nil)\n");
        return TAG_SIZE;
       
    /*error: 
     * |TAG_ERR(uint8_t)|ERROR_CODE(uint32_t)|MSG_LEN(uint32_t)|msg(len)| */
    case TAG_ERR:
        if (size < TAG_SIZE + 2 * HEADER_SIZE) {
            fprintf(stderr, "bad response: size %lu\n", size);
            return -1;
        }
        {
            int32_t code = 0;
            uint32_t len = 0;
            memcpy(&code, &data[TAG_SIZE], sizeof(code));
            memcpy(&len, &data[TAG_SIZE + HEADER_SIZE], sizeof(len));
            if (size < TAG_SIZE + 2 * HEADER_SIZE + len) {
                fprintf(stderr, "bad response: size %lu\n", size);
                return -1;
            }
            printf("(err) %d %.*s\n", code, len, &data[TAG_SIZE + 2 * HEADER_SIZE]);
            return TAG_SIZE + 2 * HEADER_SIZE + len;
        }
    case TAG_STR:
        if (size < TAG_SIZE + HEADER_SIZE) {
            fprintf(stderr, "bad response: size %lu\n", size);
            return -1;
        }
        {
            uint32_t len = 0;
            memcpy(&len, &data[TAG_SIZE], HEADER_SIZE);
            if (size < TAG_SIZE + HEADER_SIZE + len) {
                fprintf(stderr, "bad response: size %lu\n", size);
                return -1;
            }
            printf("(str) %.*s\n", len, &data[TAG_SIZE + HEADER_SIZE]);
            return TAG_SIZE + HEADER_SIZE + len;
        }
    case TAG_INT:
        if (size < TAG_SIZE + sizeof(int32_t)) {
            fprintf(stderr, "bad response: size %lu\n", size);
            return -1;
        }
        {
            int32_t val = 0;
            memcpy(&val, &data[TAG_SIZE], sizeof(int32_t));
            printf("(int) %d\n", val);
            return size;
        }
    case TAG_DBL:
        if (size < TAG_SIZE + sizeof(double)) {
            fprintf(stderr, "bad response: size %lu\n", size);
            return -1;
        }
        {
            double val = 0;
            memcpy(&val, &data[TAG_SIZE], sizeof(val));
            printf("(dbl) %g\n", val);
            return TAG_SIZE + sizeof(double);
        }
    case TAG_ARR:
        if (size < TAG_SIZE + HEADER_SIZE) {
            fprintf(stderr, "bad response: size %lu\n", size);
            return -1;
        }
        {
            uint32_t len = 0;
            memcpy(&len, &data[TAG_SIZE], HEADER_SIZE);
            printf("(arr) len=%u\n", len);
            size_t arr_bytes = TAG_SIZE + HEADER_SIZE;
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
        fprintf(stderr, "bad response: size %lu\n", size);
        return -1;
    }
}

static int32_t recv_resp(int sockfd) {
	//get server's reply
	uint8_t rbuf[HEADER_SIZE + MAX_MSG_LEN + 1];
	//read exactly HEADER_SIZE bytes
	int32_t rv;
	if ((rv = read_all(sockfd, rbuf, HEADER_SIZE)))
		return rv; //failed to read exactly HEADER_SIZE bytes
	
	size_t msg_len = 0;
	memmove(&msg_len, rbuf, HEADER_SIZE);
	if (msg_len > MAX_MSG_LEN) {
		fprintf(stderr, "response is too long, len: %lu, max_msg: %lu\n", msg_len, MAX_MSG_LEN);
		return -1;
	}
	
	//reply's body
	if ((rv = read_all(sockfd, &rbuf[HEADER_SIZE], msg_len)))
		return rv;
	
	//print response
	rv = print_resp((uint8_t *)&rbuf[HEADER_SIZE], msg_len);
	if (rv > 0 && (uint32_t)rv != msg_len) {
		fprintf(stderr, "bad response: rv!=msg_len (%d, %lu)\n", rv, msg_len);
		rv = -1;
	}
	//uint32_t rc = 0;
	//memmove(&rc, &rbuf[HEADER_SIZE], HEADER_SIZE);
	//rbuf[HEADER_SIZE + msg_len] = '\0';
	//printf("server says: [%u] %s\n", rc, &rbuf[2 * HEADER_SIZE]);
	
	return rv;
}

int main(int argc, char **argv) {
	struct addrinfo hints{}, *servinfo, *adi;
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
		exit(1);
	}
	
	//convert IPv4 and IPv6 addresses from binary to text form
	inet_ntop(adi->ai_family, get_in_addr((struct sockaddr *)adi->ai_addr),
            s, sizeof(s));
    printf("client: connecting to %s\n", s);
    
	freeaddrinfo(servinfo);
	
	std::vector<std::string> cmd(argv + 1, argv + argc);	
	if (send_req(sockfd, cmd) || recv_resp(sockfd)) {
		close(sockfd);
		exit(1);
	}
	
	close(sockfd);
	return 0;
}
