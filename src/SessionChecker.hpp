#pragma once

#include "ConnectionProcessor.hpp"

class AuthManager;

class SessionChecker : public ConnectionProcessor {
	AuthManager& am;

public:
	SessionChecker(AuthManager&);

	bool isAsync(IncomingConnection&);

	bool preCheck(IncomingConnection&, HttpData);
	void asyncCheck(IncomingConnection&, std::function<void(bool)>);
};
