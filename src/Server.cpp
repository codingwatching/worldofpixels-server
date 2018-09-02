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
  api(h, "status"),
  cmd(*this),
  conn(h, "OWOP"),
  saveTimer(0) {
	std::cout << "Admin password set to: " << s.getAdminPass() << "." << std::endl;
	std::cout << "Moderator password set to: " << s.getModPass() << "." << std::endl;

	stopCaller->setData(this);
	tb.setWorkerThreadSchedulingPriorityToLowestPossibleValueAllowedByTheOperatingSystem();

	api.set("status", [this] (uWS::HttpResponse * res, auto& rs, auto args) {
		std::string ip(res->getHttpSocket()->getAddress().address);

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
			processorInfo[demangle(typeid(p))] = p.getPublicInfo();
		});

		j["connectInfo"] = std::move(processorInfo);

		if (banned) {
			j["banInfo"] = bm.getInfoFor(ip);
		}

		std::string msg(j.dump());
		res->end(msg.data(), msg.size());
		return ApiProcessor::OK;
	});

	api.set("view", [this] (uWS::HttpResponse * res, auto& rs, auto args) {
		if (args.size() != 4 || !wm.verifyWorldName(args[1])) {
			return ApiProcessor::INVALID_ARGS;
		}

		if (!wm.isLoaded(args[1])) {
			// nginx is supposed to serve unloaded worlds and chunks.
			res->end("\"This world is not loaded!\"", 27);
			return ApiProcessor::OK;
		}

		World& world = wm.getOrLoadWorld(args[1]);

		i32 x = 0;
		i32 y = 0;

		try {
			x = std::stoi(args[2]);
			y = std::stoi(args[3]);
		} catch(...) { return ApiProcessor::INVALID_ARGS; }

		// will encode the png in another thread if necessary and end the request when done
		if (!world.sendChunk(res, x, y)) {
			// didn't immediately send
			rs.onCancel = [&world, x, y, res] {
				// the world is guaranteed not to unload until all requests finish
				// so this reference should be valid
				world.cancelChunkRequest(res, x, y);
			};
		}

		return ApiProcessor::OK;
	});

	conn.addToBeg<CaptchaChecker>(hcli).disable();
	conn.addToBeg<WorldChecker>(wm);
	conn.addToBeg<HeaderChecker>(std::initializer_list<std::string>({
		"http://ourworldofpixels.com",
		"https://ourworldofpixels.com"
	}));
	conn.addToBeg<BanChecker>(bm);
	conn.addToBeg<ConnectionCounter>();

	conn.onSocketChecked([this] (IncomingConnection& ic) {
		World& w = wm.getOrLoadWorld(ic.ci.world);
		Player::Builder pb;
		w.configurePlayerBuilder(pb);
		return new Client(ic.ws, w, pb, std::move(ic.ci.ui), std::move(ic.ip));
	});

