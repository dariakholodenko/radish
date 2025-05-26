#ifndef __HASHTABLE_HPP__
#define __HASHTABLE_HPP__

#include <cassert> 
#include <iostream>
#include <memory> //unique_ptr, shared_ptr
#include <new>
#include <string>

constexpr size_t MAX_LOAD_FACTOR = 3;
constexpr size_t MAX_NUM_ELEMENTS_TO_MOVE = 128;
//default 32-bit fnv hash function basis and prime values
constexpr int FNV_OFFSET_BASIS = 0x811c9dc5;
constexpr int FNV_PRIME = 0x01000193;

/* HashMap consists of two HashTables to improve performance 
 * and prevent latency during rehash 
 * by moving data to a bigger hashtable from the old one gradually,
 * namely const number of elements during each HT action would be moved,
 * so that an amortized time of rehash is only O(1)
 * 
 * HashTable is implemented as a dynamic array(array of buckets)
 * where a linked list of elements which hash function is equal to id
 * is stored at array[id] */
 
template <typename T, typename P>
class HashMap {
private:
	class HashNode {
	private:
		/* we want to store ptr so that in case there're strings 
		 * we could return string_view or shared_ptr 
		 * instead of copying the string*/
		std::shared_ptr<T> key;
		P value;
		HashNode *next;
	
	public:	
		HashNode(const T &key, const P &value)
			: key(std::make_shared<T>(key)), value(value), next(nullptr) {}
		
		const T &get_key() const {
			return *key;
		}
		
		std::shared_ptr<T>  get_key_ptr() {
			return key;
		}
				
		const P &get_value() const{
			return value;
		}
		
		void set_value(const P &val) {
			value = val;
		}
		
		friend class iterator;
		friend class HashTable;
		friend class HashMap;
	};
					
	class HashTable {
	private:
		HashNode **table; //array of buckets
		size_t capacity; //number of buckets
		size_t mask; //power of 2 array size, 2^n - 1
		size_t size; //overall number of elements in HashTable
		
	public:
		HashTable(size_t n) : capacity(n), mask(n - 1), size(0) {
			assert(n > 0 && ((n - 1) & n) == 0); //n is a power of 2
			
			table = new HashNode *[n](); //allocate and set to zero 
		}
		
		void clear() {
			for (size_t i = 0; i < capacity; i++) {
				if (table[i]) {
					HashNode *node = table[i];
					while (node) {
						HashNode *tmp = node;
						node = node->next;
						delete tmp;
						size--;
					}
					table[i] = nullptr;
				}
			}
		}
		
		~HashTable() {
			clear();			
			delete [] table;
			table = nullptr;
		}
		
		size_t hash_function(const T &key) {
			//this hash function is FNV, non-cryptographic hash
			size_t hash = FNV_OFFSET_BASIS;
			const uint8_t *arr = reinterpret_cast<const uint8_t *>(key.data());
			size_t len = key.size();//key.length() * sizeof(key[0]);
			
			for (size_t i = 0; i < len; i ++) {
				hash = hash ^ arr[i];
				hash *= FNV_PRIME;
			}
			
			//return bucket_id
			return hash & mask; //implicit hash % capacity;
		}
		
		size_t get_size() {
			return size;
		}
		
		size_t get_capacity() {
			return capacity;
		}
		
		std::shared_ptr<T> insert(HashNode *node) {
			assert(node);
			
			//HashNode **check = search(node->get_key());
			//if (check) //node with a given key already exists
			//	return;
				
			size_t bucket_id = hash_function(node->get_key());
			//if there's a collision just append new node 
			//to the front of the bucket's list
			HashNode *next = table[bucket_id]; 
			node->next = next;
			table[bucket_id] = node;
			size++;
			
			return node->get_key_ptr();
		}
		
		HashNode **search(const T &key) {
			size_t bucket_id = hash_function(key);
			HashNode **cur = &table[bucket_id]; 
			
			for (HashNode *it; (it = *cur) != nullptr; cur = &it->next) {
				if (it->get_key() == key)
					return cur;
			}
			
			return nullptr;
		}
		
		HashNode *erase(const T &key) {
			/* we're using a singly-linked list remove:
			 * remove nodes by assigning the next node to to_remove's ptr
			 * so that we don't need to change previous->next
			 * and after that return the removed node to actually deallocate it*/
			HashNode **to_remove;
			if ((to_remove = this->search(key)) != nullptr) {
				HashNode *node = *to_remove;
				*to_remove = node->next;
				size--;
				
				return node;
			}
			
			return nullptr;
		}
		
		HashNode *erase(HashNode **to_remove) {
			if (!to_remove || !*to_remove)
				return nullptr;
			
			return this->erase((*to_remove)->get_key());
		}
		
		
		friend class HashMap;
	};
	
