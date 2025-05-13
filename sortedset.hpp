#ifndef __SORTED_SET_HPP__
#define __SORTED_SET_HPP__

/* =====================================================================
 * This is Sorted Set data structure which stores paires of (name, score).
 * It's functionlity is achived by using simultaniously HashMap and SkipList,
 * whereas HashMap is used for it's quick access and for providing uniqueness of names
 * and SkipList is responsible for storing pairs in a sorted order.
 * //HashMap://
 * Main goal: point queries. Uses name as key.
 * Complexity: O(1) for every action on average
 * //SkipList//
 * Main goal: range queries. It stores data in ordered way based on its score 
 * Complexity: O(logN) for every action on average
 * =====================================================================*/
 
#include <string>
#include <limits> //infinity()
#include <vector> 

#include "hashmap.hpp"
#include "skiplist.hpp"

#define MINUS_INFTY -std::numeric_limits<double>::infinity()

/* TODO:
 * RANGE (w+w\t scores) +
 * RANGEGETBYSCORE
 * REMRANGEBYSCORE
 * RANK
 * */
class SortSet {
	private:
	HashMap<std::string, double> *map; //to store as (key=name, value=score)
	SkipList<double, std::string> *skiplist; //to store as (key=score, value=name)
	
	public:
	SortSet(size_t hashmap_size) : map(new HashMap<std::string, double>(hashmap_size)), 
							skiplist(new SkipList<double, std::string>()) {}
	
	~SortSet() {
		delete map;
		map = nullptr;
		delete skiplist;
		skiplist = nullptr;
	}
	
	//if we search by key then it's HashMap query
	double search(const std::string &name) {
		auto it = map->search(name);
		
		if (it == map->end())
			return MINUS_INFTY;
			
		return it.second();
	}
	
	//returns 1: if a new key was added
	//returns 0: if an already existing key was updated
	int insert(const std::string &name, double score) {
		int rc = 1;
		//if a node with a given key already exists, change its score
		auto it = map->search(name);
		if (it != map->end()) {
			skiplist->erase(it.second(), name);
			it.set_second(score); //reset the score for the hashmap node
			rc = 0; //the key already exists and was updated
		}
		
		//insert to the hashmap only new keys to prevent duplications
		if (rc) { 
			map->insert(name, score);
		}
			
		skiplist->insert(score, name);
		
		return rc;
	}
	
	int erase(const std::string &name) {
		int rc = 0;
		std::unique_ptr<double> score = map->erase(name);
		if (score) {
			skiplist->erase(*score, name);
			rc = 1;
		}
		
		return rc;
	}
	
	std::vector<std::string> range(double score, size_t offset) {
		auto it = skiplist->search_range(score);
		std::vector<std::string> v;
		for (size_t i = 0; i < offset && it != skiplist->cend(); i++) {
			try {
				const std::string &val = it.get_value();
				v.push_back(val);
				std::cout << it;
			}
			catch(...) {}
			it++;
		}
		std::cout << "\n";
		
		return v;
	}	
	
	void clear() {
		map->clear();
		skiplist->clear();
	}
};

#endif
