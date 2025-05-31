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

#include <limits> //infinity()
#include <memory> //shared_ptr
#include <string>
#include <vector> 

//custom
#include "hashmap.hpp"
#include "skiplist.hpp"

using std::shared_ptr;

constexpr double MINUS_INFTY = -std::numeric_limits<double>::infinity();

/* TODO:
 * RANGE (w+w\t scores) +
 * RANGEGETBYSCORE
 * REMRANGEBYSCORE
 * RANK
 * */
class SortSet {
	private:
	HashMap<std::string, double> *map; //to store as (key=name, value=score)
	SkipList<double, shared_ptr<std::string>> *skiplist; //to store as (key=score, value=name)
	
	public:
	SortSet(size_t hashmap_size);
	
	~SortSet();
	//if we search by key then it's HashMap query
	double search(const std::string &name);
	//returns 1: if a new key was added
	//returns 0: if an already existing key was updated
	int insert(const std::string &name, double score);
	int erase(const std::string &name);
	std::vector<std::string> range(double score, size_t offset);
	void clear();
};

#endif
