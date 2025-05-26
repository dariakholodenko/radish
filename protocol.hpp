#ifndef __PROTOCOL_HPP__
#define __PROTOCOL_HPP__

//c++
#include <cstring> //std::memcpy
#include <string>
#include <vector>

//custom
#include "io_shared_library.hpp" //HEADER_SIZE definition

/** Here are stored all protocol related classes that help to parse 
 * received from clients payloads according to the following protocol:

	req:
		+------+------+------+-----+------+
		| len1 | msg1 | len2 | msg2 | ... |
		+------+------+------+-----+------+
	
	msg format:
		+------+------+------+------+-----+------+------+
		| nstr | len1 | str1 | len2 | ... | lenn | strn |
		+------+------+------+------+-----+------+------+
	
	resp:
		+--------+------+
		| status | data |
		+--------+------+
**/

class RequestParser {
private:
	bool read_header(const uint8_t *&data, 
								const uint8_t *&end, size_t &header);
	bool read_str(const uint8_t *&data, 
					const uint8_t *&end, size_t len, std::string &str);
public:
	struct ParseResult {
		bool success;
		std::vector<std::string> cmd;
		std::string error_msg;
	};
	
	ParseResult parse(const std::vector<uint8_t> &request, size_t req_len);
};

#endif
