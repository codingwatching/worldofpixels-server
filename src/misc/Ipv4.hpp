#pragma once

#include <misc/explints.hpp>
#include <string>

class Ipv4 {
	u32 address;

public:
	Ipv4(const char *); // string -> u32 conv.
	Ipv4(u32);

	u32 get() const;
	std::string toString() const;
};
