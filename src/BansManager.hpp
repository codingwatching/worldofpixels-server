#pragma once

#include <string>
#include <map>

#include <misc/explints.hpp>

struct BanInfo {
	i64 expiresOn;
	std::string reason;
};

class BansManager {
	std::string bansFilePath;
	std::map<std::string, BanInfo> bans;
	bool bansChanged;

public:
	BansManager(std::string path);
	~BansManager();

	void readBanlist();
	void writeBanlist();

	void clearExpiredBans();
	void resetBanlist();

	bool isBanned(std::string);
	const BanInfo& getInfoFor(std::string);

	void ban(std::string ip, u64 seconds, std::string reason = "");
	bool unban(std::string ip);
};
