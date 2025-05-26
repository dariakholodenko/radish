#ifndef __IO_SHARED_HPP_
#define __IO_SHARED_HPP_

//networking
#include <sys/socket.h> //sockaddr
#include <netinet/in.h> //sockaddr_in6

/** This is a header to hold some shared 
 * between a server and a client definitions and functions **/

constexpr const char *PORT = "1234";
constexpr size_t HEADER_SIZE = sizeof(uint32_t);
constexpr size_t MAX_MSG_LEN = 256;

enum Tag : uint8_t {
	TAG_NIL = 0, //nill
	TAG_ERR = 1, //error code + msg
	TAG_STR = 2, //string
	TAG_INT = 3, //int64
	TAG_DBL = 4, //double
	TAG_ARR = 5, //array
};

enum ErrorCode : uint32_t {
	RES_OK,
	RES_CANTREAD, //failed to parse request
	RES_NODATA, //no existing data
	RES_NOCMD, //command doesn't exist
	RES_TOOLONG, //request/response/data is too long
	RES_INVALID, //invalid input
};

/* returns pointer to struct in_addr or in6_addr
 * it's inline to ensure only one definition is used 
 * during compilation across all files */
inline void *get_in_addr(struct sockaddr *sa) {
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in *)sa)->sin_addr);
	}
	
	return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

#endif
