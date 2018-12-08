#include "Storage.hpp"

#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <memory>
#include <fstream>
#include <cstdio>
#include <glob.h>
#include <cstdlib>
#include <array>

#include <Chunk.hpp>

#include <misc/PngImage.hpp>
#include <misc/rle.hpp>
#include <misc/BufferHelper.hpp>
#include <misc/utils.hpp>

bool operator<(const twoi32& a, const twoi32& b) {
	return a.pos < b.pos;
}

WorldStorage::WorldStorage(std::string worldDir, std::string worldName)
: PropertyReader(worldDir + "/props.txt"),
  worldDir(std::move(worldDir)),
  worldName(std::move(worldName)) {
	if (!fileExists(this->worldDir) && !makeDir(this->worldDir)) {
		throw std::runtime_error("Couldn't create world directory: " + this->worldDir);
	}

	loadProtectionData();
	std::string pattern(this->worldDir + "/r.*.pxr");
	glob_t result; // XXX: careful with exceptions here
	if (int err = glob(pattern.c_str(), GLOB_NOSORT, nullptr, &result)) {
		if (err != GLOB_NOMATCH) {
			std::cerr << "glob() error: " << err << std::endl;
		}
		return;
	}

	sz_t s = this->worldDir.size() + 3;
	for (sz_t i = 0; i < result.gl_pathc; i++) {
		char * c = result.gl_pathv[i];
		// we can assume that the string will be as long
		// as this path + "/r."
		c += s;
		twoi32 pos;
		pos.x = std::strtol(c, &c, 10);
		pos.y = std::strtol(c + 1, &c, 10);
		remainingOldClusters.emplace(pos);
	}

	globfree(&result);

	std::cout << "World " << getWorldName() << " has " << remainingOldClusters.size() << " old clusters left" << std::endl;
}

WorldStorage::WorldStorage(std::tuple<std::string, std::string> args)
: WorldStorage(std::move(std::get<0>(args)), std::move(std::get<1>(args))) { }

WorldStorage::~WorldStorage() {
	saveProtectionData();
}

const std::string& WorldStorage::getWorldName() const {
	return worldName;
}

const std::string& WorldStorage::getWorldDir() const {
	return worldDir;
}

std::string WorldStorage::getChunkFilePath(i32 x, i32 y) const {
	return worldDir + "/r." + std::to_string(x) + "." + std::to_string(y) + ".png";
}

bool WorldStorage::save() {
	return writeToDisk();
}

bool WorldStorage::hasMotd() {
	return hasProp("motd");
}

bool WorldStorage::hasPassword() {
	return hasProp("password");
}

u16 WorldStorage::getPixelRate() {
	u16 rate = 32;
	if (hasProp("paintrate")) try {
		u32 value = stoul(getProp("paintrate"));
		rate = value > u16(-1) ? u16(-1) : value; // XXX: what if it's 0?
	} catch(const std::exception& e) {
		std::cerr << "Invalid pixel paint rate specified in world cfg" << worldName << ", resetting. (" << e.what() << ")" << std::endl;
		delProp("paintrate");
	}
	return rate;
}

RGB_u WorldStorage::getBackgroundColor() const {
	RGB_u clr = {255, 255, 255};
	if (hasProp("bgcolor")) try {
		std::string s(getProp("bgcolor"));
		if (s.compare(0, 2, "0x") != 0) {
			// rrggbb
			if (s.size() < 8) {
				s += std::string(8 - s.size(), '0');
			}

			clr.rgb = stoul(s, nullptr, 16);
			// switch bytes around so they're read in the correct HTML color order
			std::swap(clr.r, clr.a);
			std::swap(clr.g, clr.b);
		} else {
			clr.rgb = stoul(s, nullptr, 16);
		}
	} catch(const std::exception& e) {
		std::cerr << "Invalid color specified in world cfg" << worldName << ", resetting. (" << e.what() << ")" << std::endl;
		//delProp("bgcolor");
	}
	return clr;
}

std::string WorldStorage::getMotd() const {
	return getProp("motd");
}

std::string WorldStorage::getPassword() {
	return getProp("password");
}

bool WorldStorage::getGlobalModeratorsAllowed() {
	return getProp("disablemods", "false") != "true";
}

void WorldStorage::setPixelRate(u16 v) {
	setProp("paintrate", std::to_string(v));
}

