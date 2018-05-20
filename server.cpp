#include "server.hpp"
#include "keys.hpp"

#include "json.hpp"

#include <fstream>

/* Server class functions */

size_t getUTF8strlen(const std::string& str) {
	size_t j = 0, i = 0, x = 1;
	while (i < str.size()) {
		if (x > 4) { /* Invalid unicode */
			return SIZE_MAX;
		}
		
		if ((str[i] & 0xC0) != 0x80) {
			j += x == 4 ? 2 : 1;
			x = 1;
		} else {
			x++;
		}
		i++;
	}
	if (x == 4) {
		j++;
	}
	return (j);
}

int64_t js_date_now() {
	struct timeval tp;
	gettimeofday(&tp, NULL);
	long int ms = tp.tv_sec * 1000 + tp.tv_usec / 1000;
	return ms;
}

Server::Server(const uint16_t port, const std::string& modpw, const std::string& adminpw, const std::string& path)
	: port(port),
	  modpw(modpw), 
	  adminpw(adminpw),
	  path(path + "/"),
	  cmds(this),
	  connlimiter(10, 5),
	  h(uWS::NO_DELAY, true),
	  maxconns(6),
	  captcha_required(false),
	  lockdown(false),
	  proxy_lock(false),
	  instaban(false),
	  trusting_captcha(false),
	  fastconnectaction(0) {
	std::cout << "Admin password set to: " << adminpw << "." << std::endl;
	std::cout << "Moderator password set to: " << modpw << "." << std::endl;
	readfiles();
	
	h.onConnection([this](uWS::WebSocket<uWS::SERVER> ws, uWS::UpgradeInfo ui) {
		SocketInfo * si = new SocketInfo();
		si->ip = ws.getAddress().address;
		if (si->ip.compare(0, 7, "::ffff:") == 0) {
			si->ip = si->ip.substr(7);
		}
		if (ui.originLength > 0) {
			si->origin = std::string(ui.origin, ui.originLength);
		} else {
			si->origin = "(None)";
		}
		si->player = nullptr;
		connsws.emplace(ws);
		ws.setUserData(si);
		int64_t now = js_date_now();
		bool whitelisted = ipwhitelist.find(si->ip) != ipwhitelist.end();
		bool banned = false;
		{
			auto srch = ipban.find(si->ip);
			if (srch != ipban.end()) {
				banned = (now < srch->second || srch->second < 0);
				if (!banned) {
					clearexpbans();
				} else {
					std::string ms("Remaining time: " + std::to_string((srch->second - now) / 1000) + " seconds")
					ws.send(ms.c_str(), ms.size(), uWS::TEXT);
				}
			}
		}
		bool isskidbot = instaban && si->origin == "(None)";
		bool blacklisted = ipblacklist.find(si->ip) != ipwhitelist.end();
		si->captcha_verified = {captcha_required && !(whitelisted && trusting_captcha) ? CA_WAITING : CA_OK};
		if ((lockdown && !whitelisted) || (banned)) {
			if (!banned) {
				std::string m("Sorry, the server is not accepting new connections right now.");
				ws.send(m.c_str(), m.size(), uWS::TEXT);
			} else {
				std::string m("You are banned. Appeal on the OWOP discord server, (https://discord.io/owop)");
				ws.send(m.c_str(), m.size(), uWS::TEXT);
			}
			ws.close();
			return;
		}
		if (isskidbot && !banned) {
			ipban.emplace(si->ip, now + 120 * 1000);
			admintell("DEVBanned IP (skid detected): " + si->ip);
			banned = true;
			ws.close("get rekt");
			return;
		}
		auto search = conns.find(si->ip);
		if (search == conns.end()) {
			conns[si->ip] = 1;
		} else if (++search->second > maxconns || blacklisted) {
			std::string m("Sorry, but you have reached the maximum number of simultaneous connections, (" + std::to_string(blacklisted ? 1 : maxconns) + ").");
			ws.send(m.c_str(), m.size(), uWS::TEXT);
			ws.close();
			return;
		}
		
		if (!connlimiter.can_spend()) {
			switch (fastconnectaction) {
				case 3:
					si->captcha_verified = CA_WAITING;
					break;
				case 2:
					ipban.emplace(si->ip, -1);
					admintell("DEVBanned IP (by fc): " + si->ip, true);
				case 1:
					ws.close();
					break;
			}
		}
		
		uint8_t captcha_request[2] = {CAPTCHA_REQUIRED, si->captcha_verified};
		ws.send((const char *)&captcha_request[0], sizeof(captcha_request), uWS::BINARY);
		if (proxy_lock && !whitelisted && proxyquery_checking.find(si->ip) == proxyquery_checking.end()) {
			proxyquery_checking.emplace(si->ip);
			std::string params("ip=" + si->ip);
			params += "&flags=m";
			params += CONTACT_PARAM;
			auto * tskbuf = &async_tasks;
			Server * sv = this;
			std::string ipcopy(si->ip); /* waste */
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
						if (httpcode == 429) { /* Query limit exceeded, disable the proxy check. */
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
					int32_t code = -7;
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
								sv->banip(ipcopy);
								break;
							case 0:
							case -3: /* Unroutable address / private address */
								sv->whitelistip(ipcopy);
								break;
						}
					});
					tskbuf->runTasks();
				}
			});
		}
	});
	
	h.onMessage([this, adminpw](uWS::WebSocket<uWS::SERVER> ws, const char * msg, size_t len, uWS::OpCode oc) {
		SocketInfo * const si = ((SocketInfo *)ws.getUserData());
		Client * const player = si->player;
		if (si->captcha_verified == CA_WAITING && oc == uWS::TEXT && len > 7 && std::string(msg, 7) == "CaptchA" && len < 2048) {
			char * esc_recaptcha_resp = curl_easy_escape(hcli.getCurl(), msg + 7, len - 7);
			if (!esc_recaptcha_resp) {
				ws.close();
				return;
			}
			std::string g_recaptcha_response(esc_recaptcha_resp);
			curl_free(esc_recaptcha_resp);
			NOTHING_SPECIAL_JUST_IGNORE_THIS_LINE_OF_TEXT_KTHX;
			std::string params(CAPTCHA_API_KEY);
			params += "&remoteip=" + si->ip;
			params += "&response=" + g_recaptcha_response;
			
			si->captcha_verified = CA_VERIFYING;
			auto * tskbuf = &async_tasks;
			Server * sv = this;
			hcli.queueRequest({ /* what if the socket disconnects????????????????? */
				"https://www.google.com/recaptcha/api/siteverify",
				params,
				[tskbuf, ws, sv]
				(CURL * const c, const CURLcode cc, const std::string & buf) {
					if (cc != CURLE_OK) {
						/* HTTP ERROR code check */
						const char * cerr = curl_easy_strerror(cc);
						std::cout << "Error occurred when verifying captcha: " << cerr << ", " << buf << std::endl;
						tskbuf->queueTask([cerr, sv] {
							sv->admintell("Captcha system failed: " + std::string(cerr), true);
							sv->set_captcha_protection(false);
						});
						return;
					}
					nlohmann::json response;
					bool verified = false;
					try {
						response = nlohmann::json::parse(buf);
						if (response["success"].is_boolean()) {
							verified = response["success"].get<bool>();
						}
					} catch (const std::invalid_argument & e) {
						std::cout << "Exception when parsing json by google!" << std::endl;
						return;
					}
					tskbuf->queueTask([ws, sv, verified] {
						/* This function will execute on the main thread, so no locks are needed. */
						if (sv->is_connected(ws)) { /* Let's hope this prevents all the bad stuff */
							uWS::WebSocket<uWS::SERVER> wsc = ws; /* fuck you */
							SocketInfo * const si = ((SocketInfo *)wsc.getUserData());
							si->captcha_verified = verified ? CA_OK : CA_INVALID;
							
							uint8_t captcha_request[2] = {CAPTCHA_REQUIRED, si->captcha_verified};
							wsc.send((const char *)&captcha_request[0], sizeof(captcha_request), uWS::BINARY);
							if (si->captcha_verified == CA_INVALID) {
								sv->admintell("DEVFailed captcha verification for IP: " + si->ip, true);
								wsc.close();
							} else {
								std::cout << "Captcha verified for IP: " << si->ip << std::endl;
								sv->whitelistip(si->ip);
							}
						}
					});
					/* Will only execute on the main thread IF libuv is being used */
					tskbuf->runTasks();
				}
			});
		} else if(player && oc == uWS::BINARY){
			switch(len){
				case 1: {
					uint8_t rank = *((uint8_t *)&msg[0]);
					if (rank > player->get_rank()) {
						player->safedelete(true);
					}
				} break;
				
				case 8: {
					chunkpos_t pos = *((chunkpos_t *)msg);
					player->get_chunk(pos.x, pos.y);
				} break;
				
				case 10: {
					if(player->get_rank() < Client::MODERATOR){
						/* No hacks for you either */
						player->tell("Stop playing around with mod tools! :)");
						break;
					}
					chunkpos_t pos = *((chunkpos_t *)msg);
					player->get_world()->setChunkProtection(pos.x, pos.y, (bool) msg[sizeof(chunkpos_t)]);
				} break;
				
				case 11: {
					pixupd_t pos = *((pixupd_t *)msg);
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
						player->safedelete(true);
						break;
					}
					chunkpos_t pos = *((pixupd_t *)msg);
					player->del_chunk(pos.x, pos.y, {pos.r, pos.g, pos.b});
				} break;
				
				case 776: {
					if(!player->is_admin()){
						player->safedelete(true);
						break;
					}
					chunkpos_t pos = *((chunkpos_t *)msg);
					player->get_world()->paste_chunk(pos.x, pos.y, msg + sizeof(chunkpos_t));
				} break;
			};
		} else if(player && oc == uWS::TEXT && len > 1 && msg[len-1] == '\12'){
			std::string mstr(msg, len - 1);
			if((player->is_admin() && getUTF8strlen(mstr) <= 16384) || (player->is_mod() && getUTF8strlen(mstr) <= 512) || (player->can_chat() && getUTF8strlen(mstr) <= 128)){
				player->updated();
				if(msg[0] != '/'){
					player->chat(mstr);
				} else {
					cmds.exec(player, std::string(msg + 1, len - 2));
				}
			} else {
				player->warn();
			}
		} else if(!player && si->captcha_verified == CA_OK && oc == uWS::BINARY && len > 2 && len - 2 <= 24
			  && *((uint16_t *)(msg + len - 2)) == 4321){
			join_world(ws, std::string(msg, len - 2));
		} else if(!player){
			ws.close();
		}
	});

	h.onDisconnection([this](uWS::WebSocket<uWS::SERVER> ws, int c, const char * msg, size_t len) {
		bool lock_check = false;
		SocketInfo * const si = (SocketInfo *)ws.getUserData();
		if(si->player){
			World * const w = si->player->get_world();
			if (si->player->is_admin() && lockdown) {
				lock_check = true;
			}
			si->player->safedelete(false);
			if(w && w->is_empty()){
				worlds.erase(w->name);
				w->safedelete();
			}
		}
		auto search = conns.find(si->ip);
		if (search != conns.end()) {
			if (--search->second == 0) {
				conns.erase(search);
			}
		}
		connsws.erase(ws);
		delete si;
		if (lock_check) {
			lockdown_check();
		}
	});
}

