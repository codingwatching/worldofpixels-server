#pragma once

#include "ConnectionProcessor.hpp"

#include <map>

#include <misc/explints.hpp>

class AsyncHttp;
class TimedCallbacks;

class ProxyChecker : public ConnectionProcessor {
public:
	enum class State { ALL, GUESTS, OFF };

private:
	struct Info;

	AsyncHttp& hcli;
	State state;
	std::map<std::string, Info> cache;

public:
	ProxyChecker(AsyncHttp&, TimedCallbacks&);

	void setState(State);

	bool isCheckNeededFor(IncomingConnection&);

	bool isAsync(IncomingConnection&);

	bool preCheck(IncomingConnection&, uWS::HttpRequest&);
	void asyncCheck(IncomingConnection&, std::function<void(bool)>);

	nlohmann::json getPublicInfo();
};

struct ProxyChecker::Info {
	bool isProxy;
	i64 checkTime;
};
