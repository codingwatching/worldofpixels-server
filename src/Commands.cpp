#include "Commands.hpp"

#include <Server.hpp>
#include <Client.hpp>
#include <World.hpp>

#include <misc/utils.hpp>

#include <algorithm>
#include <iostream>

Commands::Commands(Server * const sv) {
	usrcmds = {
		{"nick", std::bind(Commands::nick, sv, this, std::placeholders::_1, std::placeholders::_2)},
		{"pass", std::bind(Commands::pass, sv, this, std::placeholders::_1, std::placeholders::_2)},
		{"tell", std::bind(Commands::tell, sv, this, std::placeholders::_1, std::placeholders::_2)},
		{"help", std::bind(Commands::help, sv, this, std::placeholders::_1, std::placeholders::_2)},
		{"modlogin", std::bind(Commands::modlogin, sv, this, std::placeholders::_1, std::placeholders::_2)},
		{"adminlogin", std::bind(Commands::adminlogin, sv, this, std::placeholders::_1, std::placeholders::_2)}
	};

	modcmds = {
		{"tp", std::bind(Commands::teleport, sv, this, std::placeholders::_1, std::placeholders::_2)},
		{"kickip", std::bind(Commands::kickip, sv, this, std::placeholders::_1, std::placeholders::_2)},
		{"banip", std::bind(Commands::banip, sv, this, std::placeholders::_1, std::placeholders::_2)},
		{"bans", std::bind(Commands::bans, sv, this, std::placeholders::_1, std::placeholders::_2)},
		{"whitelist", std::bind(Commands::whitelist, sv, this, std::placeholders::_1, std::placeholders::_2)},
		{"blacklist", std::bind(Commands::blacklist, sv, this, std::placeholders::_1, std::placeholders::_2)},
		{"setrank", std::bind(Commands::setrank, sv, this, std::placeholders::_1, std::placeholders::_2)},
		{"whois", std::bind(Commands::whois, sv, this, std::placeholders::_1, std::placeholders::_2)},
		{"stealth", std::bind(Commands::stealth, sv, this, std::placeholders::_1, std::placeholders::_2)},
		{"mute", std::bind(Commands::mute, sv, this, std::placeholders::_1, std::placeholders::_2)},
		{"restrict", std::bind(Commands::restrict, sv, this, std::placeholders::_1, std::placeholders::_2)},
		{"getid", std::bind(Commands::getid, sv, this, std::placeholders::_1, std::placeholders::_2)},
		{"kick", std::bind(Commands::kick, sv, this, std::placeholders::_1, std::placeholders::_2)},
		{"lock", std::bind(Commands::lock, sv, this, std::placeholders::_1, std::placeholders::_2)},
		{"ids", std::bind(Commands::ids, sv, this, std::placeholders::_1, std::placeholders::_2)}
	};

	admincmds = {
		{"save", std::bind(Commands::save, sv, this, std::placeholders::_1, std::placeholders::_2)},
		{"kickall", std::bind(Commands::kickall, sv, this, std::placeholders::_1, std::placeholders::_2)},
		{"setprop", std::bind(Commands::setprop, sv, this, std::placeholders::_1, std::placeholders::_2)},
		{"getprop", std::bind(Commands::getprop, sv, this, std::placeholders::_1, std::placeholders::_2)},
		{"reload", std::bind(Commands::reload, sv, this, std::placeholders::_1, std::placeholders::_2)},
		{"lockdown", std::bind(Commands::lockdown, sv, this, std::placeholders::_1, std::placeholders::_2)},
		{"bansuspicious", std::bind(Commands::bansuspicious, sv, this, std::placeholders::_1, std::placeholders::_2)},
		{"worlds", std::bind(Commands::worlds, sv, this, std::placeholders::_1, std::placeholders::_2)},
		{"setpbucket", std::bind(Commands::setpbucket, sv, this, std::placeholders::_1, std::placeholders::_2)},
		{"doas", std::bind(Commands::doas, sv, this, std::placeholders::_1, std::placeholders::_2)},
		{"broadcast", std::bind(Commands::broadcast, sv, this, std::placeholders::_1, std::placeholders::_2)},
		{"totalonline", std::bind(Commands::totalonline, sv, this, std::placeholders::_1, std::placeholders::_2)},
		{"tellraw", std::bind(Commands::tellraw, sv, this, std::placeholders::_1, std::placeholders::_2)},
		{"sayraw", std::bind(Commands::sayraw, sv, this, std::placeholders::_1, std::placeholders::_2)}
	};
}

