#pragma once

#include <tuple>

#include <misc/explints.hpp>
#include <misc/fwd_uWS.h>

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

template<u8 opCode, typename... Args>
class Packet {
	// i don't want to include <uWS.h> in headers, and there
	// is no way to forward-declare a nested struct
	void * uwsPreparedMessage;

public:
	Packet(Args... args);
	~Packet(); // finalizeMessage

	void * getPrepared();

	// known at compile time, useless when size of >=1 item isn't constant, eg: string
	static constexpr bool isStatic();
	static constexpr sz_t staticBufSize();

	// maybe could be changed to std::optional to avoid exceptions
	// NOTE: doesn't read opcode!
	static std::tuple<Args...> fromBuffer(u8 * buffer, sz_t size);

	// send to a single socket
	static void one(uWS::WebSocket<true> * ws, Args... args);
};
