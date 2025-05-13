#ifndef __SKIPLIST_HPP__
#define __SKIPLIST_HPP__

/* =====================================================================
 * This is SkipList data structure which stores paires of (key, value),
 * whereas the key(e.g. int/float) gives order but not necessarily unique 
 * and the value(e.g. strings) is unique. Pairs are sorted by keys.
 * Skiplist is a probabilistic data structure which is implemented 
 * as a double linked list of double linked lists, 
 * where nodes on each level is sorted and connected from above and below
 * to the copy of the same node. 
 * Each node is stored on the lowest level and has a 50% chance 
 * to be added to the level above for each higher level 
 * as well as a new empty level has a 50% chance to be added to the top.
 * //SkipList//
 * Main goal: range queries. It stores data in ordered way based on its key 
 * Complexity: O(logN) for every action on average
 * 
 * Ex. of SkipList:
 * 
 * +----+																	   +---+
 * | -∞ | -------------------------------------------------------------------->| ∞ |  Top level
 * +----+                                                           		   +---+
 * +----+     	         +---+               	 		 +---+                 +---+
 * | -∞ | -------------> | 2 |-------------------------> | 5 |---------------> | ∞ |
 * +----+    	         +---+                  		 +---+                 +---+  Level 3
 * +----+     	         +---+                +---+ 	 +---+                 +---+      
 * | -∞ | -------------> | 2 |--------------->| 4 | ---> | 5 |---------------> | ∞ |  Level 2
 * +----+    	         +---+                +---+      +---+                 +---+
 * +----+	  +---+		 +---+		+---+	  +---+		 +---+		+---+	   +---+
 * | -∞ |---> | 1 | ---> | 2 | ---> | 3 | --->| 4 | ---> | 5 | ---> | 6 | ---> | ∞ |  Bottom level
 * +----+     +---+      +---+      +---+     +---+      +---+      +---+      +---+
 * 	Head  	   1st   	  2nd   	 3rd   	   4th   	  5th   	 6th   		NIL
 * 	Node  	   Node  	  Node  	 Node  	   Node  	  Node  	 Node  		
 * 
 * =====================================================================*/
#include <stdexcept> //std::invalid_argument
#include <limits> //infinity()
#include <cstdlib> //rand(), srand()
#include <ctime> //time() for srand()

#define INFTY std::numeric_limits<T>::infinity()

template <typename T, typename P>
class SkipList {
private:
	class SkipNode {
		private:
		T key;
		size_t level;
		SkipNode *next, *prev, *down, *up;
		
		public:
		SkipNode(const T &key, size_t level = 0, 
			SkipNode *next = nullptr, SkipNode *prev = nullptr,
			SkipNode *down = nullptr, SkipNode *up = nullptr): 
											key(key), level(level), 
											next(next), prev(prev), 
											down(down), up(up) {}
											
		virtual ~SkipNode() = default;
		
		double get_key() const {
			return key;
		}
		
		size_t get_level() const {
			return level;
		}
		
		friend std::ostream& operator<<(std::ostream& out, const SkipNode &node) {
			out << node.get_key() << ":" << node.get_level();
		
			return out;
		}
		
		friend class SkipList;
		friend class iterator;
	};
	
	class DataSkipNode : public SkipNode {
	private:
		P value;
		
	public:
		DataSkipNode(const T &key, const P &value, size_t level = 0, 
			SkipNode *next = nullptr, SkipNode *prev = nullptr,
			SkipNode *down = nullptr, SkipNode *up = nullptr): 
			SkipNode(key, level, next, prev, down, up), value(value) {}
		
		~DataSkipNode() override = default;
				
		const P &get_value() const {
			return value;
		}
		
		friend std::ostream& operator<<(std::ostream& out, const DataSkipNode &node) {
			out << "(" << node.get_key() << ", " 
					<< node.get_value() << ") :" << node.get_level();
					
			return out;
		}
		
		friend class SkipList;
		friend class iterator;
	};
	
	class iterator {
		private:
		SkipNode *current_level;
		SkipNode *current_node;
		
		public:
		iterator(SkipNode *current_level, SkipNode *current_node): 
			current_level(current_level), current_node(current_node) {}
		
		T& operator*() const {
			return current_node->key;
		}
		
		T* operator->() const {
			return &current_node->key;
		}
		
		SkipNode *get_current() const{
			return current_node;
		}
		
		//prefix increment
		//first move right to the end of the current level
		//then move down to the beginning of the lower level
		iterator& operator++() {
			if (current_node && current_node->next)
				current_node = current_node->next;
			else {
				if (current_level && current_level->down) {
					current_level = current_level->down;
				}
				else {
					current_level = nullptr;
				}
				current_node = current_level;
			}
			return *this;
		}
		
		iterator& down() {
			current_node = current_node->down;
			current_level = current_level->down;
			
			return *this;
		}
		