std::string Commands::get_cmd_list(u8 rank) const {
	std::string m("");
	switch(rank) {
		case Client::ADMIN:
			for(const auto& cmd : admincmds){
				m += cmd.first + ", ";
			}
		case Client::MODERATOR:
			for(const auto& cmd : modcmds){
				m += cmd.first + ", ";
			}
		case Client::USER:
			for(const auto& cmd : usrcmds){
				if (cmd.first == "modlogin" || cmd.first == "adminlogin") {
					continue;
				}
				m += cmd.first + ", ";
			}
			break;
		default:
			break;
	}
	if (m.size() > 2) {
		m.erase(m.end() - 2, m.end());
	}
	return m;
}

std::vector<std::string> Commands::split_args(const std::string& msg) {
	std::vector<std::string> args;
	sz_t i = 0;
	sz_t j = 0;
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
	std::unordered_map<std::string, std::function<void(Client * const, const std::vector<std::string>&)>>::const_iterator search;
	if(args.size() > 0){
		switch(cl->get_rank()) {
			case Client::ADMIN:
				search = admincmds.find(args[0]);
				if(search != admincmds.end()){
					std::cout << "Admin: " << cl->id << " (" << cl->get_world()->name
					<< ", " << cl->si->ip << ") Executed: " << msg << std::endl;
					break;
				}
			case Client::MODERATOR:
				search = modcmds.find(args[0]);
				if(search != modcmds.end()){
					std::cout << "Mod: " << cl->id << " (" << cl->get_world()->name
					<< ", " << cl->si->ip << ") Executed: " << msg << std::endl;
					break;
				}
			case Client::USER:
				search = usrcmds.find(args[0]);
				if (search != usrcmds.end()) {
					break;
				}
			default:
				if (args[0] == "modlogin" || args[0] == "adminlogin" || args[0] == "pass") {
					search = usrcmds.find(args[0]);
					if (search != usrcmds.end()) {
						break;
					}
				}
				return false;

		}
		search->second(cl, args);
		return true;
	}
	return false;
}

void Commands::pass(Server * const sv, const Commands * const cmd,
			Client * const cl, const std::vector<std::string>& args) {
	if (args.size() > 1) {
		std::string value = std::string(args[1]);
		for (sz_t i = 2; i < args.size(); i++) {
			value.append(" " + args[i]);
		}
		World * const w = cl->get_world();
		if (w->is_pass(value)) {
			cl->promote(w->get_default_rank(), w->get_paintrate());
		} else {
			cl->disconnect();
		}
	} else {
		cl->tell("Use to unlock drawing on a protected world.");
		cl->tell("Usage: /pass WORLDPASSWORD");
	}
}

void Commands::modlogin(Server * const sv, const Commands * const cmd,
			Client * const cl, const std::vector<std::string>& args) {
	if(!cl->is_mod() && args.size() == 2){
		if(sv->is_modpw(args[1])) {
			if (!cl->get_world()->mods_enabled()) {
				cl->tell("Sorry, but moderators are disabled on this world.");
				return;
			}
			std::string msg("DEV" + std::to_string(cl->id) + " (" + cl->get_world()->name + ", " + cl->si->ip + ") Got mod");
			std::cout << msg << std::endl;
			sv->admintell(msg);
			cl->promote(Client::MODERATOR, cl->get_world()->get_paintrate());
		} else {
			/* One try */
			cl->disconnect();
		}
	}
}

