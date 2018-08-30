#include "CommandManager.hpp"

#include <typeindex>

#include <nlohmann/json.hpp>

#include <misc/utils.hpp>

#include <Command.hpp>
#include <Player.hpp>

namespace nlohmann {
	template<>
	struct adl_serializer<std::type_index> {
		static void to_json(nlohmann::json& j, const std::type_index& t) {
			j = demangle(t.name());
		}
	};
}

CommandManager::CommandManager(Server& s)
: s(s) { }

bool CommandManager::exec(Player& p, std::string cmdName, nlohmann::json args) {
	Result r(false);

	auto search = commandsByName.find(cmdName);
	if (search == commandsByName.end()) {
		r.data = {
			{"error", "not_found"},
			{"reason", "Unknown command!"}
		};

		p.send(r);
		return false;
	}

	try {
		Command& cmd(getCorrectCommandOverload(search->second, args));
		if (cmd.hasPermission(p)) {
			r = cmd.doExec(p, std::move(args));
		} else {
			r.data = {
				{"error", "no_perms"},
				{"reason", "You don't have permission to do this."}
			};
		}
	} catch (const std::exception& e) {
		r.data = {
			{"error", demangle(typeid(e).name())},
			{"reason", e.what()}
		};
	}

	p.send(r);
	return r.success;
}

void CommandManager::sendAvailableCommands(Player& p) {
	nlohmann::json j = {
		{"t", "cmd_list"},
		{"cmds", nlohmann::json::array()}
	};

	for (auto& v : commandsByName) {
		nlohmann::json jsoncmd = {
			{"name", v.first},
			{"overloads", nlohmann::json::array()}
		};

		for (auto& cmd : v.second) {
			if (cmd->hasPermission(p)) {
				jsoncmd["overloads"].push_back({
					{"class_name", demangle(typeid(cmd).name())},
					{"info", cmd->getDescription()},
					{"args", cmd->getArgumentTypes()}
				});
			}
		}

		if (jsoncmd["overloads"].size() > 0) {
			j["cmds"].push_back(std::move(jsoncmd));
		}
	}

	p.send(j);
}

Command& CommandManager::getCorrectCommandOverload(const std::vector<std::unique_ptr<Command>>& v, const nlohmann::json& j) {
	for (auto& c : v) {
		if (c->checkArguments(j)) {
			return *c.get();
		}
	}

	throw std::invalid_argument("Invalid arguments for this command!");
}
