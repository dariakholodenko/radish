#ifndef __TTL_MANAGER_HPP_
#define __TTL_MANAGER_HPP_

//c++
#include <memory> //unique_ptr
#include <string>

//custom
#include "custom_heap.hpp"
#include "hashmap.hpp"

typedef enum : int {
	EXPIRED = -2,
	NOTTL = -1,
	FAILURE = 0,
	OK = 1,
} TTLStatus;

class TTLManager {
private:
	HashMap<std::string, std::string> &hmap;
	TTLHeap ttl_heap;
	
public:
	TTLManager(HashMap<std::string, std::string> &hmap);
	TTLManager(const TTLManager &) = delete;
	TTLManager &operator=(const TTLManager &) = delete;

	TTLStatus set(const std::string &key, int ttl_ms);
	TTLStatus remove(const std::string &key);
	int get_ttl(const std::string &key);
	void process_expired();
};

#endif