void Commands::adminlogin(Server * const sv, const Commands * const cmd,
			Client * const cl, const std::vector<std::string>& args) {
	if(!cl->is_admin() && args.size() == 2){
		if(sv->is_adminpw(args[1])){
			std::cout << "User: " << cl->id << " (" << cl->get_world()->name
					<< ", " << cl->si->ip << ") Got admin!" << std::endl;
			cl->promote(Client::ADMIN, cl->get_world()->get_paintrate());
		} else {
			/* One try */
			cl->disconnect();
		}
	}
}

void Commands::kickip(Server * const sv, const Commands * const cmd,
			Client * const cl, const std::vector<std::string>& args) {
	if (args.size() == 2) {
		sv->kickip(args[1]);
	} else {
		cl->tell("Usage: /kickip IP");
	}
}

void Commands::banip(Server * const sv, const Commands * const cmd,
			Client * const cl, const std::vector<std::string>& args) {
	sv->clearexpbans();
	if (args.size() == 3) {
		i64 t = -1;
		try {
			t = stol(args[2]);
		} catch(std::invalid_argument& e) {
			return;
		} catch(std::out_of_range& e) {
			return;
		}
		if (t >= 0) {
			t *= 1000;
			t *= 60;
			t += js_date_now();
		}
		sv->banip(args[1], t);
	} else {
		cl->tell("Usage: /banip IP MINUTES (-1 for infinite)");
	}
}

void Commands::getprop(Server * const sv, const Commands * const cmd,
			Client * const cl, const std::vector<std::string>& args) {
	if (args.size() == 2) {
		std::string v(cl->get_world()->getProp(args[1], "[Empty]"));
		cl->tell("World property: '" + args[1] + "' is '" + v + "'");
	}
}

void Commands::setprop(Server * const sv, const Commands * const cmd,
			Client * const cl, const std::vector<std::string>& args) {
	if (args.size() > 2) {
		std::string value = std::string(args[2]);
		for (sz_t i = 3; i < args.size(); i++) {
			value.append(" " + args[i]);
		}
		cl->get_world()->setProp(args[1], value);
		cl->tell("DEVSet world's property key '" + args[1] + "' to '" + value + "'");
	} else if (args.size() == 2) {
		cl->get_world()->setProp(args[1], "");
		cl->tell("DEVDeleted world's property key '" + args[1] + "'");
	} else {
		cl->tell("Usage: /setprop KEY [VALUE]");
	}
}

void Commands::reload(Server * const sv, const Commands * const cmd,
			Client * const cl, const std::vector<std::string>& args) {
	cl->get_world()->reload();
	cl->tell("World properties reloaded.");
}

void Commands::help(Server * const sv, const Commands * const cmd,
			Client * const cl, const std::vector<std::string>& args) {
	const std::string m(cmd->get_cmd_list(cl->get_rank()));
	cl->tell("Server: " + m + ".");
}

void Commands::getid(Server * const sv, const Commands * const cmd,
			Client * const cl, const std::vector<std::string>& args) {
	if (args.size() > 1) {
		std::string name = std::string(args[1]);
		for (sz_t i = 2; i < args.size(); i++) {
			name.append(" " + args[i]);
		}
		Client * const c = cl->get_world()->get_cli(name);
		if (c) {
			cl->tell("ID: " + std::to_string(c->id));
		} else {
			cl->tell("User not found!");
		}
	} else {
		cl->tell("Gets the id by nick. Probably useless because of HTML nicks or invisible characters. (might improve (lol jk, it will stay like this (probably (but not sure tho))))");
		cl->tell("Usage: /getid NICKNAME");
	}
}

