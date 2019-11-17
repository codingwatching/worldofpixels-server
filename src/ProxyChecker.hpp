#pragma once

#include "ConnectionProcessor.hpp"

class ProxycheckioRestApi;

class ProxyChecker : public ConnectionProcessor {
public:
	enum class State { ALL, GUESTS, OFF };

private:
	struct Info;

	ProxycheckioRestApi& pcra;
	State state;

public:
	ProxyChecker(ProxycheckioRestApi&);

	void setState(State);

	bool isCheckNeededFor(IncomingConnection&);

	bool isAsync(IncomingConnection&);

	bool preCheck(IncomingConnection&, HttpData);
	void asyncCheck(IncomingConnection&, std::function<void(bool)>);

	nlohmann::json getPublicInfo();
};
