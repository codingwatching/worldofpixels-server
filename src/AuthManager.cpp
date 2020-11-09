#include "AuthManager.hpp"

#include <charconv>
#include <iostream>
#include <algorithm>
#include <chrono>

#include <AsyncPostgres.hpp>
#include <base64.hpp>
#include <utils.hpp>

bool check16ByteaToken(std::string_view token) {
	if (token.size() != 32) {
		return false;
	}

	for (char c : token) {
		if (!isxdigit(c)) {
			return false;
		}
	}

	return true;
}

AuthManager::AuthManager(AsyncPostgres& uvdb)
: uvdb(uvdb) { }

std::optional<std::pair<u64, std::array<u8, 16>>> AuthManager::parseToken(std::string_view token) {
	sz_t toksz = token.size();
	// 16 + 1 + 24 (uid, separator, session id)
	if (!(toksz == 41 && token[16] == '|' && token[40] == '=' && token[39] == '=')) { // assume b64 part always has 2 pads
		return std::nullopt;
	}

	sz_t pos = 0;
	u64 uid;

	auto res = std::from_chars(token.data(), token.data() + toksz, uid, 16);
	pos = res.ptr - token.data(); // amount read

	if (res.ec == std::errc::invalid_argument || res.ec == std::errc::result_out_of_range || pos != 16) { // expect 25 more chars ('|' separator, and base64 part)
		std::cout << "from_chars didn't read uid correctly, read: " << pos << ", " << token[pos] << std::endl;
		return std::nullopt;
	}

	pos++; // skip separator

	std::array<u8, 16> data;
	int read = base64Decode(token.data() + pos, toksz - pos, data.data(), data.size());

	if (read != data.size()) {
		std::cout << "b64 dec didn't read full input data: " << read << std::endl;
		return std::nullopt;
	}

	return std::pair<u64, std::array<u8, 16>>{uid, data};
}

std::optional<UviasRank> AuthManager::getRank(UviasRank::Id id) const {
	auto it = ranks.find(id);
	if (it != ranks.end()) {
		return it->second;
	}

	return std::nullopt;
}

void AuthManager::updateRank(UviasRank rank) {
	auto it = ranks.find(rank.getId());
	if (it != ranks.end() && !it->second.deepEqual(rank)) {
		// rank changed, signal update to every user
		it->second = rank;

		// this is probably quite slow
		for (auto& usrp : userCache) {
			if (auto usr = usrp.second.lock(); usr && usr->getUviasRank().getId() == rank.getId()) {
				usr->updateUser(rank);
			}
		}
	} else {
		ranks.insert_or_assign(rank.getId(), rank);
	}
}

ll::shared_ptr<User> AuthManager::getUser(User::Id uid) {
	auto it = userCache.find(uid);
	if (it != userCache.end()) {
		if (auto usr = it->second.lock()) {
			return usr;
		} else {
			// pointer expired, delete
			userCache.erase(it);
		}
	}

	return {};
}

ll::shared_ptr<Session> AuthManager::getSession(std::string_view tok) {
	// XXX: this is disgusting, fix once c++ allows is_transparent on unordered maps
	auto it = sessions.find(std::string(tok));
	if (it != sessions.end()) {
		if (auto sess = it->second.lock()) {
			return sess;
		} else {
			// pointer expired, delete
			sessions.erase(it);
		}
	}

	return {};
}

bool AuthManager::kickSession(std::string_view tok) {
	auto it = sessions.find(std::string(tok));
	if (it != sessions.end()) {
		return true;
	}

	return false;
}

void AuthManager::forEachSession(std::function<void(const std::string&, ll::shared_ptr<Session>)> f) {
	for (const auto& session : sessions) {
		if (auto sessp = session.second.lock()) {
			f(session.first, std::move(sessp));
		}
	}
}

std::function<bool()> AuthManager::useSsoToken(std::string_view ssoToken, std::string_view serviceId, std::function<void(std::optional<std::string>, bool)> cb) {
	if (!check16ByteaToken(ssoToken)) {
		cb(std::nullopt, false);
		return nullptr;
	}

	auto q = uvdb.query("SELECT accounts.build_token(g.uid, g.session_id), persistent "
			"FROM accounts.get_and_del_sso_token(decode($1::CHAR(32), 'hex'), $2::VARCHAR(8)) AS g "
			"INNER JOIN accounts.sessions AS s ON g.uid = s.uid AND g.session_id = s.session_id",
		std::string(ssoToken), std::string(serviceId));

	q->then([cb{std::move(cb)}] (AsyncPostgres::Result r) {
		if (!r.size()) {
			cb(std::nullopt, false);
			return;
		}

		auto [sessionToken, persistent] = r[0].get<std::string, bool>();
		cb(std::move(sessionToken), persistent);
	});

	return [this, q{std::move(q)}] {
		return uvdb.cancelQuery(*q);
	};
}

std::function<bool()> AuthManager::loadSession(std::string_view tokStr, std::function<void(ll::shared_ptr<Session>)> f) {
	auto tok = AuthManager::parseToken(tokStr);
	if (!tok) {
		f(nullptr);
		return nullptr;
	}

	auto q = uvdb.query("SELECT extract(EPOCH FROM u.created)::BIGINT, creator_ip, "
				"username, accounts.get_total_rep(s.uid), rank_id "
			"FROM accounts.get_session($1::BIGINT, $2::BYTEA) AS s "
			"INNER JOIN accounts.users AS u ON u.uid = s.uid",
		static_cast<i64>(tok->first), tok->second);

	q->then([this, cb{std::move(f)}, uid{tok->first}, tokStr{std::string(tokStr)}] (AsyncPostgres::Result r) {
		if (!r.size()) {
			cb(nullptr);
			return;
		}

		auto [creationSecs, ip, username, totalRep, rankId] = r[0].get<i64, Ip, std::string, User::Rep, UviasRank::Id>();

		auto creationTime(std::chrono::system_clock::from_time_t(creationSecs));
		auto usr(getUser(uid));
		if (!usr) {
			std::optional<UviasRank> rank(getRank(rankId));

			if (!rank) {
				cb(nullptr);
				return;
			}

			usr = ll::make_shared<User>(uid, totalRep, *rank, std::move(username));
			userCache.insert_or_assign(uid, usr);
		}

		// this can happen, if the user connects multiple times at the same time
		// some time is wasted on unused queries, if this is the case though
		// could be optimized, but this is the simplest way of fixing it
		auto ses(getSession(tokStr));
		if (!ses) {
			ses = ll::make_shared<Session>(std::move(usr), ip, creationTime);
			sessions.insert_or_assign(tokStr, ses);
		}

		cb(std::move(ses));
	});

	return [this, q{std::move(q)}] {
		return uvdb.cancelQuery(*q);
	};
}