void Commands::teleport(Server * const sv, const Commands * const cmd,
			Client * const cl, const std::vector<std::string>& args) {
	if(args.size() == 3){
		i32 x = 0;
		i32 y = 0;
		try {
			x = stoi(args[1]);
			y = stoi(args[2]);
		} catch(std::invalid_argument& e) {
			return;
		} catch(std::out_of_range& e) {
			return;
		}
		cl->tell("Server: Teleported to X: " + std::to_string(x) + ", Y: " + std::to_string(y));
		cl->teleport(x, y);
	} else if (args.size() == 2) {
		u32 id = 0;
		try {
			id = stoul(args[1]);
		} catch(std::invalid_argument& e) {
			return;
		} catch(std::out_of_range& e) {
			return;
		}
		Client * const target = cl->get_world()->get_cli(id);
		if(target) {
			const pinfo_t * pos = target->get_pos();
			cl->teleport(pos->x >> 4, pos->y >> 4);
		}
	} else if (args.size() == 4) {
		u32 id = 0;
		i32 x = 0;
		i32 y = 0;
		try {
			id = stoul(args[1]);
			x = stoi(args[2]);
			y = stoi(args[3]);
		} catch(std::invalid_argument& e) {
			return;
		} catch(std::out_of_range& e) {
			return;
		}
		Client * const target = cl->get_world()->get_cli(id);
		if (target) {
			pinfo_t const * const p = target->get_pos();
			i32 lx = p->x >> 4;
			i32 ly = p->y >> 4;
			target->teleport(x, y);
			cl->tell("Server: Teleported " + std::to_string(id) + " from " + std::to_string(lx) + ", " + std::to_string(ly) + " to " + std::to_string(x) + ", " + std::to_string(y));
		}
	} else {
		cl->tell("Usage: /tp [ID] X Y or /tp ID");
	}
}

void Commands::restrict(Server * const sv, const Commands * const cmd,
			Client * const cl, const std::vector<std::string>& args) {
	if (args.size() == 2) {
		u8 newstate = args[1] == "true" ? Client::NONE : Client::USER;
		cl->get_world()->set_default_rank(newstate);
		std::string msg("Draw restriction is " + (newstate == Client::NONE ? std::string("ON") : std::string("OFF")));
		cl->tell(msg);
	} else {
		cl->tell("Restricts drawing to all NEW users in this world. (manually grant with /setrank (id) 1");
		cl->tell("Usage: /restrict (true/false)");
	}
}

void Commands::setrank(Server * const sv, const Commands * const cmd,
			Client * const cl, const std::vector<std::string>& args) {
	if(args.size() == 3){
		u32 id = 0;
		u32 rank = 0;
		try {
			id = stoul(args[1]);
			rank = stoul(args[2]);
		} catch(std::invalid_argument& e) {
			return;
		} catch(std::out_of_range& e) {
			return;
		}
		Client * const target = cl->get_world()->get_cli(id);
		if(target && target->get_rank() < cl->get_rank() && rank < cl->get_rank()) {
			target->promote(rank, cl->get_world()->get_paintrate());
			cl->tell("Set user's (" + std::to_string(id) + ") rank to: " + std::to_string(rank));
		} else if (target && target->get_rank() >= cl->get_rank()) {
			cl->tell("Error: Target's rank must be less than yours.");
		} else if (target && rank >= cl->get_rank()) {
			cl->tell("Error: Rank set must be less than your current rank. (" + std::to_string(rank) + " >= " + std::to_string(cl->get_rank()) + ")");
		} else {
			cl->tell("Error: User does not exist");
		}
	} else {
		cl->tell("Usage: /setrank (id) (rank [0: NONE, 1: USER, 2: MODERATOR, 3: ADMIN])");
	}
}

void Commands::kick(Server * const sv, const Commands * const cmd,
			Client * const cl, const std::vector<std::string>& args) {
	if(args.size() == 2){
		u32 id = 0;
		try {
			id = stoul(args[1]);
		} catch(std::invalid_argument& e) {
			return;
		} catch(std::out_of_range& e) {
			return;
		}
		Client * const target = cl->get_world()->get_cli(id);
		if(target && target->get_rank() < cl->get_rank()) {
			target->teleport(0, 0);
			target->disconnect();
			cl->tell("Kicked user (" + std::to_string(id) + ")");
		} else if (target && target->get_rank() >= cl->get_rank()) {
			cl->tell("Error: Target's rank must be less than yours.");
		} else {
			cl->tell("Error: User does not exist");
		}
	}
}

