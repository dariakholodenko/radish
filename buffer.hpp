#ifndef __BUFFER_HPP_
#define __BUFFER_HPP_

//c++
#include <iostream> 
#include <stdexcept> //out_of_range
#include <memory> //std::unique_ptr
#include <vector>

//custom
#include "io_shared_library.hpp" //Tag and ErrorCode definitions

template <typename T>
class RingBuffer;

template <typename T>
void append_helper(RingBuffer<uint8_t> &buffer, T val, size_t size) ;

template <typename T>
class RingBuffer {
private:
	std::vector<T> buffer;
	std::unique_ptr<std::vector<T>> buffer_uptr;
	int head = 0;
	int tail = 0;
	size_t capacity;
	bool is_empty = true;
	
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
	RingBuffer(size_t capacity) : capacity(capacity){
		buffer.resize(capacity);
		buffer_uptr = std::make_unique<std::vector<T>>();
	}	
	
	bool empty() const {
		return (head == tail) && is_empty;
	}
	
	bool is_full() const {
		return (tail == head) && !is_empty;
	}
	
	size_t size() const {
		return (head <= tail) ? tail - head : capacity - (head - tail);
	}
	
	void push_back(T element) {
		if (is_full()) { //head == tail
			throw std::out_of_range("buffer is full");
		}
		
		buffer[tail] = element;
		tail = (tail + 1) % capacity;
		is_empty = false;
	}
	
	//lazy remove from circular buffer 
	//by moving head one element forward to give space to tail
	T& pop_front() {
		if (empty()) {
			throw std::out_of_range("buffer is empty");
		}
		
		T& rv = buffer[head];	
		head = (head + 1) % capacity;
		
		if (head == tail)
			is_empty = true;
		
		return rv;
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
	
	//append the val to buffer an asked number of times
	void insert(char val, size_t times) {
		if (times > capacity - this->size())
			throw std::out_of_range("RingBuffer::insert: arg is too big");
		
		for(size_t i = 0; i < times; i++) {
			push_back(val);
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
			throw std::invalid_argument("RingBuffer::memcpy: NULL passed");
		
		if (len > capacity - this->size())
			throw std::out_of_range("RingBuffer::memcpy: passed arg is too big");
		
		if (start > this->size())
			throw std::out_of_range("RingBuffer::memcpy: start idx is out of boundaries");
			
		auto it = iterator(*this, start);
		
		for (size_t i = 0; i < len; i++) {
			*it = *src;
			it++;
			src++;
		}
	}
	
	void append_nil() {
		this->push_back(TAG_NIL);
	}
	
	void append_int(int32_t val) {
		this->push_back(TAG_INT);
		append_helper(*this, val, sizeof(val));
	}
	
	void append_dbl(double val) {
		this->push_back(TAG_DBL);
		append_helper(*this, val, sizeof(double));
	}
	
	void append_str(const std::string &val) {
		this->push_back(TAG_STR);
		
		//size() returns the amount of bytes in str
		append_helper(*this, (uint32_t)val.size(), sizeof(uint32_t));
		this->insert((uint8_t *)val.data(), val.size());
	}
	
	void append_arr(uint32_t n) {
		this->push_back(TAG_ARR);
		append_helper(*this, n, sizeof(uint32_t));
	}
	
	void append_err(int32_t code, const std::string &msg) {
		this->push_back(TAG_ERR);
		append_helper(*this, code, sizeof(int32_t));
		append_helper(*this, (uint32_t)msg.size(), sizeof(uint32_t));
		this->insert((uint8_t *)msg.data(), msg.size());
		
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

#endif