		//postfix increment
		iterator operator++(int) {
			iterator temp = *this;
			++(*this);
			return temp;
		}
		
		bool operator!=(const iterator &right) const {
			return current_node != right.current_node;
		}
		
		bool operator==(const iterator &right) const {
			return current_node == right.current_node;
		}
		
	};

public:
	class citerator { //const-iterator
	private:
		SkipNode *current_level;
		SkipNode *current_node;
		
		SkipNode *get_current() const {
			return current_node;
		}
		
	public:
		citerator(SkipNode *current_level, SkipNode *current_node): 
			current_level(current_level), current_node(current_node) {}
		
		T& operator*() const {
			return current_node->key;
		}
		
		T* operator->() const {
			return &current_node->key;
		}
		
		const P &get_value() const {
			DataSkipNode *dnode;
			if ((dnode = dynamic_cast<DataSkipNode *>(current_node)) != nullptr)
				return dnode->get_value();
			else
				throw std::invalid_argument("no value");
		}
		
		//prefix increment
		//first move right to the end of the current level
		//then move down to the beginning of the lower level
		citerator& operator++() {
			if (current_node && current_node->next)
				current_node = current_node->next;
			else {
				if (current_level && current_level->down) {
					current_level = current_level->down;
				}
				else {
					current_level = nullptr;
				}
				current_node = current_level;
			}
			return *this;
		}
		
		//postfix increment
		citerator operator++(int) {
			citerator temp = *this;
			++(*this);
			return temp;
		}
		
		bool operator!=(const citerator &right) const {
			return current_node != right.current_node;
		}
		
		bool operator==(const citerator &right) const {
			return current_node == right.current_node;
		}
		
		friend std::ostream& operator<<(std::ostream& out, const citerator &it) {
			DataSkipNode *dnode = dynamic_cast<DataSkipNode *>(it.get_current());
			if (dnode != nullptr)
				out << *dnode << " ";
			else
				out << *it.get_current() << " ";
				
			return out;
		}
		
	};

private:
	SkipNode *top;
	
	SkipNode *_add_after(const T &key, const P &value, size_t level, SkipNode *node) {
		DataSkipNode *to_add = new DataSkipNode(key, value, level, node->next);
		node->next->prev = to_add;
		node->next = to_add;
		to_add->prev = node;
		
		return to_add;
	}
	
	//if 0: new node will be inserted to the next level or new level will be added
	//if 1: not adding
	bool _toss() {
		srand(time(0));
		return rand() % 2;
	}
	
	SkipNode *_insert_rec(const T &key, const P &value, SkipNode *node) {
		SkipNode *deeper, *new_node;
		//find the first node bigger than the key in the list
		while (node->next && node->next->key <= key) {
			if (node->get_key() == key || node->next->get_key() == key) {
				DataSkipNode *dnode;
				if ((dnode = dynamic_cast<DataSkipNode *>(node->next)) != nullptr) {
					if (dnode->get_value() >= value)
						break;
				}
			}
			node = node->next;
		}
		
		if (!node->down)
			return _add_after(key, value, 0, node); //the lowest level was reached
		
		//try to insert one level lower
		deeper = _insert_rec(key, value, node->down);
		
		/* try to insert on this level:
		 * if deeper is null the node wasn't inserted on the previous level 
		 * so nothing to insert here
		 * if it was, toss a coin to decide whether to insert on this level as well */
		if (!deeper || _toss())
			return nullptr; 
		
		//insert on this level if the previous conditions are met
		new_node = _add_after(key, value, deeper->level + 1, node);
		new_node->down = deeper;
		deeper->up = new_node;
	
		return new_node;		
	}
	
	//removes the node from all levels
	void _remove(SkipNode *node) {
		if (!node) 
			return;
			
		if (node->down) 
			node->down->up = nullptr;
		
		if(node->up)
			node->up->down = nullptr;
		
		if (node->prev) 
			node->prev->next = node->next;
		
		if(node->next)
			node->next->prev = node->prev;
			
		delete node;
		node = nullptr;
	}
	
	citerator _lookup_range(const T &key) {
		/*return a node with a given key from the lowest level
		 * or if not found then the closest to it from the lowest level */
		SkipNode *node = top;
		SkipNode *level = nullptr;
		while (1) {
			//return the last node <= key in the list
			for (; node->next && node->next->key <= key; node = node->next);

			if (!node->down) 
				break; //final level
			
			node = node->down;
		}
				
		level = node;
		for (; level->prev; level = level->prev);
		
		if (node->key != key && node->next)
			node = node->next;
			
		return citerator(level, node);
	}
	