void Commands::worlds(Server * const sv, const Commands * const cmd,
			Client * const cl, const std::vector<std::string>& args) {
	//if (args.size() == 1) {
		cl->tell("Currently loaded worlds:");
		for (auto & s : sv->worlds) {
			cl->tell("-> " + s.first + " [" + std::to_string(s.second->get_pl()->size()) + "]");
		}
	//} else {
		//cl->tell("Usage: /worlds list");
	//}
}

void Commands::kickall(Server * const sv, const Commands * const cmd,
			Client * const cl, const std::vector<std::string>& args) {
	if(args.size() == 2){
		if(args[1] == "world") {
			World * const target = cl->get_world();
			sv->kickall(target);
		} else if(args[1] == "all") {
			sv->kickall();
		}
	} else {
		cl->tell("Kicks everyone from the world or all the server, except admins.");
		cl->tell("Usage: /kickall (world, all)");
	}
}

void Commands::lock(Server * const sv, const Commands * const cmd,
			Client * const cl, const std::vector<std::string>& args) {
	if(args.size() == 3){
		if (args[1] == "ip") {
			u32 ipcnt32 = 0;
			u8 ipcnt = 0;
			const u8 limit = cl->is_admin() ? 255 : 10;
			try {
				ipcnt32 = stoul(args[2]);
				ipcnt = ipcnt32 >= limit ? limit : ipcnt32 <= 1 ? 1 : ipcnt32;
			} catch(std::invalid_argument& e) {
				return;
			} catch(std::out_of_range& e) {
				return;
			}
			sv->set_max_ip_conns(ipcnt);
			cl->tell("Set max connections per ip: " + std::to_string(ipcnt));
		} else if(args[1] == "captcha") {
			bool new_state = args[2] == "true" ? true : false;
			sv->set_captcha_protection(new_state);
			cl->tell("Captcha protection " + (new_state ? std::string("enabled.") : std::string("disabled.")));
		} /*else if (args[1] == "proxy") {
			bool new_state = args[2] == "true" ? true : false;
			sv->set_proxycheck(new_state);
			cl->tell("Proxy protection " + (new_state ? std::string("enabled.") : std::string("disabled.")));
		} */else if (args[1] == "trustwhitelist") {
			bool new_state = args[2] == "true" ? true : false;
			sv->trusting_captcha = new_state;
			cl->tell("Trusting whitelist " + (new_state ? std::string("enabled.") : std::string("disabled.")));
		} else if (args[1] == "fastconnects") {
			u8 new_state = args[2] == "captcha" ? 3 : args[2] == "ban" ? 2 : args[2] == "kick" ? 1 : 0;
			sv->fastconnectaction = new_state;
			const char * tellmsg[4] = {"allowing", "kicking", "banning", "asking for captcha on"};
			cl->tell("Now " + std::string(tellmsg[new_state]) + " fast connections.");
		}
	} else {
		cl->tell("Usage: /lock (ip/captcha/trustwhitelist/fastconnects) [num_conns/(true,false)/<-/(captcha, ban, kick, none)]");
	}
}

void Commands::lockdown(Server * const sv, const Commands * const cmd,
			Client * const cl, const std::vector<std::string>& args) {
	if(args.size() == 2) {
		bool new_state = args[1] == "true" ? true : false;
		sv->set_lockdown(new_state);
	} else {
		cl->tell("Only allows players with whitelisted IPs to connect.");
		cl->tell("Admin IPs are automatically added to the whitelist (careful, don't change your ip while this is on).");
		cl->tell("If all whitelisted admins disconnect, this is automatically disabled.");
		cl->tell("Usage: /lockdown true/false");
	}
}

