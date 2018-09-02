#include "ConnectionManager.hpp"

#include <uWS.h>
#include <nlohmann/json.hpp>

#include <misc/utils.hpp>

#include <Client.hpp>

ClosedConnection::ClosedConnection(Client& c)
: ws(c.getWs()),
  ip(c.getIp()),
  wasClient(true) { }

ClosedConnection::ClosedConnection(IncomingConnection& ic)
: ws(ic.ws),
  ip(std::move(ic.ip)),
  wasClient(false) { }

ConnectionManager::ConnectionManager(uWS::Hub& h, std::string protoName) {
	h.onConnection([this, pn{std::move(protoName)}] (uWS::WebSocket<uWS::SERVER> * ws, uWS::HttpRequest req) {
		// Maybe this could be moved on the upgrade handler, somehow
		uWS::Header argHead(req.getHeader("sec-websocket-protocol", 22));
		if (!argHead) {
			ws->terminate();
			return;
		}

		auto args(tokenize(argHead.toString(), ','));
		std::map<std::string, std::string> argList;

		if (args.size() == 0 || args[0] != pn) {
			ws->terminate();
			return;
		}

		for (auto& s : args) {
			ltrim(s);
			sz_t sep = s.find_first_of('+');
			if (sep != std::string::npos) {
				argList.emplace(s.substr(0, sep), s.substr(sep + 1));
			}
		}

		auto addr = ws->getAddress();
		handleIncoming(ws, std::move(argList), req, addr.address);
	});

	h.onDisconnection([this] (uWS::WebSocket<uWS::SERVER> * ws, int c, const char * msg, sz_t len) {
		Client * cl = static_cast<Client *>(ws->getUserData());
		if (!cl) {
			// still authenticating
			auto ic = std::find_if(pending.begin(), pending.end(), [ws] (const IncomingConnection& ic) {
				return ic.ws == ws;
			});
			if (ic != pending.end()) {
				// will get deleted once the current async processor finishes
				ic->cancelled = true;
			}
		} else {
			handleDisconnect(*cl);
		}

		delete cl;
	});
}

void ConnectionManager::onSocketChecked(std::function<Client*(IncomingConnection&)> f) {
	clientTransformer = std::move(f);
}

void ConnectionManager::forEachProcessor(std::function<void(ConnectionProcessor&)> f) {
	for (auto& p : processors) {
		f(*p.get());
	}
}

void ConnectionManager::handleIncoming(uWS::WebSocket<uWS::SERVER> * ws,
		std::map<std::string, std::string> args, uWS::HttpRequest req, std::string ip) {
	auto ic = pending.emplace_back(UserInfo(), ws, std::move(args), processors.begin(), pending.end(), std::move(ip), false);
	ic->it = ic;

	for (auto it = processors.begin(); it != processors.end(); ++it) {
		if (!(*it)->preCheck(*ic, req)) {
			nlohmann::json j = {
				{"t", "auth_error"},
				{"at", "pre_check"},
				{"in", demangle(typeid(*(*it).get()))}
			};

			std::string s(j.dump());
			ws->send(s.data(), s.size(), uWS::TEXT);
			ic->nextProcessor = it;
			handleFail(*ic);
			handleDisconnect(*ic, false);
			return;
		}
	}

	handleAsync(*ic);
}

void ConnectionManager::handleAsync(IncomingConnection& ic) {
	while (ic.nextProcessor != processors.end()) {
		auto& pr = *ic.nextProcessor++;
		if (pr->isAsync(ic)) {
			{
				nlohmann::json j = {
					{"t", "auth_progress"},
					{"on", demangle(typeid(*pr.get()))}
				};
				std::string s(j.dump());
				ic.ws->send(s.data(), s.size(), uWS::TEXT);
			}
			// not safe to use ic after calling callback
			pr->asyncCheck(ic, [this, &ic, &pr] (bool ok) {
				if (ok && !ic.cancelled) {
					handleAsync(ic);
					return;
				}

				// is true if socket closed
				if (!ic.cancelled) {
					nlohmann::json j = {
						{"t", "auth_error"},
						{"at", "async_check"},
						{"in", demangle(typeid(*pr.get()))}
					};
					std::string s(j.dump());
					ic.ws->send(s.data(), s.size(), uWS::TEXT);
					handleFail(ic);
				}

				// the cancelled flag is set to true if we called handleFail
				handleDisconnect(ic, true);
			});
			return;
		}
	}

	handleEnd(ic);
}

void ConnectionManager::handleFail(IncomingConnection& ic) {
	ic.ws->close();
}

void ConnectionManager::handleEnd(IncomingConnection& ic) {
	for (auto& p : processors) {
		if (!p->endCheck(ic)) {
			nlohmann::json j = {
				{"t", "auth_error"},
				{"at", "end_check"},
				{"in", demangle(typeid(*p.get()))}
			};

			std::string s(j.dump());
			ic.ws->send(s.data(), s.size(), uWS::TEXT);
			handleFail(ic);
			handleDisconnect(ic, true);
			return;
		}
	}

	Client * cl = clientTransformer(ic);
	ic.ws->setUserData(cl);
	pending.erase(ic.it);

	for (auto& p : processors) {
		p->connected(*cl);
	}
}

void ConnectionManager::handleDisconnect(IncomingConnection& ic, bool all) {
	// don't call disconnect on processors we didn't bother (on precheck)
	const auto end = all ? processors.end() : ic.nextProcessor;

	ClosedConnection cc(ic);
	pending.erase(ic.it);
	for (auto it = processors.begin(); it != end; ++it) {
		(*it)->disconnected(cc);
	}
}

void ConnectionManager::handleDisconnect(Client& c){
	ClosedConnection cc(c);
	for (auto it = processors.begin(); it != end; ++it) {
		(*it)->disconnected(cc);
	}
}
