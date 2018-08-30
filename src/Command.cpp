#include "Command.hpp"

#include <Player.hpp>

void to_json(nlohmann::json& j, const Result& r) {
	j = {
		{ "success", r.success },
		{ "data", r.data }
	};
}

Result::Result(bool s)
: success(s) { }

Result::Result(nlohmann::json j)
: success(true),
  data(std::move(j)) { }

Result::Result(nlohmann::json j, bool s)
: success(s),
  data(std::move(j)) { }

Command::Command(Server& s, std::string name, std::string desc,
	std::vector<std::type_index> typ)
: name(std::move(name)),
  description(std::move(desc)),
  expectedArgumentTypes(std::move(typ)),
  s(s) { }

Command::~Command() = default;

const std::string& Command::getName() const {
	return name;
}

const std::string& Command::getDescription() const {
	return description;
}

const std::vector<std::type_index> Command::getArgumentTypes() const {
	return expectedArgumentTypes;
}

bool Command::hasPermission(const Player& p) const {
	return p.canUseCommands();
}
