#pragma once

#include <string>

#include <misc/explints.hpp>

#include <nlohmann/json_fwd.hpp>

class Ipv4 {
	u32 address;

public:
	Ipv4();
	Ipv4(const char *); // string -> u32 conv.
	Ipv4(u32);

	u32 get() const;
	std::string toString() const;

	bool operator ==(const Ipv4&) const;
	bool operator  <(const Ipv4&) const;
};

void to_json(nlohmann::json&, const Ipv4&);
void from_json(const nlohmann::json&, Ipv4&);