Server::~Server() {
	for (auto it = connsws.begin(); it != connsws.end();) {
		auto client = *it++;
		client.close();
	}
	for(const auto& world : worlds){
		delete world.second;
	}
	writefiles();
}

void Server::broadcastmsg(const std::string& msg) {
	h.getDefaultGroup<uWS::SERVER>().broadcast(msg.c_str(), msg.size(), uWS::TEXT);
}

void Server::run() {
	mkdir(path.c_str(), 0700);
	uv_timer_init(uv_default_loop(), &save_hdl);
	save_hdl.data = this;
	uv_timer_start(&save_hdl, (uv_timer_cb)&save_chunks, 900000, 900000);
	h.listen(port);
	h.run();
}

void Server::quit() {
	if(!uv_is_closing((uv_handle_t *)&save_hdl)){
		broadcastmsg("Server: Shutting down! (Rejoin in ~5 minutes)");
		uv_timer_stop(&save_hdl);
		uv_close((uv_handle_t *)&save_hdl, (uv_close_cb)([](uv_handle_t * const t){
			delete (Server *)t->data;
			exit(0);
		}));
	} else { /* uv_close gets stuck? */
		delete this;
		exit(0);
	}
}

void Server::save_now() {
	for(const auto& world : worlds){
		world.second->save();
	}
	writefiles();
	std::cout << "Worlds saved." << std::endl;
	admintell("DEVWorlds saved.");
}

