#pragma once

#include <unordered_map>
#include <functional>
#include <vector>
#include <string>
#include <cstdint>

namespace uWS {
	struct HttpResponse;
}

class ApiProcessor {
public:
	enum Status : std::uint8_t {
		OK = 0,
		INVALID_ARGS,
		INT_ERROR,
		UNK_REQ
	};

	using ArgList = std::vector<std::string>;
	using Func = std::function<Status(uWS::HttpResponse *, ArgList)>;

private:
	std::unordered_map<std::string, Func> methods;

public:
	ApiProcessor();

	void set(std::string, Func);
	Status exec(uWS::HttpResponse *, ArgList);
};