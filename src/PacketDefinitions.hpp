#pragma once

#include <Packet.hpp>
#include <User.hpp>
#include <Player.hpp>
#include <World.hpp>

namespace net {
// to client
enum tc : u8 {
	AUTH_PROGRESS,
	AUTH_OK,
	AUTH_ERROR,
	CMD_LIST,
	CMD_RESULT,
	SHOW_PLAYERS,
	HIDE_PLAYERS,
	CLIENT_DATA,
	/*PLAYER_JOIN,
	PLAYER_LEFT,*/
	WORLD_UPDATE,
	CHAT_MESSAGE,
	PROTECTION_UPD

	/*SET_ID,
	UPDATE,
	CHUNKDATA,
	TELEPORT,
	PERMISSIONS,
	CAPTCHA_REQUIRED,
	SET_PQUOTA*/
};

using Cursor = std::tuple<Player::Id, World::Pos, World::Pos, Player::Step, Player::Tid>;
using Pixel  = std::tuple<World::Pos, World::Pos, u8, u8, u8>;

} // namespace net

// Packet definitions, clientbound
using AuthProgress = Packet<net::tc::AUTH_PROGRESS, std::type_index>;
using AuthOk       = Packet<net::tc::AUTH_OK,       std::string, u64, std::string, bool>;
using AuthError    = Packet<net::tc::AUTH_ERROR,    std::type_index>;

using PlayersShow = Packet<net::tc::SHOW_PLAYERS, std::vector<std::tuple<User::Id, net::Cursor>>>;
using PlayersHide = Packet<net::tc::HIDE_PLAYERS, std::vector<Player::Id>>;
using ClientData  = Packet<net::tc::CLIENT_DATA,  net::Cursor>;

using WorldUpdate      = Packet<net::tc::WORLD_UPDATE,   std::vector<net::Cursor>, std::vector<net::Pixel>>;
using ChatMessage      = Packet<net::tc::CHAT_MESSAGE,   User::Id, std::string>;
using ProtectionUpdate = Packet<net::tc::PROTECTION_UPD, Chunk::ProtPos, Chunk::ProtPos, u32>;

// Packet definitions, serverbound

