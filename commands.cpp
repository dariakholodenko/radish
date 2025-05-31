#include "commands.hpp"

/* GetCommand */
void GetCommand::execute(const std::vector<std::string> &cmd, 
			RingBuffer<uint8_t> &buffer, CommandContext &ctx) {
	ctx.ttl_manager.process_expired();
	
	if (cmd.size() >= 2) {
		auto it = ctx.hmap.search(cmd[1]);
		if (it != ctx.hmap.end()) {
			auto val = it.second();
			buffer.append_str(val);
		}
		else {
			buffer.append_nil();
		}
	}
	else
		throw std::invalid_argument("usage: get <key>");
}

/* SetCommand */
void SetCommand::execute(const std::vector<std::string> &cmd, 
			RingBuffer<uint8_t> &buffer, CommandContext &ctx) {
	ctx.ttl_manager.process_expired();
	
	if (cmd.size() >= 3) {
		//if a key already exists, its value will be overrided
		ctx.hmap.insert(cmd[1], cmd[2]);
		buffer.append_nil();
	}
	else 
		throw std::invalid_argument("usage: set <key> <val>");
}

/* DelCommand */
void DelCommand::execute(const std::vector<std::string> &cmd, 
			RingBuffer<uint8_t> &buffer, CommandContext &ctx) {
	ctx.ttl_manager.process_expired();
	
	if (cmd.size() >= 2) {
		auto val = ctx.hmap.erase(cmd[1]);
		//check if succeed in key deletion
		int rc = (val != nullptr);
		buffer.append_int(rc);
	}
	else
		throw std::invalid_argument("usage: del <key>");
}

/* ExpireCommand */
void ExpireCommand::execute(const std::vector<std::string> &cmd, 
			RingBuffer<uint8_t> &buffer, CommandContext &ctx) {
	ctx.ttl_manager.process_expired();
	
	if (cmd.size() >= 3) {
		try { //check that we received a valid number from stoi
			int ttl = std::stoi(cmd[2]);
			int32_t rc = ctx.ttl_manager.set(cmd[1], ttl);
			buffer.append_int((int32_t)rc);
		}
		catch(const std::invalid_argument &e) {
			buffer.append_err(RES_INVALID, "invalid ttl");
		}
		catch(const std::out_of_range &e) {
			buffer.append_err(RES_TOOLONG, "ttl is too long");
		}
	}
	else
		throw std::invalid_argument("usage: expire <key> <ttl>");
}

/* PersistCommand */
void PersistCommand::execute(const std::vector<std::string> &cmd, 
			RingBuffer<uint8_t> &buffer, CommandContext &ctx) {
	ctx.ttl_manager.process_expired();
	
	if (cmd.size() >= 2) {
		TTLStatus rc = ctx.ttl_manager.remove(cmd[1]);
		//if the key exists and hasn't expired yet: rc = OK
		//if the has already expired or doesn't exist(which is somewhat equal): rc = EXPIRED
		buffer.append_int(rc);
	}
	else
		throw std::invalid_argument("usage: persist <key>");
}

/* GetTTLCommand */
void GetTTLCommand::execute(const std::vector<std::string> &cmd, 
			RingBuffer<uint8_t> &buffer, CommandContext &ctx) {
	ctx.ttl_manager.process_expired();
	
	if (cmd.size() >= 2) {
		int rc = ctx.ttl_manager.get_ttl(cmd[1]);
		//if the key exists and hasn't expired yet: rc = ttl
		//if the has already expired or doesn't exist(which is somewhat equal): rc = EXPIRED
		//if no ttl was set for the given key: rc = NOTTL
		buffer.append_int(rc);
	}
	else
		throw std::invalid_argument("usage: ttl <key>");
}