void Server::save_chunks(uv_timer_t * const t) {
	Server * const srv = (Server *)t->data;
	srv->save_now();
}

void Server::join_world(uWS::WebSocket<uWS::SERVER> ws, const std::string& worldname) {
	/* Validate world name, allowed chars are a..z, 0..9, '_' and '.' */
	for(size_t i = worldname.size(); i--;){
		if(!((worldname[i] > 96 && worldname[i] < 123) ||
		     (worldname[i] > 47 && worldname[i] < 58) ||
		      worldname[i] == 95 || worldname[i] == 46)){
			ws.close();
			return;
		}
	}
	const auto search = worlds.find(worldname);
	World * w = nullptr;
	if(search == worlds.end()){
		worlds[worldname] = w = new World(path, worldname);
	} else {
		w = search->second;
	}
	if(w){
		SocketInfo * si = (SocketInfo *)ws.getUserData();
		Client * const cl = si->player = new Client(w->get_id(), ws, w, si);
		w->add_cli(cl);
	}
}

bool Server::is_adminpw(const std::string& pw) {
	return pw == adminpw;
}

bool Server::is_modpw(const std::string& pw) {
	return pw == modpw;
}

uint32_t Server::get_conns(const std::string& ip) {
	auto search = conns.find(ip);
	if (search != conns.end()) {
		return search->second;
	}
	return 0;
}

