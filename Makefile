#
# To compile, type "make" or make "all"
# To remove files, type "make clean"
#
OBJS = server.o wrapper.o client.o
TARGET = server

CC = g++
CFLAGS = -g -Wall -Wextra

.SUFFIXES: .c .o 

all: server client

server: server.o wrapper.o
	$(CC) $(CFLAGS) -o server server.o wrapper.o

client: client.o wrapper.o
	$(CC) $(CFLAGS) -o client client.o wrapper.o

.c.o:
	$(CC) $(CFLAGS) -o $@ -c $<

clean:
	-rm -f $(OBJS) server client
