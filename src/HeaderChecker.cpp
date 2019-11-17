#include "HeaderChecker.hpp"

#include <algorithm>

#include <ConnectionManager.hpp>
#include <HttpData.hpp>

HeaderChecker::HeaderChecker(std::vector<std::string> v)
: acceptedOrigins(std::move(v)) { }

bool HeaderChecker::preCheck(IncomingConnection& ic, HttpData hd) {
	auto o = hd.getHeader("origin");
	if (!o || std::find(acceptedOrigins.begin(), acceptedOrigins.end(), *o) == acceptedOrigins.end()) {
		return false;
	}

	return true;
}
