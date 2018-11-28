#include "CaptchaChecker.hpp"

#include <Session.hpp>
#include <User.hpp>
#include <ConnectionManager.hpp>
#include <keys.hpp>

#include <misc/AsyncHttp.hpp>

#include <iostream>

#include <nlohmann/json.hpp>

static void to_json(nlohmann::json& j, const CaptchaChecker::State s) {
	switch (static_cast<int>(s)) { // nice switch, c++
		case static_cast<int>(CaptchaChecker::State::ALL):
			j = "ALL";
			break;

		case static_cast<int>(CaptchaChecker::State::GUESTS):
			j = "GUESTS";
			break;

		case static_cast<int>(CaptchaChecker::State::OFF):
			j = "OFF";
			break;
	}
}

CaptchaChecker::CaptchaChecker(AsyncHttp& hcli)
: hcli(hcli),
  state(State::GUESTS) { }

void CaptchaChecker::setState(State s) {
	state = s;
}

bool CaptchaChecker::isCaptchaEnabledFor(IncomingConnection& ic) {
	return state == State::ALL || (ic.session.getUser().isGuest() && state == State::GUESTS);
}

bool CaptchaChecker::isAsync(IncomingConnection& ic) {
	return isCaptchaEnabledFor(ic);
}

bool CaptchaChecker::preCheck(IncomingConnection& ic, uWS::HttpRequest&) {
	if (!isCaptchaEnabledFor(ic)) {
		return true;
	}

	auto search = ic.args.find("captcha");
	if (search == ic.args.end() || search->second.size() > 4096) {
		return false;
	}

	return true;
}

void CaptchaChecker::asyncCheck(IncomingConnection& ic, std::function<void(bool)> cb) {
	hcli.addRequest("https://www.google.com/recaptcha/api/siteverify", {
		{"secret", CAPTCHA_API_KEY},
		{"remoteip", ic.ip.toString()},
		{"response", ic.args.at("captcha")}
	}, [&ic, end{std::move(cb)}] (auto res) {
		if (!res.successful) {
			/* HTTP ERROR code check */
			std::cerr << "Error occurred when verifying captcha: " << res.errorString << ", " << res.data << std::endl;
			end(false); // :thinking:
			return;
		}

		bool verified = false;
		//std::string failReason;
		try {
			nlohmann::json response = nlohmann::json::parse(res.data);
			bool success = response["success"].get<bool>();
			std::string host(response["hostname"].get<std::string>());
			verified = success && host == "ourworldofpixels.com"; // XXX: assuming hostname
			/*if (!success) {
				failReason = "API rejected token";
				if (response["error-codes"].is_array()) {
					failReason += " " + response["error-codes"].dump();
				}
			} else if (success && !verified) {
				failReason = "Wrong hostname: '" + host + "'";
			}*/
		} catch (const std::exception& e) {
			std::cerr << "Exception when parsing json by google! (" << res.data << ")" << std::endl;
			std::cerr << "what(): " << e.what() << std::endl;
		}

		end(verified);
	});
}

nlohmann::json CaptchaChecker::getPublicInfo() {
	return state;
}
