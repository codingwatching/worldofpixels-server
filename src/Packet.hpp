#pragma once

#include <tuple>

#include <misc/explints.hpp>
#include <misc/fwd_uWS.h>
#include <misc/PrepMsg.hpp>

template<u8 opCode, typename... Args>
struct Packet : public PrepMsg {
	static constexpr u8 code = opCode;

	Packet(Args... args);

	// maybe could be changed to std::optional to avoid exceptions
	// NOTE: doesn't read opcode!
	static std::tuple<Args...> fromBuffer(const u8 * buffer, sz_t size);

	// send to a single socket
	static void one(uWS::WebSocket<true> * ws, Args... args);
};

template<typename F>
struct fromBufFromLambdaArgs : public fromBufFromLambdaArgs<decltype(&F::operator())> {};

template<typename ClassType, typename ReturnType, typename... Args>
struct fromBufFromLambdaArgs<ReturnType(ClassType::*)(Args...) const> {
	static std::tuple<Args...> call(const u8 * d, sz_t s) {
		return Packet<0, Args...>::fromBuffer(d, s);
	}
};

#include "Packet.tpp"
