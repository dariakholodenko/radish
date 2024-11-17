#ifndef __SERV_CLIENT_H_
#define __SERV_CLIENT_H_

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

#include <arpa/inet.h>

#define PORT "1234"
#define MAX_MSG 4096
#define HEADER_SIZE 4

void die(const char * error_msg);
int32_t read_full(int fd, char *buf, size_t n);
int32_t write_all(int fd, const char *buf, size_t n);
void *get_in_addr(struct sockaddr *sa);

#endif
