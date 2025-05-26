#ifndef __CUSTOM_HEAP_HPP__
#define __CUSTOM_HEAP_HPP__

#include <iostream>
#include <memory> //std::shared_ptr
#include <stdexcept>
#include <string>
#include <vector>
#include <unordered_map>

enum HeapStatus {
	EXPIRED_H = -2,
	NOTTL_H = -1,
	FAILURE_H = 0,
	OK_H = 1,
};

class HeapEntry {
private:
	std::shared_ptr<std::string> key;
	int heap_idx = -1;
	int expire_at = -1;

public:
	HeapEntry(std::shared_ptr<std::string> key, int ttl);
	~HeapEntry() = default;
	HeapEntry &operator=(const HeapEntry &other);
	
	bool operator==(const HeapEntry &other);
	bool operator!=(const HeapEntry &other);
	bool operator<(const HeapEntry &other);
	bool operator>(const HeapEntry &other);
	bool operator<=(const HeapEntry &other);
	bool operator>=(const HeapEntry &other);
	
	std::shared_ptr<std::string> get_key() const;
	int get_ttl();
	int get_heap_idx() const;
	int get_expire_at() const;
	
	void update_ttl(int new_ttl);
	void update_entry_idx(int idx);
};
	
class TTLHeap {
private:	
	//heap is used to maintain order of TTLs
	std::vector<HeapEntry *> heap; 
	//a map between keys and their heap indexes 
	//to allow action such as remove and get_ttl for a random key
	std::unordered_map<std::shared_ptr<std::string> , size_t> key2idx;
	
	void swap(HeapEntry *lhs, HeapEntry *rhs);
	void sift_down(size_t i);
	void sift_up(size_t i);
	void heapify();
	void update_in_map_idx(HeapEntry *entry);
	void update_key(size_t i, int new_key);

public:
	~TTLHeap();
	void insert(std::shared_ptr<std::string> key, int ttl);
	HeapStatus remove(std::shared_ptr<std::string> key);
	int peek() const;
	int get_ttl(std::shared_ptr<std::string>key);
	std::shared_ptr<std::string> delete_min();
	
	size_t size() const;
	bool empty() const;
	
	
	friend std::ostream& operator<<(std::ostream& out, const TTLHeap &heap) {
		for (HeapEntry *it : heap.heap) {
			out << it->get_ttl() << " ";
		}
		
		out << "\n";
		
		return out;
	}
};

#endif