void WorldStorage::setBackgroundColor(RGB_u clr) {
	setProp("bgcolor", std::string("0x") + n2hexstr(clr.rgb));
}

void WorldStorage::setMotd(std::string s) {
	setProp("motd", std::move(s));
}

void WorldStorage::setPassword(std::string s) {
	setProp("password", std::move(s));
}

void WorldStorage::setGlobalModeratorsAllowed(bool b) {
	if (!b) {
		setProp("disablemods", "true");
	} else {
		delProp("disablemods");
	}
}

void WorldStorage::convertNext() {
	if (remainingOldClusters.size() == 0) return;
	twoi32 p = *remainingOldClusters.begin();
	maybeConvert(p.x, p.y);
}

void WorldStorage::maybeConvertChunk(Chunk::Pos x, Chunk::Pos y) {
	int times = 512 / Chunk::size; // assuming result is power of 2
	maybeConvert(x >> times - 1, y >> times - 1);
}

void WorldStorage::maybeConvert(i32 x, i32 y) {
	twoi32 pos;
	pos.x = x;
	pos.y = y;

	if (Chunk::size > 512) {
		throw std::logic_error("can't convert chunks bigger than 512x512");
	}

	if (remainingOldClusters.erase(pos) == 0) {
		return;
	}
	// 512 = old region file dimensions
	int times = 512 / Chunk::size;
	std::unique_ptr<u8[]> buf;

	{
		std::string name(worldDir + "/r." + std::to_string(x) + "." + std::to_string(y) + ".pxr");
		std::ifstream file(name, std::ios::binary | std::ios::ate);
		sz_t size = file.tellg();
		file.seekg(0);
		buf = std::make_unique<u8[]>(size);
		file.read((char*)buf.get(), size);
		file.close();
		if (int err = std::remove(name.c_str())) {
			std::string e("Couldn't delete old cluster file " + name);
			std::perror(e.c_str());
		}
	}

	u8 * ptr = buf.get();
	for (int i = 0; i < times; i++) {
		for (int j = 0; j < times; j++) {
			PngImage result(Chunk::size, Chunk::size, getBackgroundColor());

			u32 offx = j * (32 / times);
			u32 offy = i * (32 / times);

			result.applyTransform([&result, ptr, offx, offy] (u32 x, u32 y) -> RGB_u {
				u32 cx = offx + (x >> 4);
				u32 cy = offy + (y >> 4);

				const u32 lookup = 3 * ((cx & 31) + (cy & 31) * 32);
				u32 pos = ((ptr[lookup + 1] & 0xFF) << 16)
						| ((ptr[lookup] & 0xFF) << 8) | (ptr[lookup + 2] & 0xFF);

				if (pos == 0) {
					return result.getPixel(x, y);
				}

				//std::cout << pos << std::endl;
				u8 * ptrpx = ptr + pos + ((y & 0xF) * 16 + (x & 0xF)) * 3;
				RGB_u px;
				px.r = ptrpx[0];
				px.g = ptrpx[1];
				px.b = ptrpx[2];
				return px;
			});

			i32 chunkx = x * times + j;
			i32 chunky = y * times + i;

			std::string path(getChunkFilePath(chunkx, chunky));
			auto search = pclust.find(pos.pos);
			if (search != pclust.end()) {
				std::array<u32, Chunk::pc * Chunk::pc> prtect;
				prtect.fill(0);
				std::vector<twoi32>& dat = search->second;

				dat.erase(std::remove_if(dat.begin(), dat.end(), [&prtect, chunkx, chunky] (twoi32 p) {
					if (Chunk::protectionAreaSize > 16) {
						throw std::logic_error("can't convert to bigger protection area sizes");
					}

					// this chunkx
					i32 tcx = p.x >> Chunk::pcShift;
					i32 tcy = p.y >> Chunk::pcShift;
					if (!(tcx == chunkx && tcy == chunky)) {
						return false;
					}

					u16 pTimes = 16 / Chunk::protectionAreaSize;


					u16 x = p.x * pTimes & Chunk::pc - 1;
					u16 y = p.y * pTimes & Chunk::pc - 1;

					for (int k = 0; k < pTimes; k++) {
						for (int l = 0; l < pTimes; l++) {
							prtect[(y + k) * Chunk::pc + (x + l)] = 1;
						}
					}

					return true;
				}), dat.end());

				if (dat.size() == 0) {
					pclust.erase(search);
				}

				result.setChunkWriter("woPp", [&prtect] {
					return rle::compress(prtect.data(), prtect.size());
				});
				result.writeFile(path); // scoped array, can't fall through
				continue;
			}
			result.writeFile(path);
		}
	}
}

