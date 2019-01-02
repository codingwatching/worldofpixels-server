#pragma once

#include <string>
#include <map>

#include <Ipv4.hpp>
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
	std::map<Ipv4, BanInfo> bans;
	bool bansChanged;

public:
	BansManager(std::string path);
	~BansManager();

	void readBanlist();
	void writeBanlist();

	void clearExpiredBans();
	void resetBanlist();

	bool isBanned(Ipv4);
	const BanInfo& getInfoFor(Ipv4);

	void ban(Ipv4, u64 seconds, std::string reason = "");
	bool unban(Ipv4);
};
