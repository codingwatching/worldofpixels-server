#pragma once

#include <uWS.h>

#include <misc/color.hpp>
#include <misc/explints.hpp>
#include <types.hpp>

class Database;

class Chunk {
	Database * const db;
	const u32 bgclr;
	const i32 cx;
	const i32 cy;
	u8 data[16 * 16 * 3];
	bool changed;

public:
	bool ranked;

	Chunk(const i32 cx, const i32 cy, const u32 bgclr, Database * const);
	~Chunk();

	sz_t compress_data_to(u8 (&msg)[16 * 16 * 3 + 10 + 4]);
	uWS::WebSocket<uWS::SERVER>::PreparedMessage * get_prepd_data_msg();

	bool set_data(const u8 x, const u8 y, const RGB);
	void send_data(uWS::WebSocket<uWS::SERVER> *, bool compressed = false);
	void save();

	void clear(const RGB);
	u8 * get_data();
	void set_data(char const * const, sz_t);
};
