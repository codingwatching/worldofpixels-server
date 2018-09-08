#pragma once

#include <string>
#include <misc/explints.hpp>

#include <nlohmann/json_fwd.hpp>

struct UserInfo {
	using Id = u64;

	const Id uid;
	std::string username;
	bool isGuest;

	UserInfo();
	UserInfo(Id, std::string);
};

void to_json(nlohmann::json&, const UserInfo&);
