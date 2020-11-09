#include "ProxyChecker.hpp"

#include <ConnectionManager.hpp>
#include <Session.hpp>
#include <User.hpp>
#include <UviasRank.hpp>

#include <ProxycheckioRestApi.hpp>
#include <HttpData.hpp>
#include <utils.hpp>

#include <nlohmann/json.hpp>

static void to_json(nlohmann::json& j, const ProxyChecker::State s) { // @suppress("Unused static function")
	switch (static_cast<int>(s)) { // it looks very nice, doesn't it?
		case static_cast<int>(ProxyChecker::State::ALL):
			j = "ALL";
			break;

		case static_cast<int>(ProxyChecker::State::GUESTS):
			j = "GUESTS";
			break;

		case static_cast<int>(ProxyChecker::State::OFF):
			j = "OFF";
			break;
	}
}

ProxyChecker::ProxyChecker(ProxycheckioRestApi& pcra)
: pcra(pcra),
  state(State::GUESTS) { }

void ProxyChecker::setState(State s) {
	state = s;
}

bool ProxyChecker::isCheckNeededFor(IncomingConnection& ic) {
	// this doesn't get the result of the cache, only if it exists (optional not empty)
	bool inCache = bool(pcra.cachedResult(ic.ip));
	if (inCache) {
		return false;
	}

	// assuming ic.ci.session is not empty
	return state == State::ALL || (state == State::GUESTS && ic.ci.session->getUser().getUviasRank().getName() == "guests");
}

bool ProxyChecker::isAsync(IncomingConnection& ic) {
	return isCheckNeededFor(ic);
}

bool ProxyChecker::preCheck(IncomingConnection& ic, HttpData) {
	// return true if not cached or isn't proxy
	return !pcra.cachedResult(ic.ip).value_or(false);
}

void ProxyChecker::asyncCheck(IncomingConnection& ic, std::function<void(bool)> cb) {
	pcra.check(ic.ip, [this, &ic, end{std::move(cb)}] (auto res, auto) {
		// if request OK and not a proxy, continue
		end(res && !*res);
	});
}

nlohmann::json ProxyChecker::getPublicInfo() {
	return state;
}
