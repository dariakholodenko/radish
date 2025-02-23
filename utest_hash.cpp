#include <iostream>
#include <vector>
#include <string>

#include "hashtable.hpp"

#define RESET   "\033[0m"
#define BLACK   "\033[30m"      /* Black */
#define RED     "\033[31m"      /* Red */
#define GREEN   "\033[32m"      /* Green */
#define YELLOW  "\033[33m"      /* Yellow */
#define BLUE    "\033[34m"      /* Blue */
#define MAGENTA "\033[35m"      /* Magenta */
#define CYAN    "\033[36m"      /* Cyan */
#define WHITE   "\033[37m"      /* White */
#define ASSERT(NUMBER, RESULT)\
		do{\
			if(RESULT) {std::cout << "List Test "<< (NUMBER) << GREEN << " passed\n" << RESET;}\
			else {std::cout << "List Test "<< (NUMBER) << RED << " failed\n" << RESET;}\
		}while(0);
		
using namespace std;
using namespace hm;

void default_correctness_test() {
	HMap map(2);
	ASSERT("0.1", map.size() == 0);
	ASSERT("0.2", map.find("aa") == nullptr);
}

void basic_map_function_test() {
	HMap map(2);
	HNode node1("1", "a");
	HNode node2("2", "b");
	map.insert(&node1);
	ASSERT("1.1", map.size() == 1);
	ASSERT("1.2", map.find("1") == &node1);
	
	map.erase("1");
	ASSERT("1.3", map.size() == 0);
	ASSERT("1.4", map.find("1") == nullptr);
	
	map.insert(&node1);
	map.insert(&node2);
	ASSERT("1.5", map.size() == 2);
	ASSERT("1.6", map.find("1") == &node1);
	ASSERT("1.7", map.find("2") == &node2);
	ASSERT("1.8", map.find("2") == &node2);
	
	map.erase("1");
	ASSERT("1.9", map.size() == 1);
	ASSERT("1.10", map.find("1") == nullptr);
	ASSERT("1.11", map.find("2") == &node2);
	
	map.erase("2");
	ASSERT("1.12", map.size() == 0);
	ASSERT("1.13", map.find("1") == nullptr);
	ASSERT("1.14", map.find("2") == nullptr);
}

void chaining_test() {
	HMap map(4);
	vector<HNode *> v;
	size_t size = 5;
	
	for (size_t i = 0; i < size; i++) {
		HNode *node = new HNode(to_string(i), string(1, 'a' + i));
		v.push_back(node);
		map.insert(node);
	}
	
	ASSERT("2.1", map.size() == size);
	for (size_t i = 0; i < size; i++) {
		HNode *node = map.find(to_string(i));		
		string num = "2." + to_string(i + 1);
		ASSERT(num, node == v[i]);
		map.erase(to_string(i));
		delete node;
	}
}

void rehash_test() {
	HMap map(2);
	vector<HNode *> v;
	size_t size = 9;
	
	for (size_t i = 0; i < size; i++) {
		HNode *node = new HNode(to_string(i), string(1, 'a' + i));
		v.push_back(node);
		map.insert(node);
	}
	
	ASSERT("3.0", map.size() == size);
	
	for (size_t i = 0; i < size; i++) {
		HNode *node = map.find(to_string(i));
		//cout << node->get_key() << " " << node->get_value() << "\n";
		
		string num = "3." + to_string(i + 1);
		ASSERT(num, node == v[i]);
		map.erase(to_string(i));
		delete node;
	}
}

void fuzz_test() {
	size_t capacity = 4;
	size_t size = 2 << 20;
	HMap map(capacity);
	vector<HNode *> v;
	
	for (size_t i = 0; i < size; i++) {
		HNode *node = new HNode(to_string(i), string(1, 'a' + i));
		v.push_back(node);
		map.insert(node);
	}
	
	ASSERT("4.0", map.size() == size);
	
	int flag = 1;
	for (size_t i = 0; i < size; i++) {
		HNode *node = map.find(to_string(i));
		flag *= (node == v[i]);
		map.erase(to_string(i));
		delete node;
	}
	
	ASSERT("4.1", flag);
}

int main() {
	cout << "\ndefault_correctness_test\n";
	default_correctness_test();
    
	cout << "\nbasic_map_function_test\n";
	basic_map_function_test();
	
	cout << "\nchaining_test\n";
	chaining_test();
	
	cout << "\nrehash_test\n";
	rehash_test();
	
	cout << "\nfuzz_test\n";
	fuzz_test();
	
	return 0;
}
