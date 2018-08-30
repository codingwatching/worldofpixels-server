#include <Command.hpp>

template<typename CommandType, typename... Args>
void CommandManager::add(Args&&... args) {
	std::unique_ptr<Command> cmd(std::make_unique<CommandType>(s, std::forward<Args>(args)...));
	auto search = commandsByName.find(cmd->getName());
	if (search == commandsByName.end()) {
		commandsByName.emplace(cmd->getName(), decltype(commandsByName)::mapped_type{std::move(cmd)});
	} else {
		search->second.emplace_back(std::move(cmd));
	}
}
