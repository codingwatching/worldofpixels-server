#include "Server.hpp"

#include <Client.hpp>
#include <Player.hpp>
#include <WorldManager.hpp>
#include <BansManager.hpp>
#include <World.hpp>

#include <ConnectionCounter.hpp>
#include <BanChecker.hpp>
#include <WorldChecker.hpp>
#include <HeaderChecker.hpp>
#include <CaptchaChecker.hpp>
#include <ProxyChecker.hpp>

#include <misc/shared_ptr_ll.hpp>
#include <misc/utils.hpp>
#include <misc/base64.hpp>

#include <nlohmann/json.hpp>

#include <iostream>
#include <utility>
#include <exception>
#include <new>

constexpr auto asyncDeleter = [] (uS::Async * a) {
	a->close();
};

Server::Server(std::string basePath)
: startupTime(std::chrono::steady_clock::now()),
  h(uWS::NO_DELAY, true, 16384),
  stopCaller(new uS::Async(h.getLoop()), asyncDeleter),
  s(std::move(basePath)),
  bm(s.getBansManager()),
  tb(h.getLoop()), // XXX: this should get destructed before other users of the taskbuffer, like WorldManager. what do?
  tc(h.getLoop()),
  am(tc),
  api(h, am),
  hcli(h.getLoop()),
  wm(tb, tc, s),
  pr(h),
  //cmd(*this),
  conn(h, am, "OWOP"),
  saveTimer(0) {
	std::cout << "Admin password set to: " << s.getAdminPass() << "." << std::endl;
	std::cout << "Moderator password set to: " << s.getModPass() << "." << std::endl;

	stopCaller->setData(this);
	tb.setWorkerThreadSchedulingPriorityToLowestPossibleValueAllowedByTheOperatingSystem();

	registerEndpoints();
	registerPackets();

	conn.addToBeg<ProxyChecker>(hcli, tc).setState(ProxyChecker::State::OFF);
	conn.addToBeg<CaptchaChecker>(hcli).setState(CaptchaChecker::State::OFF);
	conn.addToBeg<WorldChecker>(wm);
	conn.addToBeg<HeaderChecker>(std::initializer_list<std::string>{
		"http://ourworldofpixels.com",
		"https://ourworldofpixels.com",
		"https://jsconsole.com"
	});
	conn.addToBeg<BanChecker>(bm);
	conn.addToBeg<ConnectionCounter>();

	conn.onSocketChecked([this] (IncomingConnection& ic) -> Client * {
		World& w = wm.getOrLoadWorld(ic.ci.world);
		Player::Builder pb;
		w.configurePlayerBuilder(pb);

		return new Client(ic.ws, ic.session, ic.ip, pb);
	});

	h.getDefaultGroup<uWS::SERVER>().startAutoPing(30000);
}

bool Server::listenAndRun() {
	std::string addr(s.getBindAddress());
	u16 port = s.getBindPort();

	const char * host = addr.size() > 0 ? addr.c_str() : nullptr;

	if (!h.listen(host, port, nullptr, uS::ListenOptions::ONLY_IPV4)) {
		unsafeStop();
		std::cerr << "Couldn't listen on " << addr << ":" << port << "!" << std::endl;
		return false;
	}

	std::cout << "Listening on " << addr << ":" << port << std::endl;

	saveTimer = tc.startTimer([this] {
		kickInactivePlayers();
		if (wm.saveAll()) {
			std::cout << "Worlds saved." << std::endl;
		}
		return true;
	}, 900000);

	stopCaller->start(Server::doStop);

	try {
		h.run();
	} catch (const std::exception& e) {
		std::cerr << "!!! Uncaught exception occurred! Stopping server." << std::endl;
		std::cerr << "Type: " << demangle(typeid(e)) << std::endl;
		std::cerr << "what(): " << e.what() << std::endl;
		unsafeStop();
		return false;
	}

	return true;
}

bool Server::freeMemory() {
	if (wm.unloadOldChunks() == 0
		&& wm.unloadOldChunks(true) == 0

	) {
		return false;
	}

	return true;
}

void Server::kickInactivePlayers() {
	auto now(std::chrono::steady_clock::now());

	conn.forEachClient([now] (Client& c) {
		if (c.inactiveKickEnabled() && now - c.getLastActionTime() > std::chrono::hours(1)) {
			c.close();
		}
	});
}

