#include <User.hpp>

#include <nlohmann/json.hpp>

User::User()
: uid(1),
  username("Guest"),
  isGuest(true) { }

User::User(u64 uid, std::string s)
: uid(uid),
  username(std::move(s)),
  isGuest(false) { }

void to_json(nlohmann::json& j, const User& ui) {
	j["uid"] = ui.uid;
	j["username"] = ui.username;
	j["guest"] = ui.isGuest;
}