void Commands::whois(Server * const sv, const Commands * const cmd,
			Client * const cl, const std::vector<std::string>& args) {
	if(args.size() == 2){
		u32 id = 0;
		try {
			id = stoul(args[1]);
		} catch(std::invalid_argument& e) {
			return;
		} catch(std::out_of_range& e) {
			return;
		}
		Client * const target = cl->get_world()->get_cli(id);
		if(target){
			cl->tell("Client information for: " + std::to_string(id));
			cl->tell("-> Connections by this IP: " + std::to_string(sv->get_conns(target->si->ip)));
			cl->tell("-> IP: " + target->si->ip);
			cl->tell("-> Origin header: " + target->si->origin);
			cl->tell("-> Warning level: " + std::to_string(target->get_penalty()));
			cl->tell("-> Rank: " + std::to_string(target->get_rank()));
			//cl->tell("-> User agent: " + target->si->ua);
		}
	} else {
		cl->tell("Usage: /whois (Player ID)");
	}
}

void Commands::stealth(Server * const sv, const Commands * const cmd,
			Client * const cl, const std::vector<std::string>& args) {
	if(args.size() == 2){
		bool newstate = args[1] == "true" ? true : false;
		cl->set_stealth(newstate);
		cl->tell("Stealth mode " + (newstate ? std::string("enabled.") : std::string("disabled.")));
	} else {
		cl->tell("Removes the (A)/(M) from chat (you need to remove your nickname).");
		cl->tell("Usage: /stealth (true, false)");
	}
}

void Commands::ids(Server * const sv, const Commands * const cmd,
			Client * const cl, const std::vector<std::string>& args) {
	World * const w = cl->get_world();
	std::set<Client *> * const plset = w->get_pl();
	std::string str("Total: " + std::to_string(plset->size()) + "; ");
	for (auto c : *plset) {
		str += std::to_string(c->id) + ", ";
	}
	cl->tell(str);
}

void Commands::nick(Server * const sv, const Commands * const cmd,
			Client * const cl, const std::vector<std::string>& args) {
	if (args.size() >= 2) {
		std::string name = std::string(args[1]);
		for (sz_t i = 2; i < args.size(); i++) {
			name.append(" " + args[i]);
		}
		name.erase(std::remove(name.begin(), name.end(), '\n'), name.end());
		sz_t size = getUTF8strlen(name);
		if (name.size() < 1) return;
		if (cl->is_admin()) {
			cl->set_nick(name);
		} else if (cl->is_mod() && size <= 40) {
			cl->set_nick("(M) " + name);
		} else if (size <= 12) {
			cl->set_nick("[" + std::to_string(cl->id) + "] " + name);
		} else {
			cl->tell("Nickname too long! (Max: " + std::to_string(cl->is_mod() ? 40 : 12) + ")");
			return;
		}
		cl->tell("Nickname set to: '" + name + "'");
	} else {
		cl->set_nick("");
		cl->tell("Nickname reset.");
	}
}

void Commands::tell(Server * const sv, const Commands * const cmd,
			Client * const cl, const std::vector<std::string>& args) {
	if (args.size() >= 3) {
		u32 id = 0;
		try {
			id = stoul(args[1]);
		} catch(std::invalid_argument& e) {
			return;
		} catch(std::out_of_range& e) {
			return;
		}
		std::string msg = std::string(args[2]);
		for (sz_t i = 3; i < args.size(); i++) {
			msg.append(" " + args[i]);
		}
		Client * const target = cl->get_world()->get_cli(id);
		if(target) {
			cl->tell("-> You tell " + std::to_string(target->id) + ": " + msg);
			target->tell("-> " + std::to_string(cl->id) + " tells you: " + msg);
		} else {
			cl->tell("User not found!");
		}
	} else {
		cl->tell("Usage: /tell (id) (message)");
	}
}

