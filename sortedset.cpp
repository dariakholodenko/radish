#include "sortedset.hpp"

SortSet::SortSet(size_t hashmap_size) 
			: map(new HashMap<std::string, double>(hashmap_size)), 
			skiplist(new SkipList<double, shared_ptr<std::string>>()) {}

SortSet::~SortSet() {
	delete map;
	map = nullptr;
	delete skiplist;
	skiplist = nullptr;
}

//if we search by key then it's HashMap query
double SortSet::search(const std::string &name) {
	auto it = map->search(name);
	
	if (it == map->end())
		return MINUS_INFTY;
		
	return it.second();
}

//returns 1: if a new key was added
//returns 0: if an already existing key was updated
int SortSet::insert(const std::string &name, double score) {
	int rc = 1;
	//if a node with a given key already exists, change its score
	auto it = map->search(name);
	if (it != map->end()) {
		skiplist->erase(it.second(), it.first());
		it.set_second(score); //update the score for the hashmap node
		rc = 0; //the key already exists and was updated
	}
	
	//insert to hashmap only new keys to prevent unnecessary searches
	if (rc) { 
		//want to insert the direct reference to the allocated string in Hashmap
		skiplist->insert(score, map->insert(name, score));
	}
	else
		skiplist->insert(score, it.first());
	
	return rc;
}

int SortSet::erase(const std::string &name) {
	int rc = 0;
	auto it = map->search(name);
	if (it != map->end()) {
		skiplist->erase(it.second(), it.first());
		map->erase(name);
		rc = 1;
	}
	
	return rc;
}

std::vector<std::string> SortSet::range(double score, size_t offset) {
	auto it = skiplist->search_range(score);
	std::vector<std::string> v;
	for (size_t i = 0; i < offset && it != skiplist->cend(); i++) {
		try {
			const std::string &val = *it.get_value();
			v.push_back(val);
			std::cout << it;
		}
		catch(...) {}
		it++;
	}
	std::cout << "\n";
	
	return v;
}	

void SortSet::clear() {
	map->clear();
	skiplist->clear();
}
