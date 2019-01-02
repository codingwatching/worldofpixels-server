#include "BansManager.hpp"

#include <fstream>
#include <iomanip>

#include <nlohmann/json.hpp>

#include <utils.hpp>

void to_json(nlohmann::json& j, const BanInfo& b) {
	j = nlohmann::json({
		{ "expiresOn", b.expiresOn },
		{ "reason", b.reason }
	});
}

void from_json(const nlohmann::json& j, BanInfo& b) {
	b.expiresOn = j.at("expiresOn").get<i64>();
	b.reason = j.at("reason").get<std::string>();
}

BansManager::BansManager(std::string filePath)
: bansFilePath(std::move(filePath)),
  bansChanged(false) {
	readBanlist();
}

BansManager::~BansManager() {
	writeBanlist();
}

void BansManager::readBanlist() { // XXX: no exception handling
	if (fileExists(bansFilePath)) {
		std::ifstream in(bansFilePath);
		nlohmann::json j;
		in >> j;
		bans = j.get<std::map<Ipv4, BanInfo>>();
		bansChanged = false;
	}
}

void BansManager::writeBanlist() {
	if (bansChanged) {
		std::ofstream out(bansFilePath, std::ios::trunc);
		out << std::setw(2) << nlohmann::json(bans) << std::endl;
		bansChanged = false;
	}
}

void BansManager::clearExpiredBans() {
	i64 now = jsDateNow();
	for (auto it = bans.begin(); it != bans.end();) {
		if (now >= it->second.expiresOn && it->second.expiresOn > 0) {
			it = bans.erase(it);
			bansChanged = true;
		} else {
			++it;
		}
	}
}

void BansManager::resetBanlist() {
	bansChanged = true;
	bans.clear();
}

bool BansManager::isBanned(Ipv4 ip) {
	clearExpiredBans();
	return bans.count(ip) != 0;
}

const BanInfo& BansManager::getInfoFor(Ipv4 ip) {
	return bans.at(ip);
}

void BansManager::ban(Ipv4 ip, u64 seconds, std::string reason) {
	i64 timestamp = seconds > 0 ? jsDateNow() + seconds * 10000 : 0;
	bans[ip] = {timestamp, std::move(reason)};
	bansChanged = true;
}

bool BansManager::unban(Ipv4 ip) {
	bool useful = bans.erase(ip);
	if (useful) {
		bansChanged = true;
	}
	return useful;
}
