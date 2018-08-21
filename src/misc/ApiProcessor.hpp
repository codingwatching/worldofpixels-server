#pragma once

#include <unordered_map>
#include <functional>
#include <vector>
#include <string>
#include <cstdint>

namespace uWS {
	struct HttpResponse;
	struct Hub;
}

struct RequestStorage {
	std::function<void()> onCancel;
};

class ApiProcessor {
public:
	enum Status : std::uint8_t {
		OK = 0,
		INVALID_ARGS,
		INT_ERROR,
		UNK_REQ
	};

	using ArgList = std::vector<std::string>;
	using Func = std::function<Status(uWS::HttpResponse *, RequestStorage&, ArgList)>;

private:
	std::unordered_map<std::string, Func> methods;

public:
	ApiProcessor(uWS::Hub&, std::string defaultRequest);

	void set(std::string, Func);

private:
	Status exec(uWS::HttpResponse *, RequestStorage&, ArgList);
};