/* ZAddCommand */
void ZAddCommand::execute(const std::vector<std::string> &cmd,
			RingBuffer<uint8_t> &buffer, CommandContext &ctx) {
	if (cmd.size() >= 3) {
		try { //check that we received a valid number from stoi
			int score = std::stoi(cmd[2]);
			int rc = ctx.sset.insert(cmd[1], score);
			//rc == 1: key was added
			//rc == 0: key was updated
			buffer.append_int(rc);
		}
		catch(const std::invalid_argument &e) {
			buffer.append_err(RES_INVALID, "invalid score");
		}
		catch(const std::out_of_range &e) {
			buffer.append_err(RES_TOOLONG, "score is too long");
		}
	}
	else
		throw std::invalid_argument("usage: zadd <key> <score>");
}

/* ZRemCommand */
void ZRemCommand::execute(const std::vector<std::string> &cmd, 
			RingBuffer<uint8_t> &buffer, CommandContext &ctx) {
	if (cmd.size() >= 2) {
		int rc = ctx.sset.erase(cmd[1]);
		//rc == 1: key was removed
		//rc == 0: no key was found
		buffer.append_int(rc);
	}
	else
		throw std::invalid_argument("usage: zrem <key>");
}

/* ZRangeCommand */
void ZRangeCommand::execute(const std::vector<std::string> &cmd, 
			RingBuffer<uint8_t> &buffer, CommandContext &ctx) {
	//TODO add an option to pass only beginning of the range
	//TODO add an option get all keys in order(one special key as arg)
	if (cmd.size() >= 3) {
		try { //check that we received a valid number from stoi
			auto v = ctx.sset.range(std::stoi(cmd[1]), std::stoi(cmd[2]));
			buffer.append_arr(v.size());
			for (auto it : v) {
				buffer.append_str(it);
			}
		}
		catch(const std::invalid_argument &e) {
			buffer.append_err(RES_INVALID, "invalid score");
		}
		catch(const std::out_of_range &e) {
			buffer.append_err(RES_TOOLONG, "score is too long");
		}
	}
	else
		throw std::invalid_argument("usage: zrange <from> <to>");
}

/* CommandFactory */
CommandFactory::CommandFactory() {
	creators_dict["get"] = [] { return std::make_unique<GetCommand>(); };
	creators_dict["set"] = [] { return std::make_unique<SetCommand>(); };
	creators_dict["del"] = [] { return std::make_unique<DelCommand>(); };
	
	creators_dict["expire"] = [] { return std::make_unique<ExpireCommand>(); };
	creators_dict["persist"] = [] { return std::make_unique<PersistCommand>(); };
	creators_dict["ttl"] = [] { return std::make_unique<GetTTLCommand>(); };
	
	creators_dict["zadd"] = [] { return std::make_unique<ZAddCommand>(); };
	creators_dict["zrem"] = [] { return std::make_unique<ZRemCommand>(); };
	creators_dict["zrange"] = [] { return std::make_unique<ZRangeCommand>(); };
}

std::unique_ptr<Command> CommandFactory::create_command(const std::string &name) {
	auto it = creators_dict.find(name);
	if (it != creators_dict.end()) {
		return it->second();
	}
	
	return nullptr;
}

/* CommandExecutor */
CommandExecutor::CommandExecutor() 
	: hmap(HashMap<std::string, std::string>(hmap_base_capacity)),
										ttl_manager(hmap),
										sset(hmap_base_capacity) {}

void CommandExecutor::do_query(const std::vector<std::string> &cmd, 
									RingBuffer<uint8_t> &buffer) {
	if (cmd.empty()) {
		buffer.append_err(RES_NOCMD, "no input");
		return;
	}
	
	std::unique_ptr<Command> command 
						= CommandFactory().create_command(cmd[0]);
	try {
		if (command) {
			CommandContext ctx(hmap, ttl_manager, sset);
			command->execute(cmd, buffer, ctx);
		}
		else buffer.append_err(RES_NOCMD, "command doesn't exist");
	}
	catch(const std::exception &e) {
		buffer.append_err(RES_NOCMD, e.what());
	}
}

