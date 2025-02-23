#include "hashtable.hpp"

//default 32-bit fnv hash function basis and prime values
#define FNV_OFFSET_BASIS 0x811c9dc5
#define FNV_PRIME 0x01000193

using namespace hm;

/* HNode */
HNode::HNode(const std::string &key, const std::string &value) :
			key(key), value(value), next(nullptr) {}
			
std::string &HNode::get_value() {
	return value;
}

std::string &HNode::get_key() {
	return key;
}


/* iterator */
iterator::iterator(HNode *node) : cur(node) {}

HNode& iterator::operator*() const {
	return *cur;
}

HNode* iterator::operator->() const {
	return cur;
}

std::string &iterator::first() {
	return cur->key;
}

std::string &iterator::second() {
	return cur->value;
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
	return ((cur->key != right->key) || (cur->value != right->value));
}

bool iterator::operator==(const iterator &right) const {
	return ((cur->key == right->key) && (cur->value == right->value));
}
	
/* HTable */

HTable::HTable(size_t n) : capacity(n), mask(n - 1), size(0) {
	assert(n > 0 && ((n - 1) & n) == 0); //n is a power of 2
	
	table = new HNode *[n](); //allocate and set to zero 
}

HTable::~HTable() {
	delete [] table;
}

size_t HTable::hash_function(const std::string &key) {
	//this hash function is FNV, no-cryptographic hash
	size_t hash = FNV_OFFSET_BASIS;
	uint8_t *data = (uint8_t *)key.data();
	size_t len = key.length() * sizeof(key[0]);
	
	for (size_t i = 0; i < len; i ++) {
		hash = hash ^ data[i];
		hash *= FNV_PRIME;
	}
	
	//return bucket_id
	return hash & mask; //implicit hash % capacity;
}

size_t HTable::get_size() {
	return size;
}

size_t HTable::get_capacity() {
	return capacity;
}

void HTable::insert(HNode *node) {
	if (!node)
		return;
	
	HNode **check = find(node->get_key());
	if (check) //node with a given key already exists
		return;
		
	size_t bucket_id = HTable::hash_function(node->get_key());
	//if there's a collision just append new node 
	//to the front of the bucket's list
	HNode *next = table[bucket_id]; 
	node->next = next;
	table[bucket_id] = node;
	size++;
}

HNode **HTable::find(const std::string &key) {
	size_t bucket_id = HTable::hash_function(key);
	HNode **cur = &table[bucket_id]; 
	
	for (HNode *it; (it = *cur) != nullptr; cur = &it->next) {
		if (it->get_key() == key)
			return cur;
	}
	
	return nullptr;
}

HNode *HTable::erase(const std::string &key) {
	HNode **to_remove;
	if ((to_remove = this->find(key)) != nullptr) {
		HNode *node = *to_remove;
		*to_remove = node->next;
		size--;
		
		return node;
	}
	
	return nullptr;
}

HNode *HTable::erase(HNode **to_remove) {
	if (!to_remove)
		return nullptr;
	
	return this->erase((*to_remove)->key);
}

/* HMap */

HMap::HMap(size_t n) : 
		htab(new HTable(n)), rehashing_backup(nullptr), move_id(0) {}

HMap::~HMap() {
	if (htab)
		delete htab;
		
	if (rehashing_backup)
		delete rehashing_backup;
}

void HMap::move_elements() {
	size_t moved = 0;
	
	if (!rehashing_backup)
		return; //no elements to move
		
	while (moved < MAX_NUM_ELEMENTS_TO_MOVE && rehashing_backup->get_size() > 0) {
		HNode **node = &rehashing_backup->table[move_id];
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

void HMap::rehash() {
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
	htab = new HTable(capacity); //create larger htable
	move_id = 0; 
	
}

HNode *HMap::find(const std::string &key) {
	this->move_elements();
	
	HNode **node = htab->find(key);
	
	//node is not found in htab and there're elements in backup
	if (!node && rehashing_backup)
		node = rehashing_backup->find(key);
		
	
	return node ? *node : nullptr;
}

void HMap::insert(HNode *node) {	
	htab->insert(node);
	
	if (!rehashing_backup || rehashing_backup->get_size() == 0) {
		size_t load_factor = htab->get_size() / htab->get_capacity();
		if (load_factor >= MAX_LOAD_FACTOR) { 
			//std::cout << "\n\nrehash with load factor " << load_factor << "\n\n";
			rehash();
		}
	}
	
	this->move_elements();
}

void HMap::erase(HNode **node) {
	rehashing_backup->erase(node);
	htab->erase(node);
	
	this->move_elements();
}

HNode *HMap::erase(const std::string &key) {
	HNode **node = htab->find(key);
	if (node)
		htab->erase(node);
	else if (rehashing_backup) {
		node = rehashing_backup->find(key);
		rehashing_backup->erase(node);
	}
	
	this->move_elements();
	
	return node ? *node: nullptr;
}

size_t HMap::size() {
	size_t size = htab->size;
	
	if (rehashing_backup) {
		std::cout << "\nhtab size " << size << ", rehash size ";
		std::cout << rehashing_backup->size << "\n";
		size += rehashing_backup->size;
	}
	
	return size;
}