void WorldStorage::saveProtectionData() {
	std::string name(worldDir + "/pchunks.bin");
	std::vector<twoi32> data;
	for (auto& v : pclust) {
		data.insert(data.end(), v.second.begin(), v.second.end());
	}

	if (data.size() == 0) {
		if (fileExists(name)) {
			std::remove(name.c_str());
		}
		return;
	}

	std::ofstream file(name, std::ios::binary | std::ios::trunc);
	if (!file) {
		throw std::runtime_error("Couldn't open file " + name);
	}

	file.write(reinterpret_cast<const char*>(data.data()), data.size() * sizeof(twoi32));
}

void WorldStorage::loadProtectionData() {
	std::string name(worldDir + "/pchunks.bin");
	std::ifstream file(name, std::ios::binary | std::ios::ate);
	if (file) {
		const sz_t size = file.tellg();
		if (size % 8) { // not multiple of 8?
			std::cerr << "Protection file corrupted, at: "
				<< worldDir << ", ignoring." << std::endl;
		} else {
			const sz_t itemsonfile = size / 8;
			std::cout << "Sorting protected chunk data (" << itemsonfile << " prot. chunks left, world: "<< getWorldName() << ")" << std::endl;
			auto rankedarr(std::make_unique<u64[]>(itemsonfile));
			file.seekg(0);
			file.read((char*)rankedarr.get(), size);

			for (sz_t i = 0; i < itemsonfile;) {
				twoi32 s, c;
				s.pos = rankedarr[i];
				c.x = s.x >> 5;
				c.y = s.y >> 5;
				auto sr = pclust.find(c.pos);
				if (sr == pclust.end()) {
					pclust.emplace(std::make_pair(c.pos, std::vector<twoi32>{s}));
				} else {
					sr->second.push_back(s);
				}

				i++;
				if (!(i % 2048) || i == itemsonfile) {
					std::cout << "Sort progress: " << i << "/" << itemsonfile << " (" << (i * 100 / itemsonfile) << "%)" << std::endl;
				}
			}
		}
	}
}

Storage::Storage(std::string bPath)
: PropertyReader(bPath + "/props.txt"),
  basePath(std::move(bPath)),
  worldsDir(getOrSetProp("worldfolder", "world_data")),
  worldDirPath(basePath + "/" + worldsDir),
  bm(basePath + "/bans.json") {
	if (!fileExists(basePath) && !makeDir(basePath)) {
		throw std::runtime_error("Couldn't access/create directory: " + basePath);
	}

	if (!fileExists(worldDirPath) && !makeDir(worldDirPath)) {
		throw std::runtime_error("Couldn't access/create world directory: " + worldDirPath);
	}
}

std::string Storage::getRandomPassword() {
	return randomStr(10);
}

std::string Storage::getModPass() {
	return getOrSetProp("modpass", getRandomPassword());
}

std::string Storage::getAdminPass() {
	return getOrSetProp("adminpass", getRandomPassword());
}

std::string Storage::getBindAddress() {
	return getOrSetProp("bindto", "0.0.0.0");
}

u16 Storage::getBindPort() {
	u16 port = 13374;
	try {
		u32 z = std::stoul(getOrSetProp("port", "13374"));
		if (z > u16(-1)) {
			throw std::out_of_range("Out of uint16 range");
		}
		port = z & 0xFFFF;
	} catch(const std::exception& e) {
		std::cerr << "Invalid port in server cfg (" << e.what() << ")" << std::endl;
		setProp("port", "13374");
	}
	return port;
}

void Storage::setModPass(std::string s) {
	setProp("modpass", std::move(s));
}

void Storage::setAdminPass(std::string s) {
	setProp("adminpass", std::move(s));
}

void Storage::setBindAddress(std::string s) {
	setProp("bindto", std::move(s));
}

void Storage::setBindPort(u16 p) {
	setProp("port", std::to_string(p));
}

BansManager& Storage::getBansManager() {
	return bm;
}

std::tuple<std::string, std::string> Storage::getWorldStorageArgsFor(const std::string& worldName) {
	return {worldDirPath + "/" + worldName, worldName};
}
