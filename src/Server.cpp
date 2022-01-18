#include "Server.hpp"

#include <User.hpp>
#include <UviasRank.hpp>
#include <Client.hpp>
#include <Player.hpp>
#include <WorldManager.hpp>
#include <BansManager.hpp>
#include <World.hpp>
#include <PacketDefinitions.hpp>

#include <ConnectionCounter.hpp>
#include <BanChecker.hpp>
#include <SessionChecker.hpp>
#include <WorldChecker.hpp>
#include <HeaderChecker.hpp>
#include <CaptchaChecker.hpp>
#include <ProxyChecker.hpp>

#include <iostream>
#include <utility>
#include <exception>
#include <new>
#include <initializer_list>
#include <string_view>

#include <nlohmann/json.hpp>

constexpr auto asyncDeleter = [] (uS::Async * a) {
	a->close();
};

std::string_view getEnvOr(const char * env_var, std::string_view def) {
	const char * env_val = std::getenv(env_var);
	auto val = env_val ? std::string_view{env_val} : def;
	return val;
}

Server::Server(std::string basePath)
: startupTime(std::chrono::steady_clock::now()),
  h(uWS::NO_DELAY, true, 16384),
  stopCaller(new uS::Async(h.getLoop()), asyncDeleter),
  s(std::move(basePath)),
  bm(s.getBansManager()),
  tb(h.getLoop()), // XXX: this should get destructed before other users of the taskbuffer, like WorldManager. what do?
  tc(h.getLoop()),
  ap(h.getLoop(), tc),
  am(ap),
  wm(tb, tc, s),
  conn(h, "OWOP"),
  api(h),
  ac(h.getLoop()),
  pr(h, [] (Client& c) { c.updateLastActionTime(); }), // for every packet
  saveTimer(0),
  statsTimer(0) {
	stopCaller->setData(this);
	tb.setWorkerThreadsSchedulingPriorityToLowestPossibleValueAllowedByTheOperatingSystem();

	registerEndpoints();
	registerPackets();

	ap.onNotification([this] (auto notif) {
		std::cout << "[Postgre." << notif.bePid() << "/" << notif.channelName() << "]: " << notif.extra() << std::endl;

		auto search = notifHandlers.find(notif.channelName());
		if (search != notifHandlers.end()) {
			search->second(notif.extra());
		}
	});

	ap.onConnectionStateChange([this] (auto state) {
		switch (state) {
			case CONNECTION_OK:
				std::cout << "Connected to DB!" << std::endl;
				registerNotifs();
				break;

			case CONNECTION_BAD:
				std::cout << "Disconnected from DB!" << std::endl;
				break;
				
			default:
				break;
		}
	});
	
	if (const char *host = std::getenv("DB_HOST")) {
		const char *user = std::getenv("DB_USER");
		const char *pass = std::getenv("DB_PASSWORD");
		ap.connect({
			{"host", host},
			 {"user", user},
			 {"password", pass},
			 { "dbname", "uvias" }
		});
	} else {
		ap.connect({{ "dbname", "uvias" }});
	}

	std::string domain(getEnvOr("API_HOST",
#ifdef DEBUG
								"dev."
#endif								
								"ourworldofpixels.com"));
	std::cout << "Domain: " << domain << std::endl;

	ap.query<9>("SELECT accounts.set_service_info($1::VARCHAR(8), $2::VARCHAR(64), $3::VARCHAR(64), $4::VARCHAR(128), $5::VARCHAR(128), $6::INT, $7::BOOL)",
			"owop", "Our World of Pixels (dev)", domain, "/api/sso", "/", ::getpid(), domain != "ourworldofpixels.com");

	//conn.addToBeg<ProxyChecker>(pcra).setState(ProxyChecker::State::OFF);
	//conn.addToBeg<CaptchaChecker>(rcra).setState(CaptchaChecker::State::OFF);
	conn.addToBeg<BanChecker>(bm); // check bans after session is obtained -- allows user-specific bans
	conn.addToBeg<SessionChecker>(am);
	conn.addToBeg<WorldChecker>(wm);
#ifndef DEBUG
	conn.addToBeg<HeaderChecker>(std::initializer_list<std::string>({
		"https://ourworldofpixels.com",
		"https://dev.ourworldofpixels.com"
	}));
#endif
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
		if (!ic.ci.session) {
			return nullptr;
		}

		User& u = ic.ci.session->getUser();
		const UviasRank& uvr = u.getUviasRank();

		AuthOk::one(ic.ws,
			u.getId(), u.getUsername(), u.getTotalRep(),
			uvr.getId(), std::string(uvr.getName()), uvr.isSuperUser(), uvr.canSelfManage());

		World& w = wm.getOrLoadWorld(ic.ci.world);
		Player::Builder pb;
		w.configurePlayerBuilder(pb);

		return new Client(ic.ws, std::move(ic.ci.session), ic.ip, pb);
	});

	h.getDefaultGroup<uWS::SERVER>().startAutoPing(30000);
}

bool Server::listenAndRun() {
	std::string addr(s.getBindAddress());
	u16 port = s.getBindPort();

	const char * host = addr.size() > 0 ? addr.c_str() : nullptr;

	if (!h.listen(host, port)) {
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

void Server::registerNotifs() {
	notifHandlers.clear();

	const auto dbListen = [this] (std::string name, auto cb) {
		ap.query<999>("LISTEN " + name)
		->then([name] (auto r) {
			if (!r) std::cout << "[FAIL] ";
			std::cout << "Listening to " << name << " notifs" << std::endl;
		});

		notifHandlers.emplace(name, std::move(cb));
	};

	const auto rankUpdate = [this] (AsyncPostgres::Result r) {
		r.forEach([this] (int id, std::string name, bool superUser, bool selfManage) {
			am.updateRank(UviasRank(id, std::move(name), superUser, selfManage));
		});
	};

	dbListen("uv_kick", [] (auto) { });

	dbListen("uv_rep_upd", [] (auto) { });

	dbListen("uv_user_upd", [] (auto) { });
	dbListen("uv_user_del", [] (auto) { });

	dbListen("uv_rank_upd", [this, rankUpdate] (auto data) {
		nlohmann::json j = nlohmann::json::parse(data);
		int id = j["id"].get<int>();
		std::cout << "Rank updated: " << id << std::endl;

		ap.query<10>("SELECT id, name, admin_superuser, self_manage FROM accounts.ranks WHERE id = $1::INT", id)
		->then(rankUpdate);
	});

	ap.query<10>("SELECT id, name, admin_superuser, self_manage FROM accounts.ranks")
	->then([rankUpdate] (auto r) {
		std::cout << "Ranks loaded: " << r.size() << std::endl;
		rankUpdate(std::move(r));
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
