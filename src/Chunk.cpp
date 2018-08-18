#include "Chunk.hpp"

#include <vector>
#include <iostream>
#include <fstream>
#include <cstdio> // std::remove

#include <misc/rle.hpp>
#include <misc/utils.hpp>
#include <Storage.hpp>

Chunk::Chunk(i32 x, i32 y, const WorldStorage& ws)
: lastAction(jsDateNow()),
  x(x),
  y(y),
  ws(ws),
  canUnload(true),
  pngCacheOutdated(true),
  pngFileOutdated(false),
  moved(false) {
	bool readerCalled = false;
  	auto fail = [this] {
  		std::cerr << "Protection data corrupted for chunk "
		          << this->x << ", " << this->y << ". Resetting." << std::endl;
		protectionData.fill(0);
		pngFileOutdated = true;
  	};

	data.setChunkReader("woPp", [this, fail, &readerCalled] (u8 * d, sz_t size) {
		readerCalled = true;
		try {
			if (rle::getItems<u32>(d, size) != protectionData.size()) {
				fail();
				return false;
			}
			rle::decompress(d, size, protectionData.data(), protectionData.size());
		} catch(std::length_error& e) {
			fail();
			return false;
		}
		return true;
	});

	data.setChunkWriter("woPp", [this] () {
		std::shared_lock<std::shared_timed_mutex> _(sm);
		return rle::compress(protectionData.data(), protectionData.size());
	});

	std::string file(ws.getChunkFilePath(x, y));

	lockChunk();
	if (fileExists(file)) {
		data.readFile(file);
		if (!readerCalled) {
			protectionData.fill(0);
		}
	} else {
		data.allocate(512, 512, ws.getBackgroundColor());
		protectionData.fill(0);
	}
}

Chunk::Chunk(Chunk&& c)
: lastAction(c.lastAction),
  x(c.x),
  y(c.y),
  ws(c.ws),
  data(std::move(c.data)),
  protectionData(std::move(c.protectionData)),
  pngCache(std::move(c.pngCache)),
  canUnload(c.canUnload),
  pngCacheOutdated(c.pngCacheOutdated),
  pngFileOutdated(c.pngFileOutdated) {
  	// prevent the moved chunk from writing changes on destruction
	c.moved = true;
}

Chunk::~Chunk() {
	if (!moved) {
		save();
		unlockChunk();
	}
}

bool Chunk::setPixel(u16 x, u16 y, RGB_u clr) {
	if (data.getPixel(x, y).rgb != clr.rgb) {
		lastAction = jsDateNow();
		data.setPixel(x, y, clr);
		pngFileOutdated = true;
		pngCacheOutdated = true;
		return true;
	}
	return false;
}

void Chunk::setProtectionGid(u8 x, u8 y, u32 gid) {
	//lastAction = jsDateNow();
	x &= 0x1F;
	y &= 0x1F;
	pngFileOutdated = true;
	pngCacheOutdated = true;
	std::unique_lock<std::shared_timed_mutex> _(sm);
	protectionData[y * 32 + x] = gid;
}

u32 Chunk::getProtectionGid(u8 x, u8 y) const {
	x &= 0x1F;
	y &= 0x1F;
	//std::shared_lock<std::shared_timed_mutex> _(sm);
	return protectionData[y * 32 + x];
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
		data.writeFile(ws.getChunkFilePath(x, y));
		pngFileOutdated = false;
		return true;
	}
	return false;
}

i64 Chunk::getLastActionTime() const {
	return lastAction;
}

bool Chunk::shouldUnload(bool ignoreTime) const {
	return canUnload && (ignoreTime || jsDateNow() - lastAction > 60000);
}

void Chunk::preventUnloading(bool state) {
	canUnload = !state;
}

void Chunk::lockChunk() {
	std::string file(ws.getChunkFilePath(x, y) + ".lock");
	if (fileExists(file)) {
		// what do? server probably didn't close correctly
		// if we reach this condition.
		std::cerr << "!!! Chunk: " << x << ", " << y << ", from world: '" << ws.getWorldName()
			<< "' is already locked. Is another instance of the server running,"
			<< " or was it not closed correctly? Reading anyway." << std::endl;
	}

	if (!std::fstream(file, std::ios::trunc | std::ios::out | std::ios::binary)) {
		std::cerr << "!!! Couldn't create/open lockfile. Permissions problem? Throwing exception "
			<< "to prevent future problems." << std::endl;
		throw std::runtime_error("Could not create chunk lockfile! World: " + ws.getWorldName()
			+ ", Chunk: " + std::to_string(x) + ", " + std::to_string(y) + ".");
	}
}

void Chunk::unlockChunk() {
	std::string file(ws.getChunkFilePath(x, y) + ".lock");
	if (int err = std::remove(file.c_str())) {
		std::string s("Couldn't delete lockfile (" + file + ")");
		std:perror(s.c_str());
	}
}
