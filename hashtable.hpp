#ifndef __HASHTABLE__
#define __HASHTABLE__

#include <cassert> 
#include <new>
#include <string>
#include <iostream>

#define MAX_LOAD_FACTOR 3
#define MAX_NUM_ELEMENTS_TO_MOVE 128

//TODO make iterator
namespace hm {
class HNode {
private:
	std::string key;
	std::string value;
	HNode *next;

public:	
	HNode(const std::string &key, const std::string &value);	
	std::string &get_value();
	std::string &get_key();
	
	friend class iterator;
	friend class HTable;
	friend class HMap;
};
	
class iterator {
	private:
	HNode *cur;
	
	public:
	iterator(HNode *node);
	HNode& operator*() const;
	HNode* operator->() const;
	iterator& operator++(); //prefix increment
	iterator operator++(int); //postfix increment
	std::string &first();
	std::string &second();
	bool operator!=(const iterator &right) const;
	bool operator==(const iterator &right) const;
};
	
class HTable {
private:
	HNode **table; //array of buckets
	size_t capacity; //number of buckets
	size_t mask; //power of 2 array size, 2^n - 1
	size_t size; //number of occupied entries
	
public:
	HTable(size_t n);
	~HTable();
	size_t hash_function(const std::string &key);
	size_t get_size();
	size_t get_capacity();
	void insert(HNode *node);
	HNode **find(const std::string &key);
	HNode *erase(HNode **node);
	HNode *erase(const std::string &key);
	
	friend class HMap;
};

class HMap {
private:
	//we want to amortize rehashing time 
	//so we transfer data gradually during each hmap action
	//from the old htab("rehashing_backup") to the newer and larger htab("htab")
	//once "rehashing_backup" is empty and "htab" is half full 
	//we simply replace rehashing_backup with htab
	//and create a new one bigger map and assign to "htab" for the next rehashing
	HTable *htab;
	HTable *rehashing_backup;
	//it's an idx till which the htable was moved to the larger htab
	size_t move_id;
	
	//helper function that moves constant number of elements from backup to htab
	void move_elements();
	//the function that updates "htab" and "rehashing_backup"
	//once all the elements from rehashing_backup were moved to htab
	void rehash();
	
public:
	HMap(size_t n);
	~HMap();
	HNode *find(const std::string &key);
	void insert(HNode *node);
	void erase(HNode **node);
	HNode *erase(const std::string &key);
	size_t size();
};

}


#endif
