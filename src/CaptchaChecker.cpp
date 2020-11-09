#include "CaptchaChecker.hpp"

#include <Session.hpp>
#include <User.hpp>
#include <UviasRank.hpp>
#include <ConnectionManager.hpp>

#include <RecaptchaRestApi.hpp>
#include <HttpData.hpp>

#include <nlohmann/json.hpp>

static void to_json(nlohmann::json& j, const CaptchaChecker::State s) { // @suppress("Unused static function")
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

CaptchaChecker::CaptchaChecker(RecaptchaRestApi& rcra)
: rcra(rcra),
  state(State::GUESTS) { }

void CaptchaChecker::setState(State s) {
	state = s;
}

bool CaptchaChecker::isCaptchaEnabledFor(IncomingConnection& ic) {
	return state == State::ALL || (state == State::GUESTS && ic.ci.session->getUser().getUviasRank().getName() == "guests");
}

bool CaptchaChecker::isAsync(IncomingConnection& ic) {
	return isCaptchaEnabledFor(ic);
}

bool CaptchaChecker::preCheck(IncomingConnection& ic, HttpData) {
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
	rcra.check(ic.ip, ic.args.at("captcha"), [this, &ic, end{std::move(cb)}] (auto res, auto) {
		// if request OK and token verified, continue
		end(res && *res);
	});
}

nlohmann::json CaptchaChecker::getPublicInfo() {
	return state;
}