	SkipNode *_lookup(const T &key) {
		/*return a node with a given key from the upper level
		 * or if not found then the closest(from below) to it from the lowest level */
		SkipNode *node = top;
		while (1) {
			//return the last node <= key in the list
			for (; node->next && node->next->key <= key; node = node->next);
			if (node->key == key)
				return node;
				
			if (!node->down) 
				break; //final level
			
			node = node->down;
		}
		
		return node;
	}
	
	SkipNode *_lookup(const T &key, const P &value) {
		/*return a node with a given key from the upper level
		 * or if not found then the closest to it from the lowest level */
		SkipNode *node = top;
		while (1) {
			//return the last node < key in the list
			for (; node->next && node->next->key < key; node = node->next);
			while (node->next && node->next->key == key) {
				node = node->next;
				if (node->get_key() == key) {
					DataSkipNode *dnode = dynamic_cast<DataSkipNode *>(node);
					if (dnode) {
						if (dnode->get_value() == value) {
							return node;
						}
						if (dnode->get_value() > value) {
							node = node->prev;
							break;
						}
					}
				}
			}
			
			if (!node->down) 
				break; //final level
			
			node = node->down;
		}
		
		return nullptr;
	}
	
	void _remove_empty_levels() {
		SkipNode *level_node = top;
		while (level_node) {
			if (level_node->next->key == INFTY && level_node->level > 0) {
				SkipNode *tmp = level_node->down;
				_remove(level_node->next);
				_remove(level_node);
				level_node = tmp;
			}
			else
				break;
		}
		top = level_node;
	}
	
public:	
	SkipList(): top(new SkipNode(-INFTY)) {
		top->next = new SkipNode(INFTY);
		top->next->prev = top;
	}
	
	iterator begin() {
		return iterator(top, top);
	}
		
	iterator end() {
		return iterator(nullptr, nullptr);
	}
	
	citerator cbegin() const {
		return citerator(top, top);
	}
		
	citerator cend() const {
		return citerator(nullptr, nullptr);
	}
	
	void clear() {
		iterator it = begin();
		while (it != end()) {
			SkipNode *tmp = it.get_current();
			it++;
			 //we'll delete all the -infty nodes separately
			 //since they used as between-levels navigation
			 //as well, if we want just clear the skiplist and not destroy it
			 //we need to save the top level INFTY dummy node
			 //and this way preserve the upper level
			 //to become  a reset skiplist
			if (tmp->get_key() != -INFTY) {
				if (tmp->get_key() == INFTY &&
								tmp->get_level() == top->get_level()) {
					tmp->prev = top;
					top->next = tmp;
				}
				else
					delete tmp;
			}
		}
		
		//if we want to just clear the skiplist 
		//then we don't want to delete the top level
		//to prevent double work by reallocating it at the end of the function
		it = begin().down();
		
		while (it != end()) {
			SkipNode *tmp = it.get_current();
			it.down();
			delete tmp;
		}
		
		top->level = 0;
		top->down = nullptr;
		top->next->level = 0;
	}
	
	~SkipList() {
		iterator it = begin();
		while (it != end()) {
			SkipNode *tmp = it.get_current();
			it++;
			 //we'll delete all the -infty nodes separately
			 //since they used as between-levels navigation
			if (tmp->get_key() != -INFTY)
				delete tmp;
		}
		
		it = begin();
		while (it != end()) {
			SkipNode *tmp = it.get_current();
			it.down();
			delete tmp;
		}
	}
	
	const T &search(const T &key) {
		SkipNode *node = _lookup(key);
		return node->key;
	}
	
	SkipNode *insert(const T &key, const P &value) {
		SkipNode *node = top;
		SkipNode *deeper;
		deeper = _insert_rec(key, value, node);
		
		if (!deeper || _toss())
			return top;
		
		SkipNode *dummy = new SkipNode(INFTY, deeper->level + 1);
		top = new SkipNode(-INFTY, deeper->level + 1, dummy, nullptr, top);
		dummy->prev = top;
		return top;
	}
	
	void erase(const T &key) {
		SkipNode *node = _lookup(key);
		if (node->key != key)
			return;
			
		//the node was found
		while (node) {
			SkipNode *tmp = node->down;
			_remove(node);
			node = tmp;
		}
		
		_remove_empty_levels();
	}
	
	void erase(const T &key, const P &value) {
		SkipNode *node = _lookup(key, value);
		if (!node) {
			return;
		}
			
		//the node was found
		while (node) {
			SkipNode *tmp = node->down;
			_remove(node);
			node = tmp;
		}
		
		_remove_empty_levels();
	}
	
	citerator search_range(const T &key) {
		citerator it = _lookup_range(key);
		return it;
	}
	
	friend std::ostream& operator<<(std::ostream& out, const SkipList<T, P> &sl) {
		for (citerator it = sl.cbegin(); it != sl.cend(); it++) {
			out << it << " ";
			if (*it == INFTY)
				out << "\n";
		}
	
		return out;
	}
	
};

#endif
