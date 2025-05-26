#include "protocol.hpp" 

bool RequestParser::read_header(const uint8_t *&data, const uint8_t *&end, 
												size_t &header) {
	if (data + HEADER_SIZE > end) 	{
		return false;
	}
	
	std::memcpy(&header, data, HEADER_SIZE); 
	data += HEADER_SIZE;
	
	return true;
}

bool RequestParser::read_str(const uint8_t *&data, const uint8_t *&end, 
									size_t len, std::string &str) {
	if (data + len > end) 	{
		return false;
	}
	
	str.assign(data, data + len); 
	data += len;
	
	return true;
}

RequestParser::ParseResult RequestParser::parse(const std::vector<uint8_t> &request, size_t req_len) {
	const uint8_t *req_begin = request.data();
	const uint8_t *req_end = req_begin + req_len;
	size_t nstr = 0;
	std::vector<std::string> cmd;
	std::string reqstr(request.begin(), request.end());
	
	//parse how many strings there are in the request
	if (!read_header(req_begin, req_end, nstr)) {
		std::string msg = "failed to read string count: unexpected early end of request";
		return {false, {}, msg};
	}
	
	//parse each of the string to assemble a command 
	while (cmd.size() < nstr) {
		size_t len = 0;
		//parse how long is the current string
		if (!read_header(req_begin, req_end, len)) 	{
			std::string msg = "failed to read string len: unexpected early end of request";
			return {false, {}, msg};
		}
		
		//allocate an empty string in the cmd to fill it in read_str
		cmd.push_back(std::string());
		if (!read_str(req_begin, req_end, len, cmd.back())) 	{
			std::string msg = "failed to read string content: \
								the string is too long, len: " 
									+ std::to_string(len);
			return {false, {}, msg};
		}
	}
	
	if (req_begin != req_end) {
		std::string msg = "unexpected trailing data";
		return {false, {}, msg};
	}
		
	return {true, cmd, ""};
}
