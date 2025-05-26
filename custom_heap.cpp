#include <time.h> //clock_gettime() for better performance and poll() compatibility

#include "custom_heap.hpp"


static int get_monotonic_ms() {
	struct timespec tv = {0, 0};
	clock_gettime(CLOCK_MONOTONIC, &tv);
	return int(tv.tv_sec) * 1000 + tv.tv_nsec / 1000000;
}


/* HeapEntry */	
HeapEntry::HeapEntry(std::shared_ptr<std::string> key, int ttl) 
														: key(key) {
	expire_at = (ttl < 0) ? ttl : get_monotonic_ms() + ttl * 1000;
}

HeapEntry &HeapEntry::operator=(const HeapEntry &other) {
	key = std::move(other.key);
	heap_idx = other.heap_idx;
	expire_at = other.expire_at;
	
	return *this;
}

bool HeapEntry::operator==(const HeapEntry &other) {
	return expire_at == other.expire_at;
} 

bool HeapEntry::operator!=(const HeapEntry &other) {
	return expire_at != other.expire_at;
} 

bool HeapEntry::operator<(const HeapEntry &other) {
	return expire_at < other.expire_at;
}

bool HeapEntry::operator>(const HeapEntry &other) {
	return expire_at > other.expire_at;
}

bool HeapEntry::operator<=(const HeapEntry &other) {
	return expire_at <= other.expire_at;
}

bool HeapEntry::operator>=(const HeapEntry &other) {
	return expire_at >= other.expire_at;
}

std::shared_ptr<std::string> HeapEntry::get_key() const {
	return key;
}
		
int HeapEntry::get_ttl() {
	int now = get_monotonic_ms();
	if (expire_at < now) {
		return EXPIRED_H;
	}
	
	return (expire_at - now) / 1000;
}

int HeapEntry::get_heap_idx() const {
	return heap_idx;
}

int HeapEntry::get_expire_at() const {
	return expire_at;
}

void HeapEntry::update_ttl(int new_ttl) {
	expire_at = (new_ttl < 0) ? new_ttl : get_monotonic_ms() + new_ttl * 1000;
}

void HeapEntry::update_entry_idx(int idx) {
	heap_idx = idx;
}


/* TTLHeap */
void TTLHeap::swap(HeapEntry *lhs, HeapEntry *rhs) {
	HeapEntry *temp = lhs;
	lhs = rhs;
	rhs = temp;
}

void TTLHeap::sift_down(size_t i) {
	size_t cur_min = i;
	size_t left = 2 * i + 1;
	size_t right = 2 * i + 2;
	
	if ((left < heap.size()) && (heap[left] < heap[cur_min])) {
			cur_min = left;
	}
	
	if ((right < heap.size()) && (heap[right] < heap[cur_min])) {
			cur_min = right;
	}
	
	if (cur_min != i) {
		HeapEntry *cur_entry = heap[i];
		HeapEntry *cur_min_entry = heap[cur_min];
		
		swap(cur_entry, cur_min_entry);
		
		cur_entry->update_entry_idx(cur_min);
		update_in_map_idx(cur_entry);
		
		cur_min_entry->update_entry_idx(i);
		update_in_map_idx(cur_min_entry);
		
		sift_down(cur_min);
	}
}

void TTLHeap::sift_up(size_t i) {
	while (i != 0 && i < heap.size()) {
		HeapEntry *parent = heap[(i - 1) / 2];
		HeapEntry *child = heap[i];
		
		if (parent > child) {
			swap(parent, child);
			parent->update_entry_idx(i);
			update_in_map_idx(parent);
			child->update_entry_idx((i - 1) / 2);
			update_in_map_idx(child);
			i = (i - 1) / 2;
		}
		else
			break;
	}
}

void TTLHeap::heapify() {
	if (heap.empty())
		return;
		
	for (int i = heap.size() - 1; i > 0 ; i -= 2) {
		sift_down((i - 1) / 2);			
	}
}

void TTLHeap::update_in_map_idx(HeapEntry *entry) {
	key2idx[entry->get_key()] = entry->get_heap_idx();
}

void TTLHeap::update_key(size_t i, int new_key) {
	if (heap.empty()) {
		throw std::underflow_error("Heap is empty");
	}
	
	if (i >= heap.size()) {
		throw std::out_of_range("Invalid index");
	}
	
	bool up = 0;
	if (heap[i]->get_ttl() < new_key) {
		up = 1;
	}
	
	heap[i]->update_ttl(new_key);
	if (up) {
		sift_up(i);
	}
	else {
		sift_down(i);
	}
}

TTLHeap::~TTLHeap() {
	while (!heap.empty()) {
		HeapEntry *temp = heap.back();
		temp->update_ttl(-1);
		delete temp;
		heap.pop_back();			
	}
}

void TTLHeap::insert(std::shared_ptr<std::string> key, int ttl) {
	bool error = 0;
	auto it = key2idx.find(key);
	if (it != key2idx.end()) {
		//if a key alredy exists, update it
		try {
			update_key(it->second, ttl);
		}
		catch(...) {
			error = 1;
			key2idx.erase(it);
		}
	}
	if (error)
		return;
		
	HeapEntry *new_heap_entry = new HeapEntry(key, ttl);
	heap.push_back(new_heap_entry);
	size_t i = heap.size() - 1;
	new_heap_entry->update_entry_idx(i);
	update_in_map_idx(new_heap_entry);
	sift_up(i); 
}

int TTLHeap::peek() const {
	if (heap.empty()) {
		throw std::underflow_error("Heap is empty");
	}
	
	return heap[0]->get_expire_at();
}

HeapStatus TTLHeap::remove(std::shared_ptr<std::string> key) {
	auto it = key2idx.find(key);
	if (it == key2idx.end())
		return FAILURE_H;
	
	int i = it->second;
	
	if (i == 0) {
		delete_min();
		return OK_H;
	}
		
	HeapEntry *temp = heap[i];	
	heap[i] = heap.back();
	heap.pop_back();
	
	temp->update_entry_idx(-1);
	temp->update_ttl(NOTTL_H);
	delete temp;
	
	key2idx.erase(it); //remove the key from key2idx map
	
	heap[i]->update_entry_idx(i);
	sift_down(i);
	
	return OK_H;
}

int TTLHeap::get_ttl(std::shared_ptr<std::string> key) {
	auto it = key2idx.find(key);
	if (it == key2idx.end())
		return NOTTL_H;
	
	return heap[it->second]->get_ttl();
}

std::shared_ptr<std::string> TTLHeap::delete_min() {
	if (heap.empty()) {
		throw std::underflow_error("Heap is empty");
	}
	
	HeapEntry *min = heap[0];
	
	if (heap.size() > 1) {
		heap[0] = heap.back();
		sift_down(0);
	}
	
	auto it = key2idx.find(min->get_key());
	if (it != key2idx.end())
		key2idx.erase(it);
	
	auto ret = min->get_key();
	min->update_entry_idx(-1);
	min->update_ttl(NOTTL_H);
	delete min;
	
	heap.pop_back();
	
	return ret;
}

size_t TTLHeap::size() const {
	return heap.size();
}

bool TTLHeap::empty() const {
	return heap.empty();
}

