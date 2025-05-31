#
# To compile, type "make" or make "all"
# To remove files, type "make clean"
#
OBJS = client.o utest_sset.o
OBJS_SERVER = main.o custom_heap.o server.o conn_manager.o protocol.o commands.o sortedset.o ttl_manager.o
OBJS_TEST = utest_hash.o utest_skip.o utest_sset.o utest_heap.o
BINS = server client main test test_hash test_skip test_heap

TARGET = main

CC = g++
CFLAGS = -g -std=c++17 -Wall -Wextra -Wfatal-errors
#-p

.SUFFIXES: .cpp .o 

all: main client

test_hash: utest_hash.o 
	$(CC) $(CFLAGS) -o test_hash utest_hash.o 

test_skip: utest_skip.o
	$(CC) $(CFLAGS) -o test_skip utest_skip.o

test_heap: utest_heap.o
	$(CC) $(CFLAGS) -o test_heap utest_heap.o

test: utest_sset.o sortedset.o
	$(CC) $(CFLAGS) -o test utest_sset.o sortedset.o
	
server: server.o wrapper.o custom_heap.o
	$(CC) $(CFLAGS) -o server server.o wrapper.o custom_heap.o

main: $(OBJS_SERVER)
	$(CC) $(CFLAGS) -o main $(OBJS_SERVER)

client: client.o 
	$(CC) $(CFLAGS) -o client client.o 

.cpp.o:
	$(CC) $(CFLAGS) -o $@ -c $<
	
valgrind:
	valgrind --leak-check=full --show-leak-kinds=all ./test

gprof:
	make clean && make test && ./test && gprof -b test gmon.out > analysis.txt

clean:
	-rm -f $(OBJS) $(OBJS_SERVER) $(OBJS_TEST) $(BINS)