#if 0
	h.onConnection([this] (uWS::WebSocket<uWS::SERVER> * ws, uWS::HttpRequest req) {
		auto search = conns.find(si->ip);
		if (search == conns.end()) {
			conns[si->ip] = 1;
		} else if (++search->second > maxconns) {
			std::string m("Sorry, but you have reached the maximum number of simultaneous connections, (" + std::to_string(maxconns) + ").");
			ws->send(m.c_str(), m.size(), uWS::TEXT);
			ws->close();
			return;
		}

		/*if (proxy_lock && !whitelisted && proxyquery_checking.find(si->ip) == proxyquery_checking.end()) {
			proxyquery_checking.emplace(si->ip);
			std::string params("ip=" + si->ip);
			params += "&flags=m";
			params += CONTACT_PARAM;
			auto * tskbuf = &async_tasks;
			Server * sv = this;
			std::string ipcopy(si->ip);
			hcli.queueRequest({
				"http://check.getipintel.net/check.php",
				params,
				[tskbuf, ipcopy, sv]
				(CURL * const c, const CURLcode cc, const std::string & buf) {
					long httpcode = -1;
					curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &httpcode);
					if (cc != CURLE_OK || httpcode == 429) {
						std::cout << "Error occurred when checking for proxy connection, HTTP code: "
							<< httpcode << ", buffer: " << buf << std::endl;
						if (httpcode == 429) { // Query limit exceeded, disable the proxy check.
							tskbuf->queueTask([sv] {
								std::cout << "Disabling proxy check, and enabling captcha protection..." << std::endl;
								sv->admintell("DEVProxy query limit exceeded! (API returned error 429)");
								sv->admintell("DEVDisabling proxy checking, and enabling CAPTCHA... (if not already on)");
								sv->set_proxycheck(false);
								sv->set_captcha_protection(true);
							});
						} else {
							tskbuf->queueTask([sv, ipcopy, httpcode, buf, cc] {
								sv->admintell("Unknown error occurred in proxy checking API, HTTP Code: " + std::to_string(httpcode) + ", response: " + buf);
								sv->admintell("libCURL err -> " + std::string(curl_easy_strerror(cc)));
								sv->proxyquery_checking.erase(ipcopy);
							});
						}
						tskbuf->runTasks();
						return;
					}
					i32 code = -7;
					try {
						code = std::stoi(buf);
					} catch(const std::invalid_argument & e) { }
					  catch(const std::out_of_range& e) { }
					tskbuf->queueTask([sv, code, ipcopy] {
						sv->proxyquery_checking.erase(ipcopy);
						if (code < 0 && code != -3) {
							std::string e("Proxy check API returned an error: " + std::to_string(code) + ", for IP: " + ipcopy);
							puts(e.c_str());
							sv->admintell("DEV" + e, true);
							if (code == -5 || code == -6) {
								sv->set_proxycheck(false);
							}
							return;
						}
						switch (code) {
							case 1:
								sv->banip(ipcopy, -1);
								break;
							case 0:
							case -3: // Unroutable address / private address
								sv->whitelistip(ipcopy);
								break;
						}
					});
					tskbuf->runTasks();
				}
			});
		}*/
	});
#endif

	h.onMessage([this] (uWS::WebSocket<uWS::SERVER> * ws, const char * msg, sz_t len, uWS::OpCode oc) {
		Client * cl = static_cast<Client *>(ws->getUserData());
		if (!cl) { return; }
		Player& pl = cl->getPlayer();

		if (oc == uWS::BINARY) {
			switch (len) {
				case 10: {
					/*if (player->get_rank() < Client::MODERATOR) {
						player->tell("Stop playing around with mod tools! :)");
						break;
					}*/
					chunkpos_t pos = *((chunkpos_t *)msg);
					pl.getWorld().setChunkProtection(pos.x, pos.y, (bool) msg[sizeof(chunkpos_t)]);
				} break;

				case 11: {
					pixpkt_t pos = *((pixpkt_t *)msg);
					pl.tryPaint(pos.x, pos.y, {pos.r, pos.g, pos.b});
				} break;

				case 12: {
					pinfo_t pos = *((pinfo_t *)msg);
					pl.tryMoveTo(pos.x, pos.y, pos.tool);
				} break;
			};
		} else if (oc == uWS::TEXT && len > 1 && msg[len-1] == '\12') {
			std::string mstr(msg, len - 1);
			if (getUtf8StrLen(mstr) <= 128) {
				if(msg[0] != '/'){
					pl.tryChat(std::move(mstr));
				} else {
					pl.tell("Commands unavailable atm.");
					//cmds.exec(player, std::string(msg + 1, len - 2));
				}
			}
		}
	});

	h.getDefaultGroup<uWS::SERVER>().startAutoPing(15000);
}

void Server::broadcastmsg(const std::string& msg) {
	h.getDefaultGroup<uWS::SERVER>().broadcast(msg.c_str(), msg.size(), uWS::TEXT);
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
/*	for (auto it = connsws.begin(); it != connsws.end();) {
		if (Client * c = static_cast<SocketInfo *>((*it++)->getUserData())->player) {
			if (now - c->get_last_move() > 1200000 && !c->is_admin()) {
				c.disconnect();
			}
		}
	}*/
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
