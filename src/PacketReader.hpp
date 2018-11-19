#pragma once

#include <unordered_map>
#include <functional>

#include <misc/explints.hpp>
#include <misc/fwd_uWS.h>

class Client;

class PacketReader {
	using OpCode = u8;
	std::unordered_map<OpCode, std::function<void(Client&, const u8 *, sz_t)>> handlers;

public:
	PacketReader(uWS::Hub&);

	template<typename Packet, typename Func>
	void on(Func);
};

#include "PacketReader.tpp"
