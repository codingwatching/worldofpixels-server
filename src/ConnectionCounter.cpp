#include "ConnectionCounter.hpp"

#include <ConnectionManager.hpp>

#include <nlohmann/json.hpp>

ConnectionCounter::ConnectionCounter()
: total(0),
  totalChecked(0),
  currentChecking(0),
  currentActive(0),
  maxConnsPerIp(6) { }

u8 ConnectionCounter::getMaxConnectionsPerIp() const {
	return maxConnsPerIp;
}

void ConnectionCounter::setMaxConnectionsPerIp(u8 value) {
	maxConnsPerIp = value == 0 ? 1 : value;
}

bool ConnectionCounter::preCheck(IncomingConnection& ic, uWS::HttpRequest&) {
	++total;
	++currentActive;
	++currentChecking;

	auto search = connCountPerIp.find(ic.ip);
	if (search == connCountPerIp.end()) {
		search = connCountPerIp.emplace(ic.ip, 0).first;
	}

	return !(++search->second > maxConnsPerIp);
}

void ConnectionCounter::connected(Client&) {
	++totalChecked;
	--currentChecking;
}

void ConnectionCounter::disconnected(ClosedConnection& c) {
	--currentActive;
	if (!c.wasClient) { // didn't fully connect?
		--currentChecking;
	}

	auto search = connCountPerIp.find(c.ip);
	if (--search->second == 0) { // guaranteed to exist and > 0
		connCountPerIp.erase(search);
	}
}

nlohmann::json ConnectionCounter::getPublicInfo() {
	return {
		{"total", total},
		{"totalOk", totalChecked},
		{"currentChecking", currentChecking},
		{"currentActive", currentActive},
		{"maxConnsPerIp", maxConnsPerIp}
	};
}