void Commands::bans(Server * const sv, const Commands * const cmd,
			Client * const cl, const std::vector<std::string>& args) {
	sv->clearexpbans();
	auto * banarr = sv->getbans();
	if (args.size() == 2) {
		i64 now = js_date_now();
		if (args[1] == "list") {
			for (auto & ip : *banarr) {
				cl->tell("-> " + ip.first + "(" + std::to_string(ip.second < 0 ? -1 : (ip.second - now) / 1000) + ")");
			}
			cl->tell("Total: " + std::to_string(banarr->size()));
		} else if (args[1] == "clear") {
			banarr->clear();
			cl->tell("Cleared all bans.");
		}
	} else if (args.size() >= 3) {
		if (args[1] == "add") {
			try {
				std::string time(args.size() == 3 ? "-1" : args[3]);
				sv->banip(args[2], std::stol(time));
			} catch (std::invalid_argument& e) { cl->tell("Invalid number!"); }
			catch(std::out_of_range& e) { cl->tell("Number out of range"); }
		} else if (args[1] == "remove") {
			if (banarr->erase(args[2])) {
				cl->tell("Unbanned IP: " + args[2]);
			}
		}
	} else {
		cl->tell("Usage: /bans (list, clear, add, remove) [IP] [UNIX_TIME]");
	}
}

void Commands::bansuspicious(Server * const sv, const Commands * const cmd,
			Client * const cl, const std::vector<std::string>& args) {
	if (args.size() == 2) {
		bool newstate = args[1] == "true" ? true : false;
		sv->set_instaban(newstate);
		cl->tell("Suspicious banning mode " + (newstate ? std::string("enabled.") : std::string("disabled.")));
	} else {
		cl->tell("Instantly bans any connection with a non correct origin. Could be dangerous?");
		cl->tell("Usage: /bansuspicious (true, false)");
	}
}

void Commands::whitelist(Server * const sv, const Commands * const cmd,
			Client * const cl, const std::vector<std::string>& args) {
	auto * whitelistarr = sv->getwhitelist();
	if (args.size() == 2) {
		if (args[1] == "list") {
			for (auto & ip : *whitelistarr) {
				cl->tell("-> " + ip);
			}
			cl->tell("Total: " + std::to_string(whitelistarr->size()));
		} else if (args[1] == "clear") {
			whitelistarr->clear();
			cl->tell("Cleared all whitelists.");
		}
	} else if (args.size() == 3) {
		if (args[1] == "add") {
			whitelistarr->emplace(args[2]);
			cl->tell("Whitelisted IP: " + args[2]);
		} else if (args[1] == "remove") {
			if (whitelistarr->erase(args[2])) {
				cl->tell("Unwhitelisted IP: " + args[2]);
			}
		}
	} else {
		cl->tell("Usage: /whitelist (list, clear, add, remove) [IP]");
	}
}

void Commands::blacklist(Server * const sv, const Commands * const cmd,
			Client * const cl, const std::vector<std::string>& args) {
	auto * blacklistarr = sv->getblacklist();
	if (args.size() == 2) {
		if (args[1] == "list") {
			for (auto & ip : *blacklistarr) {
				cl->tell("-> " + ip);
			}
			cl->tell("Total: " + std::to_string(blacklistarr->size()));
		} else if (args[1] == "clear") {
			blacklistarr->clear();
			cl->tell("Cleared all blacklists.");
		}
	} else if (args.size() == 3) {
		if (args[1] == "add") {
			blacklistarr->emplace(args[2]);
			cl->tell("Blacklisted IP: " + args[2]);
		} else if (args[1] == "remove") {
			if (blacklistarr->erase(args[2])) {
				cl->tell("Unblacklisted IP: " + args[2]);
			}
		}
	} else {
		cl->tell("Usage: /blacklist (list, clear, add, remove) [IP]");
	}
}

void Commands::mute(Server * const sv, const Commands * const cmd,
			Client * const cl, const std::vector<std::string>& args) {
	if (args.size() == 3) {
		u32 id = 0;
		try {
			id = stoul(args[1]);
		} catch(std::invalid_argument& e) {
			return;
		} catch(std::out_of_range& e) {
			return;
		}
		Client * const target = cl->get_world()->get_cli(id);
		bool newstate = args[2] == "1";
		if (target) {
			if (target->get_rank() >= cl->get_rank()) {
				cl->tell("Error: Target's rank must be less than yours.");
			} else {
				target->mute = newstate;
				cl->tell((newstate ? std::string("Muted ") : std::string("Unmuted ")) + std::to_string(id));
			}
		}
	} else {
		cl->tell("Usage: /mute (id) (1/0)");
	}
}

