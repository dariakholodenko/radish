#ifndef __CUSTOM_HEAP_HPP__
#define __CUSTOM_HEAP_HPP__

#include <vector>
#include <stdexcept>
#include <iostream>

#include "wrapper.hpp"

class HeapEntry {
private:
	int ttl; //ttl that was received as input
	int expire_at;
	Entry *entry;

public:
	HeapEntry(int ttl, Entry *entry);
	
	bool operator==(const HeapEntry &second);
	
	bool operator!=(const HeapEntry &second);
	
	bool operator<(const HeapEntry &second);
	
	bool operator>(const HeapEntry &second);
	
	bool operator<=(const HeapEntry &second);
	
	bool operator>=(const HeapEntry &second);
		
	int get_ttl() const;
	
	int get_expire_at() const;
	
	Entry *get_entry() const;	
	
	void update_ttl(int new_ttl);
	
	void update_entry_idx(int idx);
};

class CustomHeap {
private:
	std::vector<HeapEntry *> array;
	
	void swap(HeapEntry *lhs, HeapEntry *rhs);
	
	void sift_down(size_t i);
	
	void sift_up(size_t i);
	
	void heapify();

public:
	CustomHeap() = default;
	
	~CustomHeap();
	
	size_t size();
	
	bool empty();
	
	void insert(int ttl, Entry *entry);
	
	Entry *delete_min();
	
	int peek() const;
	
	void delete_key(size_t i);
	
	void update_key(size_t i, int new_key);
	
	friend std::ostream& operator<<(std::ostream& out, const CustomHeap &heap) {
		for (HeapEntry *it : heap.array) {
			out << it->get_ttl() << " ";
		}
		
		out << "\n";
		
		return out;
	}
};

#endif
