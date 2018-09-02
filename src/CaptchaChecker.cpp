#include "CaptchaChecker.hpp"

#include <ConnectionManager.hpp>
#include <keys.hpp>

#include <misc/AsyncHttp.hpp>

#include <iostream>

CaptchaChecker::CaptchaChecker(AsyncHttp& hcli)
: hcli(hcli),
  enabledForGuests(true),
  enabledForEveryone(false) { }

void CaptchaChecker::enableForGuests() {
	enabledForGuests = true;
}

void CaptchaChecker::enableForEveryone() {
	enabledForEveryone = true;
}

void CaptchaChecker::disable() {
	enabledForGuests = false;
	enabledForEveryone = false;
}

bool CaptchaChecker::isCaptchaEnabledFor(IncomingConnection& ic) {
	return enabledForEveryone || (ic.ci.ui.isGuest && enabledForGuests);
}

bool CaptchaChecker::isAsync(IncomingConnection& ic) {
	return isCaptchaEnabledFor(ic);
}

bool CaptchaChecker::preCheck(IncomingConnection& ic, uWS::HttpRequest&) {
	if (!isCaptchaEnabledFor(ic)) {
		return true;
	}

	auto search = ic.args.find("captcha");
	if (search == ic.args.end() || search->second.size() > 2048) {
		return false;
	}

	return true;
}

void CaptchaChecker::asyncCheck(IncomingConnection& ic, std::function<void(bool)> cb) {
	hcli.addRequest("https://www.google.com/recaptcha/api/siteverify", {
		{"secret", CAPTCHA_API_KEY},
		{"remoteip", ic.ip},
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
	return {
		{ "enabledForGuests", enabledForGuests },
		{ "enabledForEveryone", enabledForEveryone }
	};
}
