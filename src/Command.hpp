#pragma once

#include <string>
#include <vector>
#include <typeindex>
#include <nlohmann/json.hpp>

class Player;
class Server;

struct Result {
	bool success;
	nlohmann::json data;

	Result(bool);
	Result(nlohmann::json);
	Result(nlohmann::json, bool);
};

void to_json(nlohmann::json& j, const Result& r);

class Command {
	const std::string name;
	const std::string description;
	const std::vector<std::type_index> expectedArgumentTypes;
	Server& s;

public:
	Command(Server&, std::string name, std::string desc,
		std::vector<std::type_index>);
	virtual ~Command();

	const std::string& getName() const;
	const std::string& getDescription() const;
	const std::vector<std::type_index> getArgumentTypes() const;
	virtual Result doExec(Player&, nlohmann::json) = 0;
	virtual bool checkArguments(const nlohmann::json&) = 0;
	virtual bool hasPermission(const Player&) const;
};
