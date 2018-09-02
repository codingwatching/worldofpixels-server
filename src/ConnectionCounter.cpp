#include "ConnectionCounter.hpp"

#include <ConnectionManager.hpp>

ConnectionCounter::ConnectionCounter()
: total(0),
  totalChecked(0),
  currentChecking(0),
  currentActive(0) { }

bool ConnectionCounter::preCheck(IncomingConnection&, uWS::HttpRequest&) {
	++total;
	++currentActive;
	++currentChecking;
	return true;
}

void ConnectionCounter::connected(Client&) {
	++totalChecked;
	--currentChecking;
}

void ConnectionCounter::disconnected(ClosedConnection& c) {
	--currentActive;
	if (!c.wasClient) {
		--currentChecking;
	}
}

nlohmann::json ConnectionCounter::getPublicInfo() {
	return {
		{"total", total},
		{"totalChecked", totalChecked},
		{"currentChecking", currentChecking}
		{"currentActive", currentActive}
	};
}
