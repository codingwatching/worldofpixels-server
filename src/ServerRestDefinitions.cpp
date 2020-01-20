#include "Server.hpp"

#include <iostream>

#include <WorldManager.hpp>
#include <User.hpp>
#include <Player.hpp>
#include <World.hpp>

#include <shared_ptr_ll.hpp>
#include <utils.hpp>
#include <base64.hpp>

#include <nlohmann/json.hpp>

std::function<void(ll::shared_ptr<Request>)> cancelHandler(std::function<bool()> f) {
	if (!f) {
		return nullptr;
	}

	return [cancel{std::move(f)}] (auto) {
		if (!cancel()) {
			std::cout << "Failed to cancel query" << std::endl;
		}
	};
}

void Server::registerEndpoints() {
	api.on(ApiProcessor::MGET)
		.path("sso")
	.end([this] (ll::shared_ptr<Request> req, std::string_view) {
		auto ssotok = req->getQueryParam("ssotoken");
		if (!ssotok) {
			req->writeStatus("400 Bad Request");
			req->end();
			return;
		}

		req->onCancel(cancelHandler(am.useSsoToken(*ssotok, "owop", [req{std::move(req)}] (auto token, bool persistent) {
			if (req->isCancelled()) {
				return;
			}

			if (!token) {
				req->writeStatus("400 Bad Request");
				req->end("INVALID_SSO_TOKEN");
				return;
			}

			// either we refresh the cookie on uvias.com, or on owop. refreshing
			// it only on owop would be a problem since it would let the cookie on
			// uvias expire. So we just refresh the cookie on uvias.com and set this
			// one to never expire, if the session is persistent
			std::vector<std::string_view> directives{"Secure", "HttpOnly", "Path=/", "SameSite=lax"};
			if (persistent) {
				// string data shouldn't be destroyed since it's a string literal
				directives.emplace_back("expires=Fri, 31 Dec 9999 23:59:59 GMT");
			}

			req->writeStatus("303 See Other");
			req->setCookie("uviastoken", *token, std::move(directives));
			// redirect to main page
			req->writeHeader("Location", "/");
			req->end();
		})));
	});

	//////////////////// /users

	// Ban user
	api.on(ApiProcessor::MPOST)
		.path("users")
		.var()
		.path("ban")
	.end([this] (ll::shared_ptr<Request> req, std::string_view, User::Id uid) {

	});

	/////////////////////// /worlds

	api.on(ApiProcessor::MGET) // Get world
		.path("worlds")
		.var()
	.end([this] (ll::shared_ptr<Request> req, std::string_view, std::string worldName) {
		if (wm.isLoaded(worldName)) {
			req->end(wm.getOrLoadWorld(worldName));
		} else {
			req->writeStatus("404 Not Found");
			req->end();
		}
	});

	api.on(ApiProcessor::MPATCH) // Modify world
		.path("worlds")
		.var()
	.end([this] (ll::shared_ptr<Request> req, std::string_view, std::string worldName) {

	});

	api.on(ApiProcessor::MGET)
		.path("worlds")
		.var()
		.path("view")
		.var()
		.var()
		/*.var()*/
	.end([this] (ll::shared_ptr<Request> req, std::string_view, std::string worldName, i32 x, i32 y/*, u8 downscaling*/) {
		if (!wm.verifyWorldName(worldName)/* || downscaling > 16 || downscaling == 0
				|| (downscaling & (downscaling - 1)) != 0*/) { // not power of 2
			req->writeStatus("400 Bad Request");
			req->end();
			return;
		}

		if (!wm.isLoaded(worldName)) {
			// you can't view worlds which are not loaded...
			// TODO: ...that you're not on?
			req->writeStatus("404 Not Found");
			req->end();
			return;
		}

		World& world = wm.getOrLoadWorld(worldName);

		// will encode the png in another thread if necessary and end the request when done
		world.sendChunk(x, y, /*downscaling,*/ std::move(req));
	});

	api.on(ApiProcessor::MPOST) // Switch world
		.path("worlds")
		.var()
		.path("cursors")
		.var()
		.path("move")
		.path("world")
		.var()
	.end([this] (ll::shared_ptr<Request> req, std::string_view, std::string worldName, Player::Id pid, std::string newWorldName) {

	});

	api.on(ApiProcessor::MPOST) // Teleport to pos
		.path("worlds")
		.var()
		.path("cursors")
		.var()
		.path("move")
		.path("pos")
		.var()
		.var()
	.end([this] (ll::shared_ptr<Request> req, std::string_view, std::string worldName, Player::Id pid, World::Pos x, World::Pos y) {

	});

	api.on(ApiProcessor::MPOST) // Teleport to player
		.path("worlds")
		.var()
		.path("cursors")
		.var()
		.path("move")
		.path("player")
		.var()
	.end([this] (ll::shared_ptr<Request> req, std::string_view, std::string worldName, Player::Id fromPid, Player::Id toPid) {

	});

	api.on(ApiProcessor::MPOST) // Kick player
		.path("worlds")
		.var()
		.path("cursors")
		.var()
		.path("kick")
	.end([this] (ll::shared_ptr<Request> req, std::string_view, std::string worldName, Player::Id pid) {

	});

	/*api.on(ApiProcessor::MGET) // Get world roles
		.path("worlds")
		.var()
		.path("roles")
	.end([this] (ll::shared_ptr<Request> req, std::string_view, std::string worldName) {

	});

	api.on(ApiProcessor::MPOST) // Create world role
		.path("worlds")
		.var()
		.path("roles")
	.end([this] (ll::shared_ptr<Request> req, std::string_view, std::string worldName) {

	});

	api.on(ApiProcessor::MPATCH) // Modify world role
		.path("worlds")
		.var()
		.path("roles")
		.var()
	.end([this] (ll::shared_ptr<Request> req, std::string_view, std::string worldName, std::string roleId) {

	});

	api.on(ApiProcessor::MDELETE) // Delete world role
		.path("worlds")
		.var()
		.path("roles")
		.var()
	.end([this] (ll::shared_ptr<Request> req, std::string_view, std::string worldName, std::string roleId) {

	});

	api.on(ApiProcessor::MPUT) // Add user to world role
		.path("worlds")
		.var()
		.path("roles")
		.var()
		.path("user")
		.var()
	.end([this] (ll::shared_ptr<Request> req, std::string_view, std::string worldName, std::string roleId, User::Id uid) {

	});

	api.on(ApiProcessor::MDELETE) // Delete user from world role
		.path("worlds")
		.var()
		.path("roles")
		.var()
		.path("user")
		.var()
	.end([this] (ll::shared_ptr<Request> req, std::string_view, std::string worldName, std::string roleId, User::Id uid) {

	});*/

	/////////////////////// /chats

	api.on(ApiProcessor::MGET) // Get chat
		.path("chats")
		.var()
	.end([this] (ll::shared_ptr<Request> req, std::string_view, std::string chatId) {

	});

	api.on(ApiProcessor::MGET) // Get messages
		.path("chats")
		.var()
		.path("messages")
	.end([this] (ll::shared_ptr<Request> req, std::string_view, std::string chatId) {

	});

	api.on(ApiProcessor::MPOST) // Send message
		.path("chats")
		.var()
		.path("messages")
	.end([this] (ll::shared_ptr<Request> req, std::string_view, std::string chatId) {

	});

	api.on(ApiProcessor::MGET) // Get message
		.path("chats")
		.var()
		.path("messages")
		.var()
	.end([this] (ll::shared_ptr<Request> req, std::string_view, std::string chatId, std::string messageId) {

	});

	api.on(ApiProcessor::MPATCH) // Edit message
		.path("chats")
		.var()
		.path("messages")
		.var()
	.end([this] (ll::shared_ptr<Request> req, std::string_view, std::string chatId, std::string messageId) {

	});

	api.on(ApiProcessor::MDELETE) // Delete message
		.path("chats")
		.var()
		.path("messages")
		.var()
	.end([this] (ll::shared_ptr<Request> req, std::string_view, std::string chatId, std::string messageId) {

	});

	/////////////////////// /emotes

	/*api.on(ApiProcessor::MGET)
		.path("emotes")
		.var()
	.end([this] (ll::shared_ptr<Request> req, std::string_view, std::string emoteId) {

	});

	api.on(ApiProcessor::MGET)
		.path("emotes")
		.var()
		.path("image")
	.end([this] (ll::shared_ptr<Request> req, std::string_view, std::string emoteId) {

	});*/

	///////////////////////
	// Extras
	///////////////////////

	api.on(ApiProcessor::MGET)
		.path("status")
	.end([this] (ll::shared_ptr<Request> req, std::string_view) {
		Ip ip(req->getIp());

		bool banned = bm.isBanned(ip);

		nlohmann::json j = {
			{ "motd", "Almost done, I think!" },
			{ "curl", {
				{ "active", ac.activeHandles() },
				{ "queued", ac.queuedRequests() }
			}},
			{ "sql", {
				{ "connected", ap.isConnected() },
				{ "queued", ap.queuedQueries() }
			}},
			{ "uptime", std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - startupTime).count() }, // lol
			{ "yourIp", ip },
			{ "banned", banned },
			{ "tps", wm.getTps() }
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
}
