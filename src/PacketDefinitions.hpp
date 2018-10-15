#pragma once

#include <Packet.hpp>
#include <UserInfo.hpp>
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
	CHAT_MESSAGE

	/*SET_ID,
	UPDATE,
	CHUNKDATA,
	TELEPORT,
	PERMISSIONS,
	CAPTCHA_REQUIRED,
	SET_PQUOTA,
	CHUNK_PROTECTED*/
};

using Cursor = std::tuple<Player::Id, Player::Pos, Player::Pos, Player::Tid>;
using Pixel  = std::tuple<World::Pos, World::Pos, u8, u8, u8>;

} // namespace net

// Packet definitions, clientbound
using AuthProgress = Packet<net::tc::AUTH_PROGRESS, std::type_index>;
using AuthOk       = Packet<net::tc::AUTH_OK,       std::string, u64, std::string, bool>;
using AuthError    = Packet<net::tc::AUTH_ERROR,    std::type_index>;

// to be deleted (?)
using CmdList   = Packet<net::tc::CMD_LIST,   std::vector<std::tuple<std::string, std::vector<std::tuple<std::type_index, std::string, std::vector<std::tuple<std::string, std::type_index, bool>>>>>>>;
using CmdResult = Packet<net::tc::CMD_RESULT, bool, std::string>; // jsonstring

using PlayersShow = Packet<net::tc::SHOW_PLAYERS, std::vector<std::tuple<UserInfo::Id, net::Cursor>>>;
using PlayersHide = Packet<net::tc::HIDE_PLAYERS, std::vector<Player::Id>>;
using ClientData  = Packet<net::tc::CLIENT_DATA,  net::Cursor>;

using WorldUpdate = Packet<net::tc::WORLD_UPDATE, std::vector<net::Cursor>, std::vector<net::Pixel>>;
using ChatMessage = Packet<net::tc::CHAT_MESSAGE, UserInfo::Id, std::string>;

// Packet definitions, serverbound

