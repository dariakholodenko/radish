#include "hashmap.hpp"

//default 32-bit fnv hash function basis and prime values
#define FNV_OFFSET_BASIS 0x811c9dc5
#define FNV_PRIME 0x01000193

/* HashNode */
HashNode::HashNode(const std::string &key, const std::string &value) 
	: key(key), value(value), next(nullptr) {}

const std::string &HashNode::get_key() {
	return key;
}

const std::string &HashNode::get_value() {
	return value;
}

/* iterator */
iterator::iterator(HashNode *node) : cur(node) {}

HashNode& iterator::operator*() const {
	return *cur;
}

HashNode* iterator::operator->() const {
	return cur;
}

const std::string &iterator::first() {
	return cur->get_key();
}

const std::string &iterator::second() {
	return cur->get_value();
}

//prefix increment
iterator& iterator::operator++() {
	cur = cur->next;
	return *this;
}

//postfix increment
iterator iterator::operator++(int) {
	iterator temp = *this;
	++(*this);
	return temp;
}

bool iterator::operator!=(const iterator &right) const {
	return ((cur->get_key() != right->get_key()) ||
			(cur->get_value() != right->get_value()));
}

bool iterator::operator==(const iterator &right) const {
	return ((cur->get_key() == right->get_key()) &&
			(cur->get_value() == right->get_value()));
}
	
/* HashTable */

HashTable::HashTable(size_t n) : capacity(n), mask(n - 1), size(0) {
	assert(n > 0 && ((n - 1) & n) == 0); //n is a power of 2
	
	table = new HashNode *[n](); //allocate and set to zero 
}

HashTable::~HashTable() {
	delete [] table;
}

size_t HashTable::hash_function(const std::string &key) {
	//this hash function is FNV, non-cryptographic hash
	size_t hash = FNV_OFFSET_BASIS;
	uint8_t *arr = (uint8_t *)key.data();
	size_t len = key.length() * sizeof(key[0]);
	
	for (size_t i = 0; i < len; i ++) {
		hash = hash ^ arr[i];
		hash *= FNV_PRIME;
	}
	
	//return bucket_id
	return hash & mask; //implicit hash % capacity;
}

size_t HashTable::get_size() {
	return size;
}

size_t HashTable::get_capacity() {
	return capacity;
}

void HashTable::insert(HashNode *node) {
	if (!node)
		return;
	
	HashNode **check = search(node->get_key());
	if (check) //node with a given key already exists
		return;
		
	size_t bucket_id = HashTable::hash_function(node->get_key());
	//if there's a collision just append new node 
	//to the front of the bucket's list
	HashNode *next = table[bucket_id]; 
	node->next = next;
	table[bucket_id] = node;
	size++;
}

HashNode **HashTable::search(const std::string &key) {
	size_t bucket_id = HashTable::hash_function(key);
	HashNode **cur = &table[bucket_id]; 
	
	for (HashNode *it; (it = *cur) != nullptr; cur = &it->next) {
		if (it->get_key() == key)
			return cur;
	}
	
	return nullptr;
}

HashNode *HashTable::erase(const std::string &key) {
	HashNode **to_remove;
	if ((to_remove = this->search(key)) != nullptr) {
		HashNode *node = *to_remove;
		*to_remove = node->next;
		size--;
		
		return node;
	}
	
	return nullptr;
}

HashNode *HashTable::erase(HashNode **to_remove) {
	if (!to_remove)
		return nullptr;
	
	return this->erase((*to_remove)->get_key());
}

/* HashMap */

HashMap::HashMap(size_t n) : 
		htab(new HashTable(n)), rehashing_backup(nullptr), move_id(0) {}

HashMap::~HashMap() {
	if (htab)
		delete htab;
		
	if (rehashing_backup)
		delete rehashing_backup;
}

void HashMap::move_elements() {
	size_t moved = 0;
	
	if (!rehashing_backup)
		return; //no elements to move
		
	while (moved < MAX_NUM_ELEMENTS_TO_MOVE && rehashing_backup->get_size() > 0) {
		HashNode **node = &rehashing_backup->table[move_id];
		if (!*node) { //empty bucket
			move_id++;
			continue;
		}
		
		htab->insert(rehashing_backup->erase(node));
		moved++;
	}
	
	if (rehashing_backup->get_size() == 0) {
		delete rehashing_backup;
		rehashing_backup = nullptr;
	}
		
}

void HashMap::rehash() {
	//start rehashing only when the old htable is empty
	if (rehashing_backup && rehashing_backup->get_size() != 0)
		return;
	
	if (rehashing_backup) {
		delete rehashing_backup; //destroy the old table
		rehashing_backup = nullptr;
	}
	
	rehashing_backup = htab; //assign the current newer version to the old one
	size_t capacity = htab->get_capacity() * 2; //double the current size
	//delete htab;
	htab = new HashTable(capacity); //create larger htable
	move_id = 0; 
	
}

HashNode *HashMap::search(const std::string &key) {
	this->move_elements();
	
	HashNode **node = htab->search(key);
	
	//node is not found in htab and there're elements in backup
	if (!node && rehashing_backup)
		node = rehashing_backup->search(key);
		
	
	return node ? *node : nullptr;
}

void HashMap::insert(HashNode *node) {
	if (!node)
		return;
		
	htab->insert(node);
	
	if (!rehashing_backup || rehashing_backup->get_size() == 0) {
		size_t load_factor = htab->get_size() / htab->get_capacity();
		if (load_factor >= MAX_LOAD_FACTOR) { 
			rehash();
		}
	}
	
	this->move_elements();
}

void HashMap::erase(HashNode **node) {
	if (!node)
		return;
		
	rehashing_backup->erase(node);
	htab->erase(node);
	
	this->move_elements();
}

HashNode *HashMap::erase(const std::string &key) {
	HashNode **node = htab->search(key);
	if (node)
		htab->erase(node);
	else if (rehashing_backup) {
		node = rehashing_backup->search(key);
		rehashing_backup->erase(node);
	}
	
	this->move_elements();
	
	return node ? *node: nullptr;
}

size_t HashMap::size() {
	size_t size = htab->size;
	
	if (rehashing_backup) {
		size += rehashing_backup->size;
	}
	
	return size;
}
