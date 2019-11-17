#include "ConnectionManager.hpp"

#include <Client.hpp>
#include <ConnectionProcessor.hpp>
#include <PacketDefinitions.hpp>

#include <utils.hpp>
#include <HttpData.hpp>

#include <iostream>

#include <uWS.h>

ClosedConnection::ClosedConnection(Client& c)
: ws(c.getWs()),
  ip(c.getIp()),
  wasClient(true) { }

ClosedConnection::ClosedConnection(IncomingConnection& ic)
: ws(ic.ws),
  ip(ic.ip),
  wasClient(false) { }

ConnectionManager::ConnectionManager(uWS::Hub& h, std::string protoName)
: defaultGroup(h.getDefaultGroup<uWS::SERVER>()) {
	h.onConnection([this, pn{std::move(protoName)}] (uWS::WebSocket<uWS::SERVER> * ws, uWS::HttpRequest req) {
		HttpData hd(&req);
		// Maybe this could be moved on the upgrade handler, somehow
		auto argHead = hd.getHeader("sec-websocket-protocol");
		if (!argHead) {
			ws->close(4000);
			return;
		}

		auto args(tokenize(*argHead, ','));
		std::map<std::string, std::string> argList;

		if (args.size() == 0 || args[0] != pn) {
			ws->close(4001);
			return;
		}

		for (auto& s : args) {
			ltrim_v(s);
			sz_t sep = s.find_first_of('+');
			if (sep != std::string_view::npos) {
				std::string str(s.substr(sep + 1));

				try {
					urldecode(str);
				} catch (const std::exception& e) {
					ws->close(4002);
					return;
				}

				argList.emplace(std::string(s.substr(0, sep)), std::move(str));
			}
		}

		auto addr = ws->getAddress();
		Ip ip;
		if (addr.family[0] == 'U' || Ip(addr.address).isLocal()) { // inefficient if using tcp
			if (auto h = hd.getHeader("x-real-ip")) {
				ip = Ip::fromString(h->data(), h->size());
			} else {
				ws->close(4003);
				return;
			}
		} else {
			ip = Ip(addr.address);
		}

		handleIncoming(ws, std::move(argList), hd, ip);
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
				if (ic->onDisconnect) {
					ic->onDisconnect();
				}
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

void ConnectionManager::forEachClient(std::function<void(Client&)> f) {
	defaultGroup.forEach([&f] (uWS::WebSocket<uWS::SERVER> * ws) {
		if (Client * c = static_cast<Client *>(ws->getUserData())) {
			f(*c);
		}
	});
}

void ConnectionManager::forEachProcessor(std::function<void(ConnectionProcessor&)> f) {
	for (auto& p : processors) {
		f(*p.get());
	}
}

void ConnectionManager::handleIncoming(uWS::WebSocket<uWS::SERVER> * ws,
		std::map<std::string, std::string> args, HttpData hd, Ip ip) {
	// can be optimized
	pending.push_front({ConnectionInfo(), ws, std::move(args), processors.begin(), pending.end(), ip, nullptr, false});
	auto ic = pending.begin();
	ic->it = ic;

	for (auto it = processors.begin(); it != processors.end(); ++it) {
		if (!(*it)->preCheck(*ic, hd)) {
			const std::type_info& ti = typeid(*it->get());

			ic->nextProcessor = std::next(it);
			handleFail(*ic, ti);
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
			AuthProgress::one(ic.ws, typeid(*pr.get()));

			// not safe to use ic after calling callback
			pr->asyncCheck(ic, [this, &ic, &pr] (bool ok) {
				// the disconnect handler is only used to signal and handle
				// disconnection to the current asyncCheck.
				ic.onDisconnect = nullptr;
				if (ok && !ic.cancelled) {
					handleAsync(ic);
					return;
				}

				// is true if socket closed
				if (!ic.cancelled) {
					handleFail(ic, typeid(*pr.get()));
				}

				// the cancelled flag is set to true if we called handleFail
				handleDisconnect(ic, true);
			});
			return;
		}
	}

	handleEnd(ic);
}

void ConnectionManager::handleFail(IncomingConnection& ic, const std::type_info& ti) {
	AuthError::one(ic.ws, ti);
	ic.ws->close(4004);
}

void ConnectionManager::handleEnd(IncomingConnection& ic) {
	for (auto& p : processors) {
		if (!p->endCheck(ic)) {
			handleFail(ic, typeid(*p.get()));
			handleDisconnect(ic, true);
			return;
		}
	}

	Client * cl = clientTransformer(ic);

	if (!cl) {
		handleFail(ic, typeid(Client));
		handleDisconnect(ic, true);
		return;
	}

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
	for (auto& p : processors) {
		p->disconnected(cc);
	}
}
