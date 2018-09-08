#include <UserInfo.hpp>

#include <nlohmann/json.hpp>

UserInfo::UserInfo()
: uid(1),
  username("Guest"),
  isGuest(true) { }

UserInfo::UserInfo(u64 uid, std::string s)
: uid(uid),
  username(std::move(s)),
  isGuest(false) { }

void to_json(nlohmann::json& j, const UserInfo& ui) {
	j["uid"] = ui.uid;
	j["username"] = ui.username;
	j["guest"] = ui.isGuest;
}
