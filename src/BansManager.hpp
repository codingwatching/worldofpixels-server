#pragma once

#include <string>
#include <map>

#include <Ip.hpp>
#include <explints.hpp>

#include <nlohmann/json_fwd.hpp>

struct BanInfo {
	i64 expiresOn;
	std::string reason;
};

void to_json(nlohmann::json&, const BanInfo&);
void from_json(const nlohmann::json&, BanInfo&);

class BansManager {
	std::string bansFilePath;
	std::map<Ip, BanInfo> bans;
	bool bansChanged;

public:
	BansManager(std::string path);
	~BansManager();

	void readBanlist();
	void writeBanlist();

	void clearExpiredBans();
	void resetBanlist();

	bool isBanned(Ip);
	const BanInfo& getInfoFor(Ip);

	void ban(Ip, u64 seconds, std::string reason = "");
	bool unban(Ip);
};
