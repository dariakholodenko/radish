#ifndef __SERV_CLIENT_H_
#define __SERV_CLIENT_H_

//C
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h> //clock_gettime() for better performance and poll() compatibility
#include <unistd.h>

//c++
#include <algorithm> //remove()
#include <cassert> //assert()
#include <iostream>
#include <iterator> 
#include <memory> //unique_ptr
#include <stdexcept> //out_of_range 
#include <string>
#include <vector>

//networking
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#include <netdb.h>
#include <netinet/in.h>
#include <fcntl.h>

#include <arpa/inet.h>

//custom libraries
#include "hashmap.hpp"
#include "sortedset.hpp"

#define PORT "1234"
#define MAX_MSG 32
#define HEADER_SIZE 4
#define BUFF_CAPACITY 2*(HEADER_SIZE + MAX_MSG)
#define HMAP_BASE_CAPACITY 128
#define CONN_TIMEOUT 5000 //5000 ms
#define IO_TIMEOUT 500
#define MAX_EVENTLOOP_JOBS_PER_ITERATION 2000

enum Tag : uint8_t {
	TAG_NIL = 0, //nill
	TAG_ERR = 1, //error code + msg
	TAG_STR = 2, //string
	TAG_INT = 3, //int64
	TAG_DBL = 4, //double
	TAG_ARR = 5, //array
};

enum Status : uint32_t {
	RES_OK = 0,
	RES_NODATA = 1, //no existing data
	RES_NOCMD = 2, //command doesn't exist
	RES_TOOLONG = 3, //request/response is too long
};

enum TTL {
	NOTTL = -1,
	EXPIRED = -2,
	OK = 1,
};

template <typename T>
class RingBuffer {
private:
	std::vector<T> buffer;
	std::unique_ptr<std::vector<T>> buffer_uptr;
	int head;
	int tail;
	size_t capacity;
	bool is_empty;
	
	class iterator {
	private:
		RingBuffer<T>& container;
		size_t id;
	
	public:
		iterator(RingBuffer<T>& container, size_t id) : 
					container(container), id(id % container.capacity) {}
					
		T& operator*() const {
			return container[id];
		}
		
		T* operator->() const {
			return &container[id];
		}
		
		//increment by n
		iterator& operator+(int n) {
			id = (id + n) % container.capacity;
			return *this;
		}
		
		//prefix increment
		iterator& operator++() {
			id = (id + 1) % container.capacity;
			return *this;
		}
		
		//postfix increment
		iterator operator++(int) {
			iterator temp = *this;
			++(*this);
			return temp;
		}
		
		bool operator!=(const iterator &right) const {
			return id != (right.id % container.capacity);
		}
		
		bool operator==(const iterator &right) const {
			return id == (right.id % container.capacity);
		}
	};
	
public: 
	RingBuffer(size_t capacity) : head(0), tail(0), 
								capacity(capacity), is_empty(true) {
		buffer.resize(capacity);
		buffer_uptr = std::make_unique<std::vector<T>>();
	}	
	
	void push_back(T element) {
		buffer[tail] = element;
		tail = (tail + 1) % capacity;
		is_empty = false;
		
		if (full()) { //head == tail
			//TODO change this
			//throw std::out_of_range("buffer is full");
			head = (head + 1) % capacity;
		}
	}
	
	//lazy remove from circular buffer 
	//by moving head one element forward to give space to tail
	T& pop_front() {
		if (empty()) 
			throw std::out_of_range("buffer is empty");
		
		T& rv = buffer[head];	
		head = (head + 1) % capacity;
		
		if (head == tail)
			is_empty = true;
		
		return rv;
	}
	
	bool empty() const {
		return (head == tail) && is_empty;
	}
	
	bool full() const {
		return (tail == head) && !is_empty;
	}
	
	size_t size() const {
		return (head <= tail) ? tail - head : capacity - (head - tail);
	}
	
	T& operator[](int id) {
		return buffer[id % capacity];
	}
	
	const T& operator[](int id) const {
		return buffer[id % capacity];
	}
	
	iterator begin() {
		return iterator(*this, head);
	}
		
	iterator end() {
		return iterator(*this, tail);
	}
	
	//append to buffer elements from a given ring buffer slice
	void insert(const RingBuffer<T>::iterator &begin, const RingBuffer<T>::iterator &end) {
		for(auto it = begin; it != end; it++) {
			push_back(*it);
		}
	}
	
	//append to buffer elements from a given array slice
	void insert(const T *arr, size_t len) {
		if (!arr)
			throw std::out_of_range("RingBuffer::append_array: NULL passed");
		
		if (len > capacity - this->size())
			throw std::out_of_range("RingBuffer::append_array: array is too big");
		
		for(size_t i = 0; i < len; i++) {
			push_back(arr[i]);
		}
	}
	
	//append to buffer elements from a given vector slice
	void insert(const typename std::vector<T>::iterator& it_begin, 
					const typename std::vector<T>::iterator& it_end) {
		if ((size_t)std::distance(it_begin, it_end) > capacity - this->size())
			throw std::out_of_range("RingBuffer::append_vector: vector is too big");
	
		for (auto it = it_begin; it != it_end; it++) {
			push_back(*it);
		}
	}
	
