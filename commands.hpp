#ifndef __COMMANDS_HPP__
#define __COMMANDS_HPP__

//c++
#include <stdexcept> //invalid_argument
#include <string>
#include <memory> //unique_ptr
#include <unordered_map>
#include <vector>

//custom
#include "buffer.hpp"
#include "hashmap.hpp"
#include "sortedset.hpp"
#include "ttl_manager.hpp"

constexpr int hmap_base_capacity = 128;

struct CommandContext {
	HashMap<std::string, std::string> &hmap;
	TTLManager &ttl_manager;
	SortSet &sset;
	
	 CommandContext(HashMap<std::string, std::string>& h,
											TTLManager& ttl,
											SortSet& s)
						: hmap(h), ttl_manager(ttl), sset(s) {}
};

class Command {
public:
	virtual ~Command() {}
	virtual void execute(const std::vector<std::string> &cmd,
						RingBuffer<uint8_t> &buffer, CommandContext &ctx) = 0;
};

class GetCommand : public Command {
public:
	void execute(const std::vector<std::string> &cmd, 
				RingBuffer<uint8_t> &buffer, CommandContext &ctx) override;
};

class SetCommand : public Command {
public:
	void execute(const std::vector<std::string> &cmd, 
				RingBuffer<uint8_t> &buffer, CommandContext &ctx) override;
};

class DelCommand : public Command {
public:
	void execute(const std::vector<std::string> &cmd, 
				RingBuffer<uint8_t> &buffer, CommandContext &ctx) override;
};

class ExpireCommand : public Command {
	void execute(const std::vector<std::string> &cmd, 
				RingBuffer<uint8_t> &buffer, CommandContext &ctx) override;
};

class PersistCommand : public Command {
	void execute(const std::vector<std::string> &cmd, 
				RingBuffer<uint8_t> &buffer, CommandContext &ctx) override;
};

class GetTTLCommand : public Command {
	void execute(const std::vector<std::string> &cmd, 
				RingBuffer<uint8_t> &buffer, CommandContext &ctx) override;
};

class ZAddCommand : public Command {
	void execute(const std::vector<std::string> &cmd,
				RingBuffer<uint8_t> &buffer, CommandContext &ctx) override;
};

class ZRemCommand : public Command {
	void execute(const std::vector<std::string> &cmd, 
				RingBuffer<uint8_t> &buffer, CommandContext &ctx) override;
};

class ZRangeCommand : public Command {
	void execute(const std::vector<std::string> &cmd, 
				RingBuffer<uint8_t> &buffer, CommandContext &ctx) override;
};

typedef std::function<std::unique_ptr<Command>()> Creator;
class CommandFactory {
private:
	std::unordered_map<std::string, Creator> creators_dict;
public:
	CommandFactory();
	std::unique_ptr<Command> create_command(const std::string &name);
};

class CommandExecutor {
private:
	HashMap<std::string, std::string> hmap;
	TTLManager ttl_manager;
	SortSet sset;
	
public:
	CommandExecutor();
	
	CommandExecutor(const CommandExecutor &) = delete;
    CommandExecutor &operator=(const CommandExecutor &) = delete;
    
	void do_query(const std::vector<std::string> &cmd, 
										RingBuffer<uint8_t> &buffer);
};

#endif
