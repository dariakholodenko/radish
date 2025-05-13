#include "custom_heap.hpp"

/* HeapEntry */
HeapEntry::HeapEntry(int ttl, Entry *entry) : ttl(ttl), entry(entry) {
	expire_at = (ttl < 0) ? ttl : get_monotonic_ms() + ttl * 1000;
	entry->set_ttl(ttl, expire_at);
}
	
bool HeapEntry::operator==(const HeapEntry &second) {
	return expire_at == second.expire_at;
} 

bool HeapEntry::operator!=(const HeapEntry &second) {
	return expire_at != second.expire_at;
} 

bool HeapEntry::operator<(const HeapEntry &second) {
	return expire_at < second.expire_at;
}

bool HeapEntry::operator>(const HeapEntry &second) {
	return expire_at > second.expire_at;
}

bool HeapEntry::operator<=(const HeapEntry &second) {
	return expire_at <= second.expire_at;
}

bool HeapEntry::operator>=(const HeapEntry &second) {
	return expire_at >= second.expire_at;
}

int HeapEntry::get_ttl() const {
	return ttl;
}

int HeapEntry::get_expire_at() const {
	return expire_at;
}

Entry *HeapEntry::get_entry() const {
		return entry;
}

void HeapEntry::update_ttl(int new_ttl) {
	ttl = new_ttl;
	expire_at = (ttl < 0) ? ttl : get_monotonic_ms() + ttl * 1000;
	entry->set_ttl(ttl, expire_at);
}

void HeapEntry::update_entry_idx(int idx) {
	entry->set_heap_idx(idx);
}


/* CustomHeap */
void CustomHeap::swap(HeapEntry *lhs, HeapEntry *rhs) {
	HeapEntry *temp(lhs);
	lhs = rhs;
	rhs = temp;
}

void CustomHeap::sift_down(size_t i) {
	size_t cur_min = i;
	size_t left = 2 * i + 1;
	size_t right = 2 * i + 2;
	
	if ((left < array.size()) && (*array[left] < *array[cur_min])) {
			cur_min = left;
	}
	
	if ((right < array.size()) && (*array[right] < *array[cur_min])) {
			cur_min = right;
	}
	
	if (cur_min != i) {
		HeapEntry *cur_entry = array[i];
		HeapEntry *cur_min_entry = array[cur_min];
		swap(cur_entry, cur_min_entry);
		cur_entry->update_entry_idx(cur_min);
		cur_min_entry->update_entry_idx(i);
		
		sift_down(cur_min);
	}
}

void CustomHeap::sift_up(size_t i) {
	while (i != 0 && i < array.size()) {
		HeapEntry *parent = array[(i - 1) / 2];
		HeapEntry *child = array[i];
		
		if (*parent > *child) {
			swap(parent, child);
			parent->update_entry_idx(i);
			child->update_entry_idx((i - 1) / 2);
			i = (i - 1) / 2;
		}
		else
			break;
	}
}

void CustomHeap::heapify() {
	if (array.empty())
		return;
		
	for (int i = array.size() - 1; i > 0 ; i -= 2) {
		sift_down((i - 1) / 2);			
	}
}

CustomHeap::~CustomHeap() {
	while (!array.empty()) {
		HeapEntry *temp = array.back();
		temp->update_ttl(-1);
		delete temp;
		array.pop_back();			
	}
}

size_t CustomHeap::size() {
	return array.size();
}

bool CustomHeap::empty() {
	return array.empty();
}

void CustomHeap::insert(int ttl, Entry *entry) {
	HeapEntry *new_heap_entry = new HeapEntry(ttl, entry);
	array.push_back(new_heap_entry);
	size_t i = array.size() - 1;
	new_heap_entry->update_entry_idx(i);
	sift_up(i);
}

Entry *CustomHeap::delete_min() {
	if (array.empty()) {
		throw std::underflow_error("Heap is empty");
	}
	
	HeapEntry *min = array[0];
	
	if (array.size() > 1) {
		array[0] = array.back();
		sift_down(0);
	}
		
	array.pop_back();
	
	min->update_entry_idx(-1);
	min->update_ttl(NOTTL);
	
	Entry *ret = min->get_entry();
	delete min;
	
	return ret;
}

int CustomHeap::peek() const {
	if (array.empty()) {
		throw std::underflow_error("Heap is empty");
	}
	
	return array[0]->get_expire_at();
}

void CustomHeap::delete_key(size_t i) {
	if (i >= array.size())
		throw std::out_of_range("Invalid index");
	
	if (i == 0) {
		delete_min();
		return;
	}
		
	HeapEntry *temp = array[i];	
	array[i] = array.back();
	array.pop_back();
	
	temp->update_entry_idx(-1);
	temp->update_ttl(NOTTL);
	delete temp;
	
	array[i]->update_entry_idx(i);
	sift_down(i);
}

void CustomHeap::update_key(size_t i, int new_key) {
	if (array.empty()) {
		throw std::underflow_error("Heap is empty");
	}
	
	if (i >= array.size()) {
		throw std::out_of_range("Invalid index");
	}
	
	bool up = 0;
	if (array[i]->get_ttl() < new_key) {
		up = 1;
	}
	
	array[i]->update_ttl(new_key);
	if (up) {
		sift_up(i);
	}
	else {
		sift_down(i);
	}
}
