#include "ProxyChecker.hpp"

#include <ConnectionManager.hpp>
#include <Session.hpp>
#include <keys.hpp>

#include <misc/AsyncHttp.hpp>
#include <misc/TimedCallbacks.hpp>
#include <misc/utils.hpp>

#include <iostream>

#include <nlohmann/json.hpp>

// TODO: don't check the same ip more than once each time

static void to_json(nlohmann::json& j, const ProxyChecker::State s) {
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

ProxyChecker::ProxyChecker(AsyncHttp& hcli, TimedCallbacks& tc)
: hcli(hcli),
  state(State::GUESTS) {
	tc.startTimer([this] {
		auto now(std::chrono::steady_clock::now());

		for (auto it = cache.begin(); it != cache.end();) {
			std::chrono::hours expiryInterval(it->second.isProxy ? 12 : 24);

			if (now - it->second.checkTime > expiryInterval) {
				it = cache.erase(it);
			} else {
				++it;
			}
		}

		return true;
	}, 60 * 60 * 1000);
}

void ProxyChecker::setState(State s) {
	state = s;
}

bool ProxyChecker::isCheckNeededFor(IncomingConnection& ic) {
	bool inCache = cache.find(ic.ip) != cache.end();
	return !inCache && (state == State::ALL || (ic.session.getUser().isGuest() && state == State::GUESTS));
}

bool ProxyChecker::isAsync(IncomingConnection& ic) {
	return isCheckNeededFor(ic);
}

bool ProxyChecker::preCheck(IncomingConnection& ic, uWS::HttpRequest&) {
	auto search = cache.find(ic.ip);
	return search == cache.end() || !search->second.isProxy;
}

void ProxyChecker::asyncCheck(IncomingConnection& ic, std::function<void(bool)> cb) {
	hcli.addRequest("http://proxycheck.io/v2/" + ic.ip.toString(), {
		{"key", PROXY_API_KEY},
	}, [this, &ic, end{std::move(cb)}] (auto res) {
		if (!res.successful) {
			std::cerr << "Error when checking for proxy: " << res.errorString << ", " << res.data << std::endl;
			end(!ic.session.getUser().isGuest());
			return;
		}

		std::string ip(ic.ip.toString());
		bool verified = false;
		bool isProxy = false;
		try {
			nlohmann::json j = nlohmann::json::parse(res.data);
			// std::string st(j["status"].get<std::string>());
			if (j["message"].is_string()) {
				std::cout << "Message from proxycheck.io: " << j["message"].get<std::string>() << std::endl;
			}

			if (j[ip].is_object()) {
				verified = true;
				isProxy = j[ip]["proxy"].get<std::string>() == "yes";
			}
		} catch (const std::exception& e) {
			std::cerr << "Exception when parsing json by proxycheck.io! (" << res.data << ")" << std::endl;
			std::cerr << "what(): " << e.what() << std::endl;
		}

		if (verified) {
			cache.emplace(ic.ip, ProxyChecker::Info{isProxy, std::chrono::steady_clock::now()});
		}

		end(verified ? !isProxy : !ic.session.getUser().isGuest());
	});
}

nlohmann::json ProxyChecker::getPublicInfo() {
	return state;
}