void Server::admintell(const std::string & msg, bool modsToo) {
	for (auto client : connsws) {
		SocketInfo const * const si = (SocketInfo *)client.getUserData();
		if (si->player && (si->player->is_admin() || (modsToo && si->player->is_mod()))) {
			si->player->tell(msg);
		}
	}
}

void Server::kickall(World * const wrld) {
	for (auto it = connsws.begin(); it != connsws.end();) {
		auto client = *it++;
		SocketInfo const * const si = (SocketInfo *)client.getUserData();
		if (si->player && si->player->get_world() == wrld && !si->player->is_admin()) {
			si->player->safedelete(true);
		}
	}
}

void Server::kickall() {
	for (auto it = connsws.begin(); it != connsws.end();) {
		auto client = *it++;
		SocketInfo const * const si = (SocketInfo *)client.getUserData();
		if (si->player && !si->player->is_admin()) {
			si->player->safedelete(true);
		} else if (!si->player) {
			client.close();
		}
	}
}

void Server::kickip(const std::string & ip) {
	bool useless = true;
	for (auto it = connsws.begin(); it != connsws.end();) {
		auto client = *it++;
		SocketInfo const * const si = (SocketInfo *)client.getUserData();
		if (si->ip == ip) {
			useless = false;
			client.close();
		}
	}
	if (!useless) {
		admintell("DEVKicked IP: " + ip, true);
		conns.erase(ip);
	}
}

void Server::clearexpbans() {
	int64_t now = js_date_now();
	for (auto it = ipban.begin(); it != ipban.end();) {
		auto ban = *it++;
		if (now >= ban.second) {
			admintell("DEVBan expired for: " + ban.first, true);
			ipban.erase(ban->first);
		}
	}
}

void Server::banip(const std::string & ip, int64_t until) {
	if (ipban.emplace(ip, until).second) {
		admintell("DEVBanned IP: " + ip + ", until T" + std::to_string(until), true);
		auto search = conns.find(ip);
		if (search != conns.end()) {
			kickip(ip);
		}
	}
}

std::unordered_map<std::string, int64_t> * Server::getbans() {
	return &ipban;
}

std::unordered_set<std::string> * Server::getwhitelist() {
	return &ipwhitelist;
}

