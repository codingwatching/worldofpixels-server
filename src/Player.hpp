#pragma once

#include <string>
#include <functional>

#include <nlohmann/json_fwd.hpp>

#include <misc/explints.hpp>
#include <misc/color.hpp>
#include <misc/Bucket.hpp>

class Client;
class World;
class UserInfo;

class Player {
public:
	using Id = u32;
	using Pos = i64;
	class Builder;

private:
	Client& cl;
	World& world;
	const Id playerId;
	Pos x;
	Pos y;
	Bucket chatLimiter;
	Bucket paintLimiter;
	bool chatAllowed;
	bool cmdsAllowed;
	bool modifyWorldAllowed;
	u8 toolId;

public:
	Player(const Player&) = delete;

	Player(Client&, World&, Id, Pos, Pos,
		Bucket, Bucket, bool, bool, bool);
	Player(const Player::Builder&);
	~Player();

	bool canChat() const;
	bool canUseCommands() const;
	bool canModifyWorld() const;

	Client& getClient() const;
	World& getWorld() const;
	UserInfo& getUserInfo() const;

	void setPaintRate(u16 rate, u16 per);

	Pos getX() const;
	Pos getY() const;
	Id getPid() const;

	void teleportTo(Pos x, Pos y);
	void tell(const std::string&);

	void tryPaint(Pos x, i32 y, RGB_u);
	void tryMoveTo(i32 x, i32 y, u8 toolId);
	void tryChat(const std::string&);

	void send(const u8 *, sz_t, bool text = false);
	void send(const nlohmann::json&);

	bool operator ==(const Player&) const;
	bool operator  <(const Player&) const;
};

class Player::Builder {
	Client * cl;
	World * wo;
	u32 playerId;
	i32 startX;
	i32 startY;
	Bucket paintLimiter;
	Bucket chatLimiter;
	bool chatAllowed;
	bool cmdsAllowed;
	bool modifyWorldAllowed;

public:
	Builder();

	Builder& setClient(Client&);
	Builder& setWorld(World&);
	Builder& setPlayerId(u32);
	Builder& setSpawnPoint(i32, i32);
	Builder& setPaintBucket(Bucket);
	Builder& setChatBucket(Bucket);
	Builder& setChatAllowed(bool);
	Builder& setCmdsAllowed(bool);
	Builder& setModifyWorldAllowed(bool);

	//Player build(); // not useful until c++17 (copy ellision)

	friend Player;
};
