#include "ttl_manager.hpp"

//c
#include <time.h> //clock_gettime() for better performance and poll() compatibility


static int get_monotonic_ms() {
	struct timespec tv = {0, 0};
	clock_gettime(CLOCK_MONOTONIC, &tv);
	return int(tv.tv_sec) * 1000 + tv.tv_nsec / 1000000;
}

TTLManager::TTLManager(HashMap<std::string, std::string> &hmap) : hmap(hmap) {}

TTLStatus TTLManager::set(const std::string &key, int ttl_ms) {
	auto it = hmap.search(key);
	if (it == hmap.end()) 
		return EXPIRED; //the key has expired or doesn't exist
		
	ttl_heap.insert(it.first(), ttl_ms);
	return OK;		
}

TTLStatus TTLManager::remove(const std::string &key) {
	auto it = hmap.search(key);
	if (it != hmap.end()) {
		HeapStatus rc;
		if ((rc = ttl_heap.remove(it.first())) == OK_H)
			return OK;
	}
	
	return EXPIRED; //the key has expired or doesn't exist
}

int TTLManager::get_ttl(const std::string &key) {
	auto it = hmap.search(key);
	if (it == hmap.end())
		return EXPIRED; //the key has expired or doesn't exist
	
	return ttl_heap.get_ttl(it.first());
}

void TTLManager::process_expired() {
	int now = get_monotonic_ms();
	while (!ttl_heap.empty() && ttl_heap.peek() <= now) {
		const std::string &key = *ttl_heap.delete_min();
		hmap.erase(key);
	}
}
