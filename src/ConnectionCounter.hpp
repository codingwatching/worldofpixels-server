#pragma once

#include "ConnectionProcessor.hpp"

#include <map>
#include <string>

#include <misc/Ipv4.hpp>
#include <misc/explints.hpp>

class ConnectionCounter : public ConnectionProcessor {
	u32 total;
	u32 totalChecked;

	u32 currentChecking;
	u32 currentActive;

	u8 maxConnsPerIp;
	std::map<Ipv4, u8> connCountPerIp;

public:
	ConnectionCounter();

	u8 getMaxConnectionsPerIp() const;
	void setMaxConnectionsPerIp(u8);

	bool preCheck(IncomingConnection&, uWS::HttpRequest&);

	void connected(Client&);
	void disconnected(ClosedConnection&);

	nlohmann::json getPublicInfo();
};
