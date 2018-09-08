#pragma once

#include <map>
#include <string>
#include <vector>
#include <memory>

#include <nlohmann/json_fwd.hpp>

class Command;
class Server;
class Player;

class CommandManager {
	std::map<std::string, std::vector<std::unique_ptr<Command>>> commandsByName;
	Server& s;

public:
	CommandManager(Server&);

	template<typename CommandType, typename... Args>
	void add(Args&&... args);

	bool exec(Player&, std::string, nlohmann::json);
	void sendAvailableCommands(Player&);

private:
	Command& getCorrectCommandOverload(const std::vector<std::unique_ptr<Command>>&, const nlohmann::json&);
};

#include "CommandManager.tpp"
