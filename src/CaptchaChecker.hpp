#pragma once

#include "ConnectionProcessor.hpp"

class AsyncHttp;

class CaptchaChecker : public ConnectionProcessor {
public:
	enum class State { ALL, GUESTS, OFF };

private:
	AsyncHttp& hcli;
	State state;

public:
	CaptchaChecker(AsyncHttp&);

	void setState(State);

	bool isCaptchaEnabledFor(IncomingConnection&);

	bool isAsync(IncomingConnection&);

	bool preCheck(IncomingConnection&, uWS::HttpRequest&);
	void asyncCheck(IncomingConnection&, std::function<void(bool)>);

	nlohmann::json getPublicInfo();
};
