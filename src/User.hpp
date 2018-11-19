#pragma once

#include <string>
#include <misc/explints.hpp>

#include <nlohmann/json_fwd.hpp>

struct User {
	using Id = u64;

	const Id uid;
	std::string username;
	bool isGuest;

	User();
	User(Id, std::string);
};

void to_json(nlohmann::json&, const User&);
