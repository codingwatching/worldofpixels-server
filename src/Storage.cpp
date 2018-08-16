#include "Storage.hpp"

#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <memory>
#include <fstream>
#include <cstdio>
#include <vector>
#include <map>

#include <misc/utils.hpp>
#include <config.hpp>

WorldStorage::WorldStorage(std::string worldDir, std::string worldName)
: PropertyReader(worldDir + "/props.cfg"),
  worldDir(std::move(worldDir)),
  worldName(std::move(worldName)) {
	if (!fileExists(this->worldDir) && !makeDir(this->worldDir)) {
		throw std::runtime_error("Couldn't create world directory: " + this->worldDir);
	}
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

void WorldStorage::save() {
	writeToDisk();
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

RGB WorldStorage::getBackgroundColor() const {
	RGB clr = {255, 255, 255};
	if (hasProp("bgcolor")) try {
		clr.rgb = stoul(getProp("bgcolor"));
		// switch bytes around so they're read in the correct HTML color order
		//clr.rgb = (clr.rgb & 0xFF) << 16 | (clr.rgb & 0xFF00) | (clr.rgb & 0xFF0000) >> 16;
	} catch(const std::exception& e) {
		std::cerr << "Invalid color specified in world cfg" << worldName << ", resetting. (" << e.what() << ")" << std::endl;
		//delProp("bgcolor");
	}
	return clr;
}

std::string WorldStorage::getMotd() {
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

void WorldStorage::setBackgroundColor(RGB clr) {
	setProp("bgcolor", std::to_string(clr.rgb));
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

void WorldStorage::convertProtectionData(std::function<void(i32, i32)> f) {
	std::string name(worldDir + "/pchunks.bin");
	std::ifstream file(name, std::ios::binary | std::ios::ate);
	if (file) {
		const sz_t size = file.tellg();
		if (size % 8) { // not multiple of 8?
			std::cerr << "Protection file corrupted, at: "
				<< worldDir << ", ignoring." << std::endl;
		} else {
			const sz_t itemsonfile = size / 8;
			std::cout << "Starting conversion of protected chunk data (" << itemsonfile << " chunks)" << std::endl;
			auto rankedarr(std::make_unique<u64[]>(itemsonfile));
			file.seekg(0);
			file.read((char*)rankedarr.get(), size);
			union twoi32 {
				struct {
					i32 x;
					i32 y;
				} p;
				u64 pos;
			};
			std::map<u64, std::vector<twoi32>> clust;
			for (sz_t i = 0; i < itemsonfile; i++) {
				twoi32 s, c;
				s.pos = rankedarr[i];
				c.p.x = s.p.x >> 5;
				c.p.y = s.p.y >> 5;
				auto sr = clust.find(c.pos);
				if (sr == clust.end()) {
					clust.emplace(std::make_pair(c.pos, std::vector<twoi32>{s}));
				} else {
					sr->second.push_back(s);
				}
				if (!(i % 1024)) {
					std::cout << "Sort progress: " << i << "/" << itemsonfile << " (" << (i * 100 / itemsonfile) << "%)" << std::endl;
				}
			}
			sz_t i = 0;
			sz_t c = 0;
			for (const auto& v : clust) {
				for (const twoi32 s : v.second) {
					f(s.p.x, s.p.y);
					if (!(i % 512)) {
						std::cout << "Conv. progress: " << i << "/" << itemsonfile << " (" << (i * 100 / itemsonfile) << "%, c: " << c << ")" << std::endl;
					}
					++i;
				}
				++c;
			}
		}
		std::remove(name.c_str());
	}
}

Storage::Storage(std::string bPath)
: PropertyReader(bPath + "/props.cfg"),
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

WorldStorage Storage::getWorldStorageFor(std::string worldName) {
	return WorldStorage(worldDirPath + "/" + worldName, worldName);
}