void Server::registerEndpoints() {
	api.on(ApiProcessor::GET)
		.path("auth")
		.path("guest")
	.onOutsider(true, [this] (ll::shared_ptr<Request> req, nlohmann::json) {
		std::string ua;
		std::string lang("en");

		if (auto h = req->getData()->getHeader("accept-language", 15)) {
			auto langs(tokenize(h.toString(), ',', true));
			if (langs.size() != 0) {
				lang = std::move(langs[0]);
				// example: "en-US;q=0.9", we want just "en" (for now)
				sz_t i = lang.find_first_of("-;");
				if (i != std::string::npos) {
					lang.erase(i); // from - or ; to the end
				}
			}
		}

		if (auto h = req->getData()->getHeader("user-agent", 10)) {
			ua = h.toString();
		}

		Ipv4 ip(req->getIp()); // done here because the move invalidates ref before calling getIp
		am.createGuestSession(ip, ua, lang, [req{std::move(req)}] (auto token, Session& s) {
			if (!req->isCancelled()) {
				std::string b64tok(base64Encode(token.data(), token.size()));
				req->end({
					{ "token", std::move(b64tok) }
				});
			} else {
				// expire the session?
			}
		});
	});

	api.on(ApiProcessor::GET)
		.path("status")
	.onOutsider(false, [this] (ll::shared_ptr<Request> req, nlohmann::json) {
		Ipv4 ip(req->getIp());

		bool banned = bm.isBanned(ip);

		nlohmann::json j = {
			{ "motd", "Almost done!" },
			{ "activeHttpHandles", hcli.activeHandles() },
			{ "uptime", std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - startupTime).count() }, // lol
			{ "yourIp", ip },
			{ "banned", banned }
		};

		nlohmann::json processorInfo;

		conn.forEachProcessor([&processorInfo] (ConnectionProcessor& p) {
			nlohmann::json j = p.getPublicInfo();
			if (!j.is_null()) {
				processorInfo[demangle(typeid(p))] = std::move(j);
			}
		});

		j["connectInfo"] = std::move(processorInfo);

		if (banned) {
			j["banInfo"] = bm.getInfoFor(ip);
		}

		req->end(j);
	});

	api.on(ApiProcessor::GET)
		.path("view")
		.var()
		.var()
		.var()
	.onFriend([this] (ll::shared_ptr<Request> req, nlohmann::json j, Session& s, std::string worldName, i32 x, i32 y) {
		//std::cout << "[" << j << "]" << worldName << ","<< x << "," << y << std::endl;
		if (!wm.verifyWorldName(worldName)) {
			req->writeStatus("400 Bad Request", 15);
			req->end();
			return;
		}

		if (!wm.isLoaded(worldName)) {
			// nginx is supposed to serve unloaded worlds and chunks.
			req->writeStatus("404 Not Found", 13);
			req->end();
			return;
		}

		World& world = wm.getOrLoadWorld(worldName);

		// will encode the png in another thread if necessary and end the request when done
		world.sendChunk(x, y, std::move(req));
	});

	api.on(ApiProcessor::GET)
		.path("debug")
	.onAny([] (ll::shared_ptr<Request> req, nlohmann::json j) {

		req->end({
			{ "call", "outsider" },
			{ "body", std::move(j) }
		});

	}, [] (ll::shared_ptr<Request> req, nlohmann::json j, Session& s) {

		req->end({
			{ "call", "friend" },
			{ "body", std::move(j) },
			{ "session", {
				{ "user", s.getUser() },
				{ "ip", s.getCreatorIp() },
				{ "ua", s.getCreatorUserAgent() },
				{ "lang", s.getPreferredLanguage() }
			}}
		});

	});
}

void Server::registerPackets() {
	//pr.on<>
}

void Server::unsafeStop() {
	if (stopCaller) {
		h.getDefaultGroup<uWS::SERVER>().close(1012);
		stopCaller = nullptr;
		tc.clearTimers();
		tb.prepareForDestruction();
	}
}

void Server::doStop(uS::Async * a) {
	std::cout << "Signal received, stopping server..." << std::endl;
	Server * s = static_cast<Server *>(a->getData());
	s->unsafeStop();
}

void Server::stop() {
	if (stopCaller) {
		stopCaller->send();
	}
}
