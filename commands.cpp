#include "server.hpp"
#include <vector>
#include <functional>

Commands::Commands(Server * const sv) {
	usrcmds = {
		{"adminlogin", std::bind(Commands::adminlogin, sv, this, std::placeholders::_1, std::placeholders::_2)}
	};

	admincmds = {
		{"help", std::bind(Commands::help, sv, this, std::placeholders::_1, std::placeholders::_2)},
		{"tp", std::bind(Commands::teleport, sv, this, std::placeholders::_1, std::placeholders::_2)},
		{"kick", std::bind(Commands::kick, sv, this, std::placeholders::_1, std::placeholders::_2)}
	};
}

std::string Commands::get_cmd_list(const bool admin) const {
	std::string m("");
	for(const auto& cmd : usrcmds){
		m += cmd.first + ", ";
	}
	if(admin){
		for(const auto& cmd : admincmds){
			m += cmd.first + ", ";
		}
	}
	m.erase(m.end() - 2, m.end());
	return m;
}

std::vector<std::string> Commands::split_args(const std::string& msg) {
	std::vector<std::string> args;
	size_t i = 0;
	size_t j = 0;
	const char * msgp = msg.c_str();
	do {
		if(msgp[i] == ' '){
			if(i - j > 0){
				args.push_back(std::string(msgp + j, i - j));
			}
			j = i + 1; /* +1 to skip the space */
		}
	} while(++i < msg.size());
	if(i - j > 0){
		args.push_back(std::string(msgp + j, i - j));
	}
	return args;
}

bool Commands::exec(Client * const cl, const std::string& msg) const {
	std::vector<std::string> args(split_args(msg));
	if(args.size() > 0){
		if(cl->is_admin()){
			auto search = admincmds.find(args[0]);
			if(search != admincmds.end()){
				std::cout << "Admin: " << cl->id << " (" << cl->get_world()->name
					<< ", " << cl->ip << ") Executed: " << msg << std::endl;
				search->second(cl, args);
				return true;
			}
		}
		auto search = usrcmds.find(args[0]);
		if(search != usrcmds.end()){
			search->second(cl, args);
			return true;
		}
	}
	return false;
}


void Commands::adminlogin(Server * const sv, const Commands * const cmd,
			Client * const cl, const std::vector<std::string>& args) {
	if(!cl->is_admin() && args.size() == 2){
		if(sv->is_adminpw(args[1])){
			std::cout << "User: " << cl->id << " (" << cl->get_world()->name
					<< ", " << cl->ip << ") Got admin!" << std::endl;
			cl->promote();
		} else {
			/* One try */
			cl->safedelete(true);
		}
	}
}

void Commands::help(Server * const sv, const Commands * const cmd,
			Client * const cl, const std::vector<std::string>& args) {
	const std::string m(cmd->get_cmd_list(cl->is_admin()));
	cl->tell("Server: " + m + ".");
}

void Commands::teleport(Server * const sv, const Commands * const cmd,
			Client * const cl, const std::vector<std::string>& args) {
	if(args.size() == 3){
		int32_t x = 0;
		int32_t y = 0;
		try {
			x = stoi(args[1]);
			y = stoi(args[2]);
		} catch(std::invalid_argument) {
			return;
		} catch(std::out_of_range) {
			return;
		}
		cl->tell("Server: Teleported to X: " + std::to_string(x) + ", Y: " + std::to_string(y));
		cl->teleport(x, y);
	}
}

void Commands::kick(Server * const sv, const Commands * const cmd,
			Client * const cl, const std::vector<std::string>& args) {
	if(args.size() == 2){
		uint32_t id = 0;
		try {
			id = stoul(args[1]);
		} catch(std::invalid_argument) {
			return;
		} catch(std::out_of_range) {
			return;
		}
		Client * const target = cl->get_world()->get_cli(id);
		if(target){
			target->safedelete(true);
		}
	}
}
