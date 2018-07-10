#include "Chunk.hpp"

#include <vector>

#include <Database.hpp>

Chunk::Chunk(const i32 cx, const i32 cy, const u32 bgclr, Database * const db)
	: db(db),
	  bgclr(bgclr),
	  cx(cx),
	  cy(cy),
	  changed(false),
	  ranked(db->getChunkProtection(cx, cy)) {
	if(!db->get_chunk(cx, cy, (char *)&data)){
		for (sz_t i = 0; i < sizeof(data); i++) {
			data[i] = (u8) (bgclr >> ((i % 3) * 8));
		}
		//memset(data, 255, sizeof(data)); // infra req: change color
	}
}

Chunk::~Chunk() {
	save();
}

bool Chunk::set_data(const u8 x, const u8 y, const RGB clr) {
	const u16 pos = (y * 16 + x) * 3;
	if(data[pos] == clr.r && data[pos + 1] == clr.g && data[pos + 2] == clr.b){
		return false;
	}
	data[pos] = clr.r;
	data[pos + 1] = clr.g;
	data[pos + 2] = clr.b;
	changed = true;
	return true;
}

sz_t Chunk::compress_data_to(u8 (&msg)[16 * 16 * 3 + 10 + 4]) {
	const u16 s = 16 * 16 * 3;
	struct compressedPoint {
		u16 pos;
		u16 length;
	};
	std::vector<compressedPoint> compressedPos;
	u16 compBytes = 3;
	u32 lastclr = data[2] << 16 | data[1] << 8 | data[0];
	u16 t = 1;
	for (u16 i = 3; i < sizeof(data); i += 3) {
		u32 clr = data[i + 2] << 16 | data[i + 1] << 8 | data[i];
		compBytes += 3;

		if (clr == lastclr) {
			++t;
		} else {
			if (t >= 3) {
				compBytes -= t * 3 + 3;
				compressedPos.push_back({compBytes, t});
				compBytes += 5 + 3;
			}
			lastclr = clr;
			t = 1;
		}
	}

	if (t >= 3) {
		compBytes -= t * 3;
		compressedPos.push_back({compBytes, t});
		compBytes += 5;
	}

	const u16 totalcareas = compressedPos.size();
	//std::cout << compBytes + totalcareas * 2 << std::endl;
	msg[0] = CHUNKDATA;
	memcpy(&msg[1], &cx, 4);
	memcpy(&msg[5], &cy, 4);
	memcpy(&msg[9], &ranked, 1);
	u8 * curr = &msg[10];
	memcpy(curr, &s, sizeof(u16));
	curr += sizeof(u16);
	memcpy(curr, &totalcareas, sizeof(u16));
	curr += sizeof(u16);
	for (auto point : compressedPos) {
		memcpy(curr, &point.pos, sizeof(u16));
		curr += sizeof(u16);
	}
	sz_t di = 0;
	sz_t ci = 0;
	for (auto point : compressedPos) {
		while (ci < point.pos) {
			curr[ci++] = data[di++];
		}
		memcpy(curr + ci, &point.length, sizeof(u16));
		ci += sizeof(u16);
		curr[ci++] = data[di++];
		curr[ci++] = data[di++];
		curr[ci++] = data[di++];
		di += point.length * 3 - 3;
	}
	while (di < s) {
		curr[ci++] = data[di++];
	}
	return compBytes + totalcareas * 2 + 10 + 2 + 2;
}

uWS::WebSocket<uWS::SERVER>::PreparedMessage * Chunk::get_prepd_data_msg() {
	u8 msg[16 * 16 * 3 + 10 + 4];
	sz_t size = compress_data_to(msg);
	uWS::WebSocket<uWS::SERVER>::PreparedMessage * prep = uWS::WebSocket<uWS::SERVER>::prepareMessage(
			(char *) &msg[0], size, uWS::BINARY, false);
	return prep;
}

void Chunk::send_data(uWS::WebSocket<uWS::SERVER> * ws, bool compressed) {
	u8 msg[16 * 16 * 3 + 10 + 4];
	sz_t size = compress_data_to(msg);
	ws->send((const char *)&msg[0], size, uWS::BINARY);
}

u8 * Chunk::get_data() {
	return data;
}

void Chunk::set_data(char const * const newdata, sz_t size) {
	memcpy(data, newdata, size);
	changed = true;
}

void Chunk::save() {
	if(changed){
		changed = false;
		db->set_chunk(cx, cy, (char *)&data);
		/* std::cout << "Chunk saved at X: " << cx << ", Y: " << cy << std::endl; */
	}
}

void Chunk::clear(const RGB clr) {
	u32 rgb = clr.b << 16 | clr.g << 8 | clr.r;
	for (sz_t i = 0; i < sizeof(data); i++) {
		data[i] = (u8) (rgb >> ((i % 3) * 8));
	}
	changed = true;
}
