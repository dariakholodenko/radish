#
# To compile, type "make" or make "all"
# To remove files, type "make clean"
#
OBJS = server.o wrapper.o client.o utest_hash.o hashtable.o
TARGET = server

CC = g++
CFLAGS = -g -std=c++14 -Wall -Wextra

.SUFFIXES: .cpp .o 

all: server client test

test: utest_hash.o hashtable.o
	$(CC) $(CFLAGS) -o test utest_hash.o hashtable.o
	
server: server.o wrapper.o hashtable.o
	$(CC) $(CFLAGS) -o server server.o wrapper.o hashtable.o

client: client.o wrapper.o 
	$(CC) $(CFLAGS) -o client client.o wrapper.o

.cpp.o:
	$(CC) $(CFLAGS) -o $@ -c $<
	
valgrind:
	valgrind --leak-check=full --show-leak-kinds=all ./server  

clean:
	-rm -f $(OBJS) server client test
