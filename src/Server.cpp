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
  ap(h.getLoop()),
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
  saveTimer(0),
  statsTimer(0) {
	std::cout << "Admin password set to: " << s.getAdminPass() << "." << std::endl;
	std::cout << "Moderator password set to: " << s.getModPass() << "." << std::endl;

	stopCaller->setData(this);
	tb.setWorkerThreadSchedulingPriorityToLowestPossibleValueAllowedByTheOperatingSystem();

	registerEndpoints();
	registerPackets();

	ap.setNotifyFunc([] (auto notif) {
		std::cout << "[Postgre." << notif.bePid() << "/" << notif.channelName() << "]: " << notif.extra() << std::endl;
	});

	ap.connect();

	// still not connected, however, you can query:
	ap.query("DELETE FROM friends")
	.then([] (auto r) {
		if (!r) std::cout << "[FAIL] ";
		std::cout << "Cleaned table friends" << std::endl;
	});

	ap.query("INSERT INTO friends (name, age, male, descript) VALUES ('John', 18, true, 'Test description')")
	.then([] (auto r) {
		if (!r) std::cout << "[FAIL] ";
		std::cout << "query 1 cb exec" << std::endl;
	});

	ap.query("INSERT INTO friends (name, age, male, descript) VALUES ('Sarah', 17, false, 'Test description 2')")
	.then([] (auto r) {
		if (!r) std::cout << "[FAIL] ";
		std::cout << "query 2 cb exec" << std::endl;
	});

	ap.query("INSERT INTO friends (name, age, male, descript) VALUES ('Alex', 18, null, 'Test description 3')")
	.then([] (auto r) {
		if (!r) std::cout << "[FAIL] ";
		std::cout << "query 3 cb exec" << std::endl;
	});

	ap.query("SELECT * FROM friends")
	.then([] (auto r) {
		if (!r) std::cout << "[FAIL] ";
		std::cout << "query 4 cb exec" << std::endl;
		if (!r) return;
		for (AsyncPostgres::Result::Row row : r) { // can't auto, why???
			auto v = row.get<std::string, int, estd::optional<bool>, std::string>();
			std::cout << "-> " << std::get<0>(v) << "," << std::get<1>(v) << ",";
			if (std::get<2>(v)) std::cout << *std::get<2>(v);
			else std::cout << "null";
			std::cout << "," << std::get<3>(v) << std::endl;
		}
	});


	conn.addToBeg<ProxyChecker>(hcli, tc).setState(ProxyChecker::State::OFF);
	conn.addToBeg<CaptchaChecker>(hcli).setState(CaptchaChecker::State::OFF);
	conn.addToBeg<WorldChecker>(wm);
	/*conn.addToBeg<HeaderChecker>(std::initializer_list<std::string>{
		"http://ourworldofpixels.com",
		"https://ourworldofpixels.com",
	});*/
	conn.addToBeg<BanChecker>(bm);
	conn.addToBeg<ConnectionCounter>().setCounterUpdateFunc([this] (ConnectionCounter& cc) {
		if (statsTimer) { // if not 0
			tc.resetTimer(statsTimer);
		} else {
			statsTimer = tc.startTimer([this, &cc] {
				wm.forEach([count{cc.getCurrentActive()}] (World& w) {
					w.sendPlayerCountStats(count);
				});

				statsTimer = 0;
				return false;
			}, 100);
		}
	});

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

void Server::registerPackets() {
	//pr.on<>
}

void Server::unsafeStop() {
	if (stopCaller) {
		h.getDefaultGroup<uWS::SERVER>().close(1012);
		stopCaller = nullptr;
		tc.clearTimers();
		tb.prepareForDestruction();
		ap.lazyDisconnect();
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
