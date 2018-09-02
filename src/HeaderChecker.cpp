#include "HeaderChecker.hpp"

#include <algorithm>

#include <ConnectionManager.hpp>

#include <uWS.h>

HeaderChecker::HeaderChecker(std::vector<std::string> v)
: acceptedOrigins(std::move(v)) { }

bool HeaderChecker::preCheck(IncomingConnection& ic, uWS::HttpRequest& req) {
	uWS::Header o = req.getHeader("origin", 6);
	if (!o || std::find(acceptedOrigins.begin(), acceptedOrigins.end(), o.toString()) == acceptedOrigins.end()) {
		return false;
	}

	return true;
}
