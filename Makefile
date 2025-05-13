#
# To compile, type "make" or make "all"
# To remove files, type "make clean"
#
OBJS = server.o wrapper.o client.o hashmap.o custom_heap.o utest_hash.o utest_skip.o utest_sset.o utest_heap.o
TARGET = server

CC = g++
CFLAGS = -g -std=c++14 -Wall -Wextra -Wfatal-errors
#-p

.SUFFIXES: .cpp .o 

all: server client test

test_hash: utest_hash.o 
	$(CC) $(CFLAGS) -o test_hash utest_hash.o 
#hashmap.o

test_skip: utest_skip.o
	$(CC) $(CFLAGS) -o test_skip utest_skip.o

test_heap: utest_heap.o
	$(CC) $(CFLAGS) -o test_heap utest_heap.o

test: utest_sset.o 
	$(CC) $(CFLAGS) -o test utest_sset.o
	
server: server.o wrapper.o custom_heap.o
	$(CC) $(CFLAGS) -o server server.o wrapper.o custom_heap.o
#hashtable.o

client: client.o wrapper.o 
	$(CC) $(CFLAGS) -o client client.o wrapper.o

.cpp.o:
	$(CC) $(CFLAGS) -o $@ -c $<
	
valgrind:
	valgrind --leak-check=full --show-leak-kinds=all ./test

gprof:
	make clean && make test && ./test && gprof -b test gmon.out > analysis.txt

clean:
	-rm -f $(OBJS) server client test test_hash test_skip test_heap
