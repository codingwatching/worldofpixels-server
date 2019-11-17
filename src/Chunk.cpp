#include "Chunk.hpp"

#include <vector>
#include <iostream>
#include <fstream>
#include <cstdio> // std::remove

#include <rle.hpp>
#include <utils.hpp>
#include <Storage.hpp>

static_assert((Chunk::size & (Chunk::size - 1)) == 0,
	"Chunk::size must be a power of 2");

static_assert((Chunk::protectionAreaSize & (Chunk::protectionAreaSize - 1)) == 0,
	"Chunk::protectionAreaSize must be a power of 2");

static_assert((Chunk::size % Chunk::protectionAreaSize) == 0,
	"Chunk::size must be divisible by Chunk::protectionAreaSize");

static_assert((Chunk::pc & (Chunk::pc - 1)) == 0,
	"size / protectionAreaSize must result in a power of 2");

Chunk::Chunk(Pos x, Pos y, const WorldStorage& ws)
: lastAction(std::chrono::steady_clock::now()),
  x(x),
  y(y),
  ws(ws),
  canUnload(false), // DON'T unload before this is constructed (can happen by alloc fail)
  protectionDataEmpty(false),
  pngCacheOutdated(true),
  pngFileOutdated(false) {
	bool readerCalled = false;
  	auto fail = [this] {
  		std::cerr << "Protection data corrupted for chunk "
		          << this->x << ", " << this->y << ". Resetting." << std::endl;
		protectionData.fill(0);
		pngFileOutdated = true;
		protectionDataEmpty = true;
  	};

	data.setChunkReader("woPp", [this, fail{std::move(fail)}, &readerCalled] (u8 * d, sz_t size) {
		// returning false will throw
		// instead of stopping the server, reset the protections
		// for this chunk
		readerCalled = true;
		try {
			if (rle::getItems<u32>(d, size) != protectionData.size()) {
				fail();
				return true;
			}

			rle::decompress(d, size, protectionData.data(), protectionData.size());
		} catch(std::length_error& e) {
			fail();
			return true;
		}

		return true;
	});

	data.setChunkWriter("woPp", [this] {
		std::shared_lock<std::shared_timed_mutex> _(sm);
		if (protectionDataEmpty) {
			// don't write protection data if it's all 0
			return std::pair<std::unique_ptr<u8[]>, sz_t>{nullptr, 0};
		}

		return rle::compress(protectionData.data(), protectionData.size());
	});


	if (data.readFile(ws.getChunkFilePath(x, y))) {
		if (!readerCalled) {
			protectionData.fill(0);
			protectionDataEmpty = true;
		}
	} else {
		data.allocate(Chunk::size, Chunk::size, ws.getBackgroundColor());
		protectionData.fill(0);
		protectionDataEmpty = true;
	}

	preventUnloading(false);
}

Chunk::~Chunk() {
	if (isChunkEmpty()) {
		std::string fpath(ws.getChunkFilePath(x, y));
		if (int err = std::remove(fpath.c_str())) {
			std::string s("Couldn't delete chunk file (" + fpath + ")");
			std:perror(s.c_str());
		}

		return;
	}

	try {
		save();
	} catch (const std::runtime_error& e) {
		std::cerr << "Error while saving chunk: " << e.what() << std::endl;
	}
}

bool Chunk::setPixel(u16 x, u16 y, RGB_u clr) {
	x &= Chunk::size - 1;
	y &= Chunk::size - 1;

	if (data.getPixel(x, y).rgb != clr.rgb) {
		updateLastActionTime();
#warning "Fix possible concurrent access"
		data.setPixel(x, y, clr); // XXX: possible concurrent access... must be looked at
		pngFileOutdated = true;
		pngCacheOutdated = true;
		return true;
	}

	return false;
}

void Chunk::setProtectionGid(ProtPos x, ProtPos y, u32 gid) {
	//updateLastActionTime();
	x &= Chunk::pc - 1;
	y &= Chunk::pc - 1;

	pngFileOutdated = true;
	pngCacheOutdated = true;

	std::unique_lock<std::shared_timed_mutex> _(sm);
	protectionDataEmpty = false;
	protectionData[y * Chunk::pc + x] = gid;
}

u32 Chunk::getProtectionGid(ProtPos x, ProtPos y) const {
	x &= Chunk::pc - 1;
	y &= Chunk::pc - 1;

	//std::shared_lock<std::shared_timed_mutex> _(sm);
	return protectionData[y * Chunk::pc + x];
}

bool Chunk::isPngCacheOutdated() const {
	return pngCacheOutdated;
}

void Chunk::unsetCacheOutdatedFlag() {
	pngCacheOutdated = false;
}

void Chunk::updatePngCache() {
	data.writeFileOnMem(pngCache);
	// pngCacheOutdated = false;
}

const std::vector<u8>& Chunk::getPngData() const {
	return pngCache;
}

bool Chunk::save() {
	if (pngFileOutdated) {
		std::string fpath(ws.getChunkFilePath(x, y));
		if (pngCacheOutdated) {
			data.writeFile(fpath);
		} else {
			std::ofstream f(fpath, std::ios::out | std::ios::binary | std::ios::trunc);
			if (!f) {
				throw std::runtime_error("Couldn't open file: " + fpath);
			}

			f.write(reinterpret_cast<char *>(pngCache.data()), pngCache.size());
		}

		pngFileOutdated = false;
		return true;
	}

	return false;
}

void Chunk::updateLastActionTime() {
	lastAction = std::chrono::steady_clock::now();
}

std::chrono::steady_clock::time_point Chunk::getLastActionTime() const {
	return lastAction;
}

bool Chunk::shouldUnload(bool ignoreTime) const {
	return canUnload && (ignoreTime || std::chrono::steady_clock::now() - lastAction > std::chrono::minutes(1));
}

void Chunk::preventUnloading(bool state) {
	canUnload = !state;
}

bool Chunk::isChunkEmpty() {
	// very slow
	for (u32 i = 0; i < protectionData.size(); i++) {
		if (protectionData[i] != 0) {
			return false;
		}
	}

	if (!protectionDataEmpty) {
		protectionDataEmpty = true;
		pngCacheOutdated = true;
		pngFileOutdated = true;
	}

	RGB_u bgclr = ws.getBackgroundColor();
	for (u32 y = 0; y < data.getHeight(); y++) {
		for (u32 x = 0; x < data.getWidth(); x++) {
			if (data.getPixel(x, y).rgb != bgclr.rgb) {
				return false;
			}
		}
	}

	return true;
}
