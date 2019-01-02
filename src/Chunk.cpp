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
  pngCacheOutdated(true),
  pngFileOutdated(false) {
	bool readerCalled = false;
  	auto fail = [this] {
  		std::cerr << "Protection data corrupted for chunk "
		          << this->x << ", " << this->y << ". Resetting." << std::endl;
		protectionData.fill(0);
		pngFileOutdated = true;
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
		data.allocate(Chunk::size, Chunk::size, ws.getBackgroundColor());
		protectionData.fill(0);
	}

	preventUnloading(false);
}

Chunk::~Chunk() {
	save();
	unlockChunk();
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
		data.writeFile(ws.getChunkFilePath(x, y));
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
