#include "Server.hpp"

#include <Client.hpp>
#include <Player.hpp>
#include <WorldManager.hpp>
#include <BansManager.hpp>
#include <World.hpp>
#include <keys.hpp>

#include <ConnectionCounter.hpp>
#include <BanChecker.hpp>
#include <WorldChecker.hpp>
#include <HeaderChecker.hpp>
#include <CaptchaChecker.hpp>
#include <ProxyChecker.hpp>

#include <misc/utils.hpp>

#include <nlohmann/json.hpp>

#include <iostream>
#include <utility>

constexpr auto asyncDeleter = [] (uS::Async * a) {
	a->close();
};

Server::Server(std::string basePath)
: startupTime(jsDateNow()),
  h(uWS::NO_DELAY, true, 16384),
  stopCaller(new uS::Async(h.getLoop()), asyncDeleter),
  s(std::move(basePath)),
  bm(s.getBansManager()),
  tb(h.getLoop()), // XXX: this should get destructed before other users of the taskbuffer, like WorldManager. what do?
  tc(h.getLoop()),
  hcli(h.getLoop()),
  wm(tb, tc, s),
  pr(h),
  api(h),
  //cmd(*this),
  conn(h, "OWOP"),
  saveTimer(0) {
	std::cout << "Admin password set to: " << s.getAdminPass() << "." << std::endl;
	std::cout << "Moderator password set to: " << s.getModPass() << "." << std::endl;

	stopCaller->setData(this);
	tb.setWorkerThreadSchedulingPriorityToLowestPossibleValueAllowedByTheOperatingSystem();

	api.on(ApiProcessor::GET)
		.path("status")
	.end([this] (std::shared_ptr<Request> req, nlohmann::json) {
		std::string ip(req->getResponse()->getHttpSocket()->getAddress().address);

		bool banned = bm.isBanned(ip);

		nlohmann::json j = {
			{ "motd", "Now with Enterprise Quality™ code!" },
			{ "activeHttpHandles", hcli.activeHandles() },
			{ "uptime", jsDateNow() - startupTime },
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
	.end([this] (std::shared_ptr<Request> req, nlohmann::json j, std::string worldName, i32 x, i32 y) {
		//std::cout << "[" << j << "]" << worldName << ","<< x << "," << y << std::endl;
		if (!wm.verifyWorldName(worldName)) {
			req->writeStatus("400 Bad Request");
			req->end();
			return;
		}

		if (!wm.isLoaded(worldName)) {
			// nginx is supposed to serve unloaded worlds and chunks.
			req->writeStatus("404 Not Found");
			req->end();
			return;
		}

		World& world = wm.getOrLoadWorld(worldName);

		// will encode the png in another thread if necessary and end the request when done
		world.sendChunk(x, y, std::move(req));
	});

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
		return new Client(ic.ws, w, pb, std::move(ic.ci.ui), std::move(ic.ip));
	});

	//pr.set<>

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

	h.run();
	return true;
}

void Server::kickInactivePlayers() {
	i64 now = jsDateNow();
	conn.forEachClient([now] (Client& c) {
		if (c.inactiveKickEnabled() && now - c.getLastActionTime() > 1200000) {
			c.close();
		}
	});
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
