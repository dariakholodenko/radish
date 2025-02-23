#ifndef __SERV_CLIENT_H_
#define __SERV_CLIENT_H_

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

//c++
#include <iostream>
#include <vector>
#include <string>
#include <cassert> 
#include <stdexcept> //out_of_range 
#include <iterator> 
#include <memory> //unique_ptr
#include <unordered_map>

//networking
#include <sys/types.h>
#include <sys/socket.h>
#include <poll.h>
#include <netdb.h>
#include <netinet/in.h>
#include <fcntl.h>

#include <arpa/inet.h>

//custom libraries
#include "hashtable.hpp"

#define PORT "1234"
#define MAX_MSG 32
#define HEADER_SIZE 4
#define BUFF_CAPACITY 2*(HEADER_SIZE + MAX_MSG)
#define HMAP_BASE_CAPACITY 128

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
	
	friend std::ostream& operator<<(std::ostream& out, const RingBuffer<T> buffer) {
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

void die(const char * error_msg);
int32_t read_all(int fd, uint8_t *buf, size_t n);
int32_t write_all(int fd, uint8_t *buf, size_t n);
void *get_in_addr(struct sockaddr *sa);

#endif
