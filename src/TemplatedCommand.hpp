#pragma once

#include <tuple>
#include <string>
#include <utility>

#include <nlohmann/json_fwd.hpp>

#include <Command.hpp>

class Server;
class Player;

template<typename... Args>
class TemplatedCommand : Command {
	using Tuple = std::tuple<Args...>;

public:
	TemplatedCommand(Server& s, std::string name, std::string desc);

	virtual bool checkArguments(const nlohmann::json&);
	Result doExec(Player&, nlohmann::json);

private:
	virtual Result exec(Player&, Args...) = 0;
	template<std::size_t... I>
	Result reallyExec(Player&, Tuple, std::index_sequence<I...>);
};

#include "TemplatedCommand.tpp"