	//we want to amortize rehashing time 
	//so we transfer data gradually during each HashMap action
	//from the old htab("rehashing_backup") to the newer and larger htab("htab")
	//once "rehashing_backup" is empty and "htab" is half full 
	//we simply replace rehashing_backup with htab
	//and create a new one bigger map and assign to "htab" for the next rehashing
	HashTable *htab;
	HashTable *rehashing_backup;
	//it's an idx till which the rehashing_backup was moved to the htab(which is larger)
	size_t move_id;
	
	//helper function that moves constant number of elements from backup to htab
	void _move_elements() {
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
	
	//the function that updates "htab" and "rehashing_backup"
	//once all the elements from rehashing_backup were moved to htab	
	void _rehash() {
		//start rehashing only when the old htable is empty
		if (rehashing_backup && rehashing_backup->get_size() != 0)
			return;
		
		if (rehashing_backup) {
			delete rehashing_backup; //destroy the old table
			rehashing_backup = nullptr;
		}
		
		rehashing_backup = htab; //assign the current newer version to the old one
		size_t capacity = htab->get_capacity() * 2; //double the current size
		htab = new HashTable(capacity); //create larger htable
		move_id = 0; 
		
	}
	
public:	
	class iterator {
		private:
		HashNode *cur;
		
		public:
		iterator(HashNode *node) : cur(node) {}

		HashNode& operator*() const {
			return *cur;
		}
		
		HashNode* operator->() const {
			return cur;
		}
		
		std::shared_ptr<T> first() {
			return cur->get_key_ptr();
		}
		
		const P &second() {
			return cur->get_value();
		}
		
		void set_second(const P &val) {
			cur->set_value(val);
		}
		
		//prefix increment
		iterator& operator++() {
			cur = cur->next;
			return *this;
		}
		
		//postfix increment
		iterator operator++(int) {
			iterator temp = *this;
			++(*this);
			return temp;
		}
		
		bool operator!=(const iterator &right) const {
			if (cur == nullptr || right.cur == nullptr)
				return cur != right.cur;
				
			return ((cur->get_key() != right->get_key()) ||
					(cur->get_value() != right->get_value()));
		}
		
		bool operator==(const iterator &right) const {
			if (cur == nullptr || right.cur == nullptr)
				return cur == right.cur;
				
			return ((cur->get_key() == right->get_key()) &&
					(cur->get_value() == right->get_value()));
		}
		
	};
	
	HashMap(size_t n) : 
		htab(new HashTable(n)), rehashing_backup(nullptr), move_id(0) {}

	~HashMap() {
		//HashTables themselves and allocated for their data nodes are destroyed using ~HashTable()
		if (htab) {
			delete htab;
			htab = nullptr;
		}
			
		if (rehashing_backup) {
			delete rehashing_backup;
			rehashing_backup = nullptr;
		}
	}
	
	iterator end() {
		return iterator(nullptr);
	}
	
	iterator search(const T &key) {
		this->_move_elements();
		
		HashNode **node = htab->search(key);
		
		//node is not found in htab and there're elements in backup
		if (!node && rehashing_backup)
			node = rehashing_backup->search(key);
			
		
		return node ? iterator(*node) : iterator(nullptr);
	}
	
	std::shared_ptr<T> insert(const T &key, const P &value) {
		if (!rehashing_backup || rehashing_backup->get_size() == 0) {
			size_t load_factor = htab->get_size() / htab->get_capacity();
			if (load_factor >= MAX_LOAD_FACTOR) { 
				_rehash();
			}
		}
		
		this->_move_elements();
		
		auto it = search(key);
		//if a key already exists just override its value with a new one
		if (it != this->end()) { 
			it.set_second(value);
			return it.first();
		}
			
		return htab->insert(new HashNode(key, value));;
	}
	
	std::unique_ptr<P> erase(const T &key) {
		this->_move_elements();
		
		HashNode **node = htab->search(key);
		HashNode *to_remove = nullptr;
		
		if (node) 
			to_remove = htab->erase(node);
		else if (rehashing_backup) {
			node = rehashing_backup->search(key);
			to_remove = rehashing_backup->erase(node);
		}
		
		if (to_remove) {
			std::unique_ptr<P> val = std::make_unique<P>(to_remove->get_value());
			delete to_remove;
			to_remove = nullptr;
			
			return val;
		}
		
		return nullptr;
	}
	
	//clear all the data from the HashMap
	void clear() {
		if (htab)
			htab->clear();
		
		if (rehashing_backup)
			rehashing_backup->clear();
		
		move_id = 0;
	}
	
	size_t size() {
		size_t size = htab->size;
		
		if (rehashing_backup) {
			size += rehashing_backup->size;
		}
		
		return size;
	}
	
};

#endif
