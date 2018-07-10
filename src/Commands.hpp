#pragma once

#include <unordered_map>
#include <string>
#include <vector>
#include <functional>

#include <misc/explints.hpp>

class Server;
class Client;

class Commands {
	std::unordered_map<std::string, std::function<void(Client * const, const std::vector<std::string>&)>> usrcmds;
	std::unordered_map<std::string, std::function<void(Client * const, const std::vector<std::string>&)>> modcmds;
	std::unordered_map<std::string, std::function<void(Client * const, const std::vector<std::string>&)>> admincmds;

public:
	Commands(Server * const);

	std::string get_cmd_list(u8 rank) const;

	static std::vector<std::string> split_args(const std::string&);

	bool exec(Client * const, const std::string& msg) const;

	static void pass(Server * const, const Commands * const, Client * const, const std::vector<std::string>& args);
	static void modlogin(Server * const, const Commands * const, Client * const, const std::vector<std::string>& args);
	static void adminlogin(Server * const, const Commands * const, Client * const, const std::vector<std::string>& args);

	static void getprop(Server * const, const Commands * const, Client * const, const std::vector<std::string>& args);
	static void setprop(Server * const, const Commands * const, Client * const, const std::vector<std::string>& args);
	static void reload(Server * const, const Commands * const, Client * const, const std::vector<std::string>& args);

	static void teleport(Server * const, const Commands * const, Client * const, const std::vector<std::string>& args);
	static void stealth(Server * const, const Commands * const, Client * const, const std::vector<std::string>& args);
	static void setrank(Server * const, const Commands * const, Client * const, const std::vector<std::string>& args);
	static void restrict(Server * const, const Commands * const, Client * const, const std::vector<std::string>& args);

	static void doas(Server * const, const Commands * const, Client * const, const std::vector<std::string>& args);
	static void kickip(Server * const, const Commands * const, Client * const, const std::vector<std::string>& args);
	static void save(Server * const, const Commands * const, Client * const, const std::vector<std::string>& args);
	static void setpbucket(Server * const, const Commands * const, Client * const, const std::vector<std::string>& args);
	static void banip(Server * const, const Commands * const, Client * const, const std::vector<std::string>& args);
	static void help(Server * const, const Commands * const, Client * const, const std::vector<std::string>& args);
	static void getid(Server * const, const Commands * const, Client * const, const std::vector<std::string>& args);
	static void kick(Server * const, const Commands * const, Client * const, const std::vector<std::string>& args);
	static void kickall(Server * const, const Commands * const, Client * const, const std::vector<std::string>& args);
	static void lock(Server * const, const Commands * const, Client * const, const std::vector<std::string>& args);
	static void lockdown(Server * const, const Commands * const, Client * const, const std::vector<std::string>& args);
	static void whois(Server * const, const Commands * const, Client * const, const std::vector<std::string>& args);
	static void bans(Server * const, const Commands * const, Client * const, const std::vector<std::string>& args);
	static void bansuspicious(Server * const, const Commands * const, Client * const, const std::vector<std::string>& args);
	static void whitelist(Server * const, const Commands * const, Client * const, const std::vector<std::string>& args);
	static void blacklist(Server * const, const Commands * const, Client * const, const std::vector<std::string>& args);
	static void ids(Server * const, const Commands * const, Client * const, const std::vector<std::string>& args);
	static void nick(Server * const, const Commands * const, Client * const, const std::vector<std::string>& args);
	static void tell(Server * const, const Commands * const, Client * const, const std::vector<std::string>& args);
	static void mute(Server * const, const Commands * const, Client * const, const std::vector<std::string>& args);
	static void worlds(Server * const, const Commands * const, Client * const, const std::vector<std::string>& args);

	static void broadcast(Server * const, const Commands * const, Client * const, const std::vector<std::string>& args);
	static void totalonline(Server * const, const Commands * const, Client * const, const std::vector<std::string>& args);
	static void tellraw(Server * const, const Commands * const, Client * const, const std::vector<std::string>& args);
	static void sayraw(Server * const, const Commands * const, Client * const, const std::vector<std::string>& args);
};
