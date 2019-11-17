#include "SessionChecker.hpp"

#include <ConnectionManager.hpp>
#include <Session.hpp>

#include <AsyncPostgres.hpp>
#include <AuthManager.hpp>
#include <HttpData.hpp>

SessionChecker::SessionChecker(AuthManager& am)
: am(am) { }

bool SessionChecker::isAsync(IncomingConnection& ic) {
	// only call async check if the session isn't set already
	return !ic.ci.session;
}

bool SessionChecker::preCheck(IncomingConnection& ic, HttpData hd) {
	auto tok = hd.getCookie("uviastoken");
	if (tok) {
		// if the session is loaded already, set it right away
		ic.ci.session = am.getSession(*tok);

		if (!ic.ci.session) {
			// store the token somewhere else, since the http data will be
			// deleted when we reach the async checks
			ic.args.insert_or_assign("uviastoken", std::string(*tok));
		}
	}

	// only continue if the uviastoken cookie is present
	return bool(tok);
}

void SessionChecker::asyncCheck(IncomingConnection& ic, std::function<void(bool)> cb) {
	auto it = ic.args.find("uviastoken");
	if (it == ic.args.end()) {
		cb(false);
		return;
	}

	auto cancel = am.loadSession(it->second, [&ic, cb{std::move(cb)}] (auto ses) {
		ic.ci.session = std::move(ses);
		// only continue if the session is valid
		cb(bool(ic.ci.session));
	});

	if (cancel) {
		ic.onDisconnect = [cancel{std::move(cancel)}] {
			// cancel the query if the socket disconnected
			cancel();
		};
	}
}