void Commands::doas(Server * const sv, const Commands * const cmd,
			Client * const cl, const std::vector<std::string>& args) {
	if (args.size() >= 3) {
		u32 id = 0;
		try {
			id = stoul(args[1]);
		} catch(std::invalid_argument& e) {
			return;
		} catch(std::out_of_range& e) {
			return;
		}
		std::string msg = std::string(args[2]);
		for (sz_t i = 3; i < args.size(); i++) {
			msg.append(" " + args[i]);
		}
		Client * const target = cl->get_world()->get_cli(id);
		if (target) {
			sv->cmds.exec(target, msg);
		}
	} else {
		cl->tell("Execute commands as another user. (without the initial '/')");
		cl->tell("Usage: /doas ID COMMAND");
	}
}

/* Also TODO: replace all \n with \0 in props.txt server side */
void Commands::setpbucket(Server * const sv, const Commands * const cmd,
			Client * const cl, const std::vector<std::string>& args) {
	if (args.size() == 4) {
		u32 id = 0;
		u32 rate = 0;
		u32 per = 0;
		try {
			id = stoul(args[1]);
			rate = stoul(args[2]);
			per = stoul(args[3]);
		} catch(std::invalid_argument& e) {
			return;
		} catch(std::out_of_range& e) {
			return;
		}
		per = per > 0xFFFF ? 0xFFFF : per == 0 ? 1 : per;
		rate = rate > 0xFFFF ? 0xFFFF : rate;
		Client * const target = cl->get_world()->get_cli(id);
		if (target) {
			target->set_pbucket(rate, per);
			cl->tell("Set client's place bucket: (" + std::to_string(rate) + ", " + std::to_string(per) + ")");
		} else {
			cl->tell("Client not found!");
		}
	} else {
		cl->tell("Set a client's drawing limit. (RATE pixels every PER seconds, no floats allowed)");
		cl->tell("Usage: /setpbucket ID RATE PER");
	}
}

void Commands::broadcast(Server * const sv, const Commands * const cmd,
			Client * const cl, const std::vector<std::string>& args) {
	if (args.size() >= 2) {
		std::string msg = std::string(args[1]);
		for (sz_t i = 2; i < args.size(); i++) {
			msg.append(" " + args[i]);
		}
		sv->broadcastmsg(msg);
	} else {
		cl->tell("Broadcasts a message to all connections.");
	}
}

void Commands::totalonline(Server * const sv, const Commands * const cmd,
			Client * const cl, const std::vector<std::string>& args) {
	cl->tell("Total connections to the server: " + std::to_string(sv->connsws.size()));
}

void Commands::tellraw(Server * const sv, const Commands * const cmd,
			Client * const cl, const std::vector<std::string>& args) {
	if (args.size() >= 3) {
		u32 id = 0;
		try {
			id = stoul(args[1]);
		} catch(std::invalid_argument& e) {
			return;
		} catch(std::out_of_range& e) {
			return;
		}
		std::string msg = std::string(args[2]);
		for (sz_t i = 3; i < args.size(); i++) {
			msg.append(" " + args[i]);
		}
		Client * const target = cl->get_world()->get_cli(id);
		if(target) {
			cl->tell("Message sent.");
			target->tell(msg);
		} else {
			cl->tell("User not found!");
		}
	} else {
		cl->tell("Displays a message to a user as raw text.");
		cl->tell("Usage: /tellraw (id) (message)");
	}
}

void Commands::sayraw(Server * const sv, const Commands * const cmd,
			Client * const cl, const std::vector<std::string>& args) {
	if (args.size() >= 2) {
		std::string msg = std::string(args[1]);
		for (sz_t i = 2; i < args.size(); i++) {
			msg.append(" " + args[i]);
		}

		cl->get_world()->broadcast(msg);
	} else {
		cl->tell("Send a message as raw text.");
		cl->tell("Usage: /sayraw (message)");
	}
}

void Commands::save(Server * const sv, const Commands * const cmd,
			Client * const cl, const std::vector<std::string>& args) {
	sv->save_now();
}
