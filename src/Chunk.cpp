#include "Chunk.hpp"

#include <misc/rle.hpp>
#include <vector>

#include <Database.hpp>

Chunk::Chunk(i32 x, i32 y, WorldStorage& ws)
: x(x),
  y(y),
  ws(ws),
  pngCacheOutdated(true),
  pngFileOutdated(false) {
  	bool wasRead = false;
  	auto fail = [&protectionData, x, y] {
  		std::cerr << "Protection data corrupted for chunk "
		          << x << ", " << y << ". Resetting." << std::endl;
		std::fill(std::begin(protectionData), std::end(protectionData), 0);
  	};

	data.setChunkReader("woPp", [&wasRead, &protectionData, fail] (u8 * d, sz_t size) {
		constexpr sz_t arrSize = sizeof(protectionData) / sizeof(u32);
		try {
			if (rle::getItems<u32>(d, size) != arrSize) {
				fail();
				return false;
			}
			rle::decompress(d, size, &protectionData[0], arrSize);
			wasRead = true;
		} catch(std::length_error& e) {
			fail();
		}
	});

	data.setChunkWriter("woPp", [&protectionData] () {
		constexpr sz_t arrSize = sizeof(protectionData) / sizeof(u32);
		return rle::compress(&protectionData[0], arrSize);
	});
}

Chunk::~Chunk() {
	save();
}

void Chunk::setPixel(u16 x, u16 y, RGB clr) {
	data.setPixel(x, y, clr);
	changed = true;
}


u8 * Chunk::get_data() {
	return data;
}

void Chunk::set_data(char const * const newdata, sz_t size) {
	memcpy(data, newdata, size);
	changed = true;
}

void Chunk::save() {
	if (changed) {
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
