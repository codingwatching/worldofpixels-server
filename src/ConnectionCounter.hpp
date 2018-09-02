#pragma once

#include "ConnectionProcessor.hpp"

#include <misc/explints.hpp>

class ConnectionCounter : public ConnectionProcessor {
	u32 total;
	u32 totalChecked;

	u32 currentChecking;
	u32 currentActive;

public:
	ConnectionCounter();

	bool preCheck(IncomingConnection&, uWS::HttpRequest&);

	void connected(Client&);
	void disconnected(ClosedConnection&);

	nlohmann::json getPublicInfo();
};