std::unordered_set<std::string> * Server::getblacklist() {
	return &ipblacklist;
}

void Server::whitelistip(const std::string & ip) {
	if (ipwhitelist.emplace(ip).second) {
		admintell("DEVWhitelisted IP: " + ip);
	}
}

void Server::set_max_ip_conns(uint8_t max) {
	maxconns = max;
	bool remaining = true;
	while (remaining) {
		remaining = false;
		for (auto client : connsws) {
			SocketInfo const * const si = (SocketInfo *)client.getUserData();
			auto search = conns.find(si->ip);
			if (search != conns.end() && search->second > max) {
				remaining = true;
				if (si->player) {
					if (si->player->is_admin()) {
						remaining = false;
						continue;
					}
					si->player->safedelete(true);
				} else {
					client.close();
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
	for (auto & ip : ipwhitelist) {
		auto search = conns.find(ip);
		if (search != conns.end() && search->second > 0) {
			return;
		}
	}
	set_lockdown(false);
}

void Server::set_lockdown(bool state) {
	lockdown = state;
	if (lockdown) {
		for (auto client : connsws) {
			SocketInfo const * const si = (SocketInfo *)client.getUserData();
			if (si->player && si->player->is_admin()) {
				ipwhitelist.emplace(si->ip);
			}
		}
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
				SocketInfo const * const si = (SocketInfo *)client.getUserData();
				if (si->origin == "(None)") {
					remaining = true;
					client.close();
					break;
				}
			}
		}
		admintell("DEVSuspicious banning enabled.", true);
	} else {
		admintell("DEVSuspicious banning disabled.", true);
	}
}

void Server::set_proxycheck(bool state) {
	proxy_lock = state;
	if (!proxy_lock) {
		/* Removes all pending requests with this url */
		hcli.removeRequests("http://check.getipintel.net/check.php");
		for (auto & ip : proxyquery_checking) {
			/* Kick these IPs because we're not sure if they are proxies or not */
			kickip(ip);
		}
		proxyquery_checking.clear();
		admintell("DEVProxy check disabled.", true);
	} else {
		admintell("DEVProxy check enabled.", true);
	}
}

bool Server::is_connected(const uWS::WebSocket<uWS::SERVER> ws) {
	return connsws.find(ws) != connsws.end();
}

void Server::writefiles() {
	std::ofstream file("bans.txt", std::ios_base::trunc);
	for (auto ip : ipban) {
		file << ip->first << " " << ip->second << std::endl;
	}
	file.flush();
	file.close();
	file.open("whitelist.txt", std::ios_base::trunc);
	for (auto & ip : ipwhitelist) {
		file << ip << std::endl;
	}
	file.flush();
	file.close();
	file.open("blacklist.txt", std::ios_base::trunc);
	for (auto & ip : ipblacklist) {
		file << ip << std::endl;
	}
	file.flush();
	file.close();
}

void Server::readfiles() {
	std::string ip;
	std::ifstream file("bans.txt");
	while (file.good()) {
		std::getline(file, ip);
		if (ip.size() > 0) {
			size_t keylen = ip.find_first_of(' ');
			if (keylen != std::string::npos) {
				try {
					ipban[ip.substr(0, keylen)] = std::stol(ip.substr(keylen + 1));
				} catch (std::invalid_argument & e) { }
				  catch (std::out_of_range & e) { }
			} else {
				ipban[ip] = -1;
			}
		}
		ip.clear();
	}
	file.close();
	std::cout << ipban.size() << " bans read." << std::endl;
	file.open("whitelist.txt");
	while (file.good()) {
		std::getline(file, ip);
		if (ip.size() > 0) {
			ipwhitelist.emplace(ip);
		}
		ip.clear();
	}
	file.close();
	std::cout << ipwhitelist.size() << " whitelists read." << std::endl;
	file.open("blacklist.txt");
	while (file.good()) {
		std::getline(file, ip);
		if (ip.size() > 0) {
			ipblacklist.emplace(ip);
		}
		ip.clear();
	}
	file.close();
	std::cout << ipblacklist.size() << " blacklists read." << std::endl;
}
