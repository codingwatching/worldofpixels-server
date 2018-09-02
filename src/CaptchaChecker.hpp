#pragma once

#include "ConnectionProcessor.hpp"

class AsyncHttp;

class CaptchaChecker : public ConnectionProcessor {
	AsyncHttp& hcli;
	bool enabledForGuests;
	bool enabledForEveryone;

public:
	CaptchaChecker(AsyncHttp&);

	void enableForGuests();
	void enableForEveryone();
	void disable();

	bool isCaptchaEnabledFor(IncomingConnection&);

	bool isAsync(IncomingConnection&);

	bool preCheck(IncomingConnection&, uWS::HttpRequest&);
	void asyncCheck(IncomingConnection&, std::function<void(bool)>);

	nlohmann::json getPublicInfo();
};