	void erase_front(size_t len) {
		if (size() < len)
			throw std::out_of_range("RingBuffer::erase_front: erase length is larger than the buffer size");
		
		for (size_t i = 0; i < len; i++) {
			pop_front();
		}
	}
	
	//transform circular buffer to vector and return a pointer to it
	std::vector<T> to_vector() {		
		std::vector<T> vector_buffer;
		for (int id = head; id != tail; id = (id + 1) % capacity) {
			vector_buffer.push_back(buffer[id]);
		}
		
		return vector_buffer;
	}
	
	//"override" to the vector's method data()
	T* data() {
		buffer_uptr->clear();
		for (int i = head; i != tail; i = (i + 1) % capacity) {
			buffer_uptr->push_back(buffer[i]);
		}
		/*if (array_buffer.size() == 0 || tail < head) { //straighten the buffer to array
			array_buffer.clear();
			
			for (int id = head; id != tail; id = (id + 1) % capacity) {
				array_buffer.push_back(buffer[id]);
			}
			
			return array_buffer.data();
		}
		//if head < tail there's no need to convert ringBuff to arr 
		//since it still behaves like one
		return buffer.data() + head;
		}*/
		return buffer_uptr->data();
	}
	
	void memcpy(size_t start, T *src, size_t len) {
		if (!src)
			throw std::out_of_range("RingBuffer::memmove: NULL passed");
		
		if (len > capacity - this->size())
			throw std::out_of_range("RingBuffer::memmove: passed arg is too big");
			
		auto it = iterator(*this, start);
		
		for (size_t i = 0; i < len; i++) {
			*it = *src;
			it++;
			src++;
		}
	}
	
	friend std::ostream& operator<<(std::ostream& out, const RingBuffer<T> &buffer) {
		if (buffer.empty()) {
			out << "\nbuffer is empty";
		}
		
		for (int i = buffer.head; i != buffer.tail; i = (i + 1) % buffer.capacity) {
			out << buffer[i] << " ";
		}
		
		out << "\n";
		
		return out;
	}
};
template <typename T>
void append_helper(RingBuffer<uint8_t> &buffer, T val, size_t size) {
	buffer.insert((const uint8_t *)&val, size);
}

struct Timer;

struct Conn {
	int fd;
	//app's intention for the event loop
	bool want_read;
	bool want_write;
	bool want_close;
	
	//buffered input and output
	RingBuffer<uint8_t> incoming; //request to be parsed from the app
	RingBuffer<uint8_t> outgoing; //response for the app
	
	//timer
	Timer *timer;
	
	Conn(int fd) : fd(fd), want_read(false), 
			want_write(false), want_close(false),
			incoming(BUFF_CAPACITY), outgoing(BUFF_CAPACITY), 
			timer(nullptr) {}
	
	void append_to_outgoing(size_t len) {
		outgoing.insert(incoming.begin(), incoming.begin() + len);
	}
	
	void append_to_incoming(std::vector<uint8_t>& buff, size_t len) {
		incoming.insert(buff.begin(), buff.begin() + len);
	}
	
	void consume_from_incoming(size_t len) {
		incoming.erase_front(len);
	}
	
	void consume_from_outgoing(size_t len) {
		outgoing.erase_front(len);
	}
};

struct Timer {
	int time;
	Conn *conn;
	
	Timer(Conn *conn) : conn(conn) {
		struct timespec tv = {0, 0};
		clock_gettime(CLOCK_MONOTONIC, &tv);
		time = int(tv.tv_sec) * 1000 + tv.tv_nsec / 1000 / 1000;
	}
	
	int first() {
		return time;
	}
	
	void set_time(int new_time) {
		time = new_time;
	}
	
	Conn *second() {
		return conn;
	}
	
	bool operator==(const Timer &second) const {
		return (time == second.time) && (conn == second.conn);
	}
	
	bool operator!=(const Timer &second) const {
		return (time != second.time) || (conn != second.conn);
	}
};

int get_monotonic_ms();

struct Entry {
	std::string key;
	std::string value;
	int heap_idx;
	int ttl;
	int expire_at;
	
	Entry(const std::string &key, const std::string &value, 
		int idx = -1, int ttl = -1, int expire_at = -1) : 
								key(key), value(value), heap_idx(idx), 
								ttl(ttl), expire_at(expire_at) {}
	
	std::string &get_key() {
		return key;
	}
	
	void set_heap_idx(int idx) {
		heap_idx = idx;
	}
	
	int get_heap_idx() const {
		return heap_idx;
	}
	
	void set_ttl(int new_ttl, int new_expire_at) {
		ttl = new_ttl;
		expire_at = new_expire_at;
	}
	
	int get_ttl() {
		int now = get_monotonic_ms();
		if (expire_at < now) {
			ttl = EXPIRED;
		}
		else {
			ttl = (expire_at - now) / 1000;
		}
		return ttl;
	}
		
	bool operator==(const Entry &second) const {
		return value == second.value;
	} 
	
	bool operator!=(const Entry &second) const {
		return value != second.value;
	} 

};

void die(const char * error_msg);
int32_t read_all(int fd, uint8_t *buf, size_t n);
int32_t write_all(int fd, uint8_t *buf, size_t n);
void *get_in_addr(struct sockaddr *sa);

#endif
