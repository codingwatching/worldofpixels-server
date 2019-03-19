#pragma once

#include "ConnectionProcessor.hpp"

#include <chrono>
#include <map>

#include <explints.hpp>
#include <Ip.hpp>

class AsyncHttp;
class TimedCallbacks;

class ProxyChecker : public ConnectionProcessor {
public:
	enum class State { ALL, GUESTS, OFF };

private:
	struct Info;

	AsyncHttp& hcli;
	State state;
	std::map<Ip, Info> cache;

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
	std::chrono::steady_clock::time_point checkTime;
};
