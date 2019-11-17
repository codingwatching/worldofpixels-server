#pragma once

#include <utility>
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <functional>
#include <unordered_map>

#include <UviasRank.hpp>
#include <Session.hpp>
#include <User.hpp>

#include <explints.hpp>
#include <shared_ptr_ll.hpp>

class AsyncPostgres;
class SessionChecker;

class AuthManager {
	AsyncPostgres& uvdb;
	std::unordered_map<UviasRank::Id, UviasRank> ranks;
	std::unordered_map<std::string, ll::weak_ptr<Session>> sessions; // token as key
	std::unordered_map<User::Id, ll::weak_ptr<User>> userCache;

public:
	AuthManager(AsyncPostgres&);

	static std::optional<std::pair<u64, std::array<u8, 16>>> parseToken(std::string_view token);

	std::optional<UviasRank> getRank(UviasRank::Id) const;
	void updateRank(UviasRank);

	ll::shared_ptr<User> getUser(User::Id);
	void reloadUser(User::Id); // if name or any other thing changed

	// Returns nullptr if there is no active session by this token
	ll::shared_ptr<Session> getSession(std::string_view);
	bool kickSession(std::string_view);
	void forEachSession(std::function<void(const std::string&, ll::shared_ptr<Session>)>);

	std::function<bool()> useSsoToken(std::string_view ssoToken, std::string_view serviceId, std::function<void(std::optional<std::string>, bool)>);

private:
	std::function<bool()> loadSession(std::string_view, std::function<void(ll::shared_ptr<Session>)>);

	friend SessionChecker;
};
