#include "Server.hpp"

#include <Client.hpp>
#include <WorldManager.hpp>
#include <BansManager.hpp>
#include <World.hpp>
#include <keys.hpp>

#include <misc/utils.hpp>

#include <nlohmann/json.hpp>

#include <fstream>
#include <iostream>
#include <utility>

constexpr auto asyncDeleter = [](uS::Async * a) {
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
  totalConnections(0),
  saveTimer(0),
  maxconns(6),
  captcha_required(false),
  lockdown(false),
  proxy_lock(false),
  instaban(false),
  trusting_captcha(false),
  fastconnectaction(0) {
	std::cout << "Admin password set to: " << s.getAdminPass() << "." << std::endl;
	std::cout << "Moderator password set to: " << s.getModPass() << "." << std::endl;

	stopCaller->setData(this);
	tb.setWorkerThreadSchedulingPriorityToLowestPossibleValueAllowedByTheOperatingSystem();

	api.set("status", [this] (uWS::HttpResponse * res, auto& rs, auto args) {
		std::string ip(res->getHttpSocket()->getAddress().address);

		u8 yourConns = 0;
		bool banned = bm.isBanned(ip);
		{ auto s = conns.find(ip); if (s != conns.end()) yourConns = s->second; }

		nlohmann::json j = {
			{ "motd", "Now with Enterprise Quality™ code!" },
			{ "totalConnections", totalConnections },
			{ "captchaEnabled", captcha_required },
			{ "maxConnectionsPerIp", maxconns },
			{ "users", connsws.size() },
			{ "uptime", jsDateNow() - startupTime },
			{ "yourIp", ip },
			{ "yourConns", yourConns },
			{ "banned", banned }
		};

		std::string msg(j.dump());
		res->end(msg.data(), msg.size());
		return ApiProcessor::OK;
	});

	api.set("disconnectme", [this] (uWS::HttpResponse * res, auto& rs, auto args) {
		std::string ip(res->getHttpSocket()->getAddress().address);

		nlohmann::json j = {
			{ "hadEffect", this->kickip(ip) } // this-> required by gcc bug
		};

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
		if (!world.send_chunk(res, x, y)) {
			// didn't immediately send
			rs.onCancel = [&world, x, y, res] {
				// the world is guaranteed not to unload until all requests finish
				// so this reference should be valid
				world.cancelChunkRequest(x, y, res);
			};
		}

		return ApiProcessor::OK;
	});

	conn.onSocketChecked([this] (IncomingConnection& ic) {
		std::string world(ic.args.at("world"));

	});

	h.onConnection([this](uWS::WebSocket<uWS::SERVER> * ws, uWS::HttpRequest req) {
		++totalConnections;
		/*uWS::Header argHead(req.getHeader("sec-websocket-protocol", 22));
		if (!argHead) {
			ws->terminate();
			return;
		}
		auto args(tokenize(argHead.toString(), ','));
		for (auto& s : args) {
			ltrim(s);
			std::cout << s << std::endl;
		}*/
		SocketInfo * si = new SocketInfo();
		si->ip = ws->getAddress().address;

		uWS::Header o = req.getHeader("origin", 6);
		si->origin = o ? o.toString() : "(None)";

		si->player = nullptr;
		connsws.emplace(ws);
		ws->setUserData(si);

		bool banned = bm.isBanned(si->ip);

		bool isskidbot = instaban && si->origin == "(None)";
		si->captcha_verified = {captcha_required ? CA_WAITING : CA_OK};

		if (lockdown || banned) {
			if (!banned) {
				std::string m("Sorry, the server is not accepting new connections right now.");
				ws->send(m.c_str(), m.size(), uWS::TEXT);
			} else {
				auto info(bm.getInfoFor(si->ip));
				std::string m("You are banned. Appeal on the OWOP discord server, (https://discord.io/owop)");
				m += "\nReason: " + info.reason;
				m += "\nTime remaining: " + std::to_string(jsDateNow() - info.expiresOn / 1000) + " seconds.";
				ws->send(m.c_str(), m.size(), uWS::TEXT);
			}
			ws->close();
			return;
		}

		if (isskidbot) {
			bm.ban(si->ip, 0, "Normal Shutdown, Thank you for playing");
			admintell("DEVBanned IP (skid detected): " + si->ip);
			ws->close();
			return;
		}

		auto search = conns.find(si->ip);
		if (search == conns.end()) {
			conns[si->ip] = 1;
		} else if (++search->second > maxconns) {
			std::string m("Sorry, but you have reached the maximum number of simultaneous connections, (" + std::to_string(maxconns) + ").");
			ws->send(m.c_str(), m.size(), uWS::TEXT);
			ws->close();
			return;
		}

		u8 captcha_request[2] = {CAPTCHA_REQUIRED, si->captcha_verified};
		ws->send((const char *)&captcha_request[0], sizeof(captcha_request), uWS::BINARY);
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

	h.onMessage([this](uWS::WebSocket<uWS::SERVER> * ws, const char * msg, sz_t len, uWS::OpCode oc) {
		SocketInfo * const si = ((SocketInfo *)ws->getUserData());
		Client * const player = si->player;
		if (si->captcha_verified == CA_WAITING && oc == uWS::TEXT && len > 7 && std::string(msg, 7) == "CaptchA" && len < 2048) {
			std::string org_capt(msg + 7, len - 7);
			NOTHING_SPECIAL_JUST_IGNORE_THIS_LINE_OF_TEXT_KTHX;

			si->captcha_verified = CA_VERIFYING;
			hcli.addRequest("https://www.google.com/recaptcha/api/siteverify", {
				{"secret", CAPTCHA_API_KEY},
				{"remoteip", si->ip},
				{"response", org_capt}
			}, [this, ws] (AsyncHttp::Result res) {
				if (!res.successful) {
					/* HTTP ERROR code check */
					std::cout << "Error occurred when verifying captcha: " << res.errorString << ", " << res.data << std::endl;
					admintell("Captcha system failed: " + std::string(res.errorString), true);
					set_captcha_protection(false);
					return;
				}

				bool verified = false;
				std::string failReason;
				try {
					nlohmann::json response = nlohmann::json::parse(res.data);
					if (response["success"].is_boolean() && response["hostname"].is_string()) {
						bool success = response["success"].get<bool>();
						std::string host(response["hostname"].get<std::string>());
						verified = success && host == "ourworldofpixels.com";
						if (!success) {
							failReason = "API rejected token";
							if (response["error-codes"].is_array()) {
								failReason += " " + response["error-codes"].dump();
							}
						} else if (success && !verified) {
							failReason = "Wrong hostname: '" + host + "'";
						}
					}
				} catch (const std::invalid_argument & e) {
					std::cout << "Exception when parsing json by google! (" << res.data << ")" << std::endl;
					failReason = "API JSON parsing failed";
					//ws.close(); to be safely fixed. captcha will be required to pass on connect
					// not after connecting, will fix many issues.
				}

				if (is_connected(ws)) { /* Let's hope this prevents all the bad stuff */
					SocketInfo * const si = static_cast<SocketInfo *>(ws->getUserData());
					si->captcha_verified = verified ? CA_OK : CA_INVALID;

					u8 captcha_request[2] = {CAPTCHA_REQUIRED, si->captcha_verified};
					ws->send((const char *)&captcha_request[0], sizeof(captcha_request), uWS::BINARY);
					if (si->captcha_verified == CA_INVALID) {
						admintell("DEVFailed captcha, IP: " + si->ip + " (" + failReason + ")", true);
						ws->close();
					} else {
						std::cout << "Captcha verified for IP: " << si->ip << std::endl;
					}
				}
			});
		} else if(player && oc == uWS::BINARY){
			switch(len){
				case 1: {
					u8 rank = *((u8 *)&msg[0]);
					if (rank > player->get_rank()) {
						player->disconnect();
					}
				} break;

				/*case 8: {
					chunkpos_t pos = *((chunkpos_t *)msg);
					player->get_chunk(pos.x, pos.y);
				} break;*/

				case 10: {
					if(player->get_rank() < Client::MODERATOR){
						/* No hacks for you either */
						player->tell("Stop playing around with mod tools! :)");
						break;
					}
					chunkpos_t pos = *((chunkpos_t *)msg);
					player->get_world().setChunkProtection(pos.x, pos.y, (bool) msg[sizeof(chunkpos_t)]);
				} break;

				case 11: {
					pixpkt_t pos = *((pixpkt_t *)msg);
					player->put_px(pos.x, pos.y, {pos.r, pos.g, pos.b});
				} break;

				case 12: {
					pinfo_t pos = *((pinfo_t *)msg);
					if((pos.tool <= 2 || pos.tool == 4 || pos.tool == 5 || pos.tool == 7 || pos.tool == 8) || (player->is_admin() || player->is_mod())){
						player->move(pos);
					} else {
						pos.tool = 0;
						player->move(pos);
					}
				} break;

				case 13: {
					if(!(player->is_admin() || player->is_mod())){
						/* No hacks for you */
						player->disconnect();
						break;
					}
					pixpkt_t pos = *((pixpkt_t *)msg);
					player->del_chunk(pos.x, pos.y, {pos.r, pos.g, pos.b});
				} break;

				case 776: {
					if(!player->is_admin()){
						player->disconnect();
						break;
					}
					chunkpos_t pos = *((chunkpos_t *)msg);
					player->get_world().paste_chunk(pos.x, pos.y, msg + sizeof(chunkpos_t));
				} break;
			};
		} else if(player && oc == uWS::TEXT && len > 1 && msg[len-1] == '\12'){
			std::string mstr(msg, len - 1);
			if((player->is_admin() && getUtf8StrLen(mstr) <= 16384) || (player->is_mod() && getUtf8StrLen(mstr) <= 512) || (player->can_chat() && getUtf8StrLen(mstr) <= 128)){
				player->updated();
				if(msg[0] != '/'){
					player->chat(mstr);
				} else {
					player->tell("Commands unavailable atm.");
					//cmds.exec(player, std::string(msg + 1, len - 2));
				}
			} else {
				player->warn();
			}
		} else if(!player && si->captcha_verified == CA_OK && oc == uWS::BINARY && len > 2 && len - 2 <= 24
			  && *((u16 *)(msg + len - 2)) == 4321){
			join_world(ws, std::string(msg, len - 2));
		} else if(!player){
			ws->close();
		}
	});

	h.onDisconnection([this](uWS::WebSocket<uWS::SERVER> * ws, int c, const char * msg, sz_t len) {
		SocketInfo * const si = static_cast<SocketInfo *>(ws->getUserData());
		if (si->player) {
			World& w = si->player->get_world();
			delete si->player;
		}
		auto search = conns.find(si->ip);
		if (search != conns.end()) {
			if (--search->second == 0) {
				conns.erase(search);
			}
		}
		connsws.erase(ws);
		delete si;
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

void Server::join_world(uWS::WebSocket<uWS::SERVER> * ws, const std::string& worldName) {
	if (!wm.verifyWorldName(worldName)) {
		ws->close();
		return;
	}

	World& world = wm.getOrLoadWorld(worldName);

	SocketInfo * si = static_cast<SocketInfo *>(ws->getUserData());
	si->player = new Client(ws, world, si);

	world.add_cli(si->player);
}

bool Server::is_adminpw(const std::string& pw) {
	return pw == s.getAdminPass();
}

bool Server::is_modpw(const std::string& pw) {
	return pw == s.getAdminPass();
}

u32 Server::get_conns(const std::string& ip) {
	auto search = conns.find(ip);
	if (search != conns.end()) {
		return search->second;
	}
	return 0;
}

void Server::admintell(const std::string & msg, bool modsToo) {
	for (auto client : connsws) {
		SocketInfo const * const si = (SocketInfo *)client->getUserData();
		if (si->player && (si->player->is_admin() || (modsToo && si->player->is_mod()))) {
			si->player->tell(msg);
		}
	}
}

void Server::kickInactivePlayers() {
	i64 now = jsDateNow();
	for (auto it = connsws.begin(); it != connsws.end();) {
		if (Client * c = static_cast<SocketInfo *>((*it++)->getUserData())->player) {
			if (now - c->get_last_move() > 1200000 && !c->is_admin()) {
				c->disconnect();
			}
		}
	}
}

void Server::kickall(World& wrld) {
	for (auto it = connsws.begin(); it != connsws.end();) {
		auto client = *it++;
		SocketInfo const * const si = (SocketInfo *)client->getUserData();
		if (si->player && si->player->get_world().getWorldName() == wrld.getWorldName() && !si->player->is_admin()) {
			si->player->disconnect();
		}
	}
}

void Server::kickall() {
	for (auto it = connsws.begin(); it != connsws.end();) {
		auto client = *it++;
		SocketInfo const * const si = (SocketInfo *)client->getUserData();
		if (si->player && !si->player->is_admin()) {
			si->player->disconnect();
		} else if (!si->player) {
			client->close();
		}
	}
}

bool Server::kickip(const std::string & ip) {
	bool useless = true;
	for (auto it = connsws.begin(); it != connsws.end();) {
		auto client = *it++;
		SocketInfo const * const si = (SocketInfo *)client->getUserData();
		if (si->ip == ip) {
			useless = false;
			client->close();
		}
	}
	if (!useless) {
		admintell("DEVKicked IP: " + ip, true);
		conns.erase(ip);
	}
	return !useless;
}

void Server::set_max_ip_conns(u8 max) {
	maxconns = max;
	bool remaining = true;
	while (remaining) {
		remaining = false;
		for (auto client : connsws) {
			SocketInfo const * const si = (SocketInfo *)client->getUserData();
			auto search = conns.find(si->ip);
			if (search != conns.end() && search->second > max) {
				remaining = true;
				if (si->player) {
					if (si->player->is_admin()) {
						remaining = false;
						continue;
					}
					si->player->disconnect();
				} else {
					client->close();
				}
				break;
			}
		}
	}
}

void Server::set_captcha_protection(bool state) {
	captcha_required = state;
	if (captcha_required) {
		admintell("DEVCaptcha protection enabled.", true);
	} else {
		admintell("DEVCaptcha protection disabled.", true);
	}
}

void Server::lockdown_check() {
	// XXX: possible trap
	if (connsws.size() == 0) {
		set_lockdown(false);
	}
}

void Server::set_lockdown(bool state) {
	lockdown = state;
	if (lockdown) {
		admintell("DEVLockdown mode enabled.", true);
	} else {
		//ipwhitelist.clear();
		admintell("DEVLockdown mode disabled.", true);
	}
}

void Server::set_instaban(bool state) {
	instaban = state;
	if (instaban) {
		bool remaining = true;
		while (remaining) {
			remaining = false;
			for (auto client : connsws) {
				SocketInfo const * const si = (SocketInfo *)client->getUserData();
				if (si->origin == "(None)") {
					remaining = true;
					client->close();
					break;
				}
			}
		}
		admintell("DEVSuspicious banning enabled.", true);
	} else {
		admintell("DEVSuspicious banning disabled.", true);
	}
}

bool Server::is_connected(uWS::WebSocket<uWS::SERVER> * ws) {
	return connsws.find(ws) != connsws.end();
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
