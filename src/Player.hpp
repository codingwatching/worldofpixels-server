#pragma once

#include <string>
#include <functional>

#include <nlohmann/json_fwd.hpp>

#include <World.hpp>
#include <misc/explints.hpp>
#include <misc/color.hpp>
#include <misc/Bucket.hpp>

class Client;
class World;
class UserInfo;

class Player {
public:
	using Id = u32;
	using Tid = u8; // ToolId
	using Step = u8; // Extra precision for X and Y
	class Builder;

private:
	Client& cl;
	World& world;
	const Id playerId;
	World::Pos x;
	World::Pos y;
	Bucket chatLimiter;
	Bucket paintLimiter;
	bool chatAllowed;
	bool cmdsAllowed;
	bool modifyWorldAllowed;
	Tid toolId;
	Step pixelStep;

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

	void tryPaint(World::Pos x, World::Pos y, RGB_u);
	void tryMoveTo(Pos x, Pos y, Tid toolId);
	void tryChat(const std::string&);

	void send(Packet&);

	bool operator ==(const Player&) const;
	bool operator  <(const Player&) const;
};

class Player::Builder {
	Client * cl;
	World * wo;
	Player::Id playerId;
	Player::Pos startX;
	Player::Pos startY;
	Bucket paintLimiter;
	Bucket chatLimiter;
	bool chatAllowed;
	bool cmdsAllowed;
	bool modifyWorldAllowed;

public:
	Builder();

	Builder& setClient(Client&);
	Builder& setWorld(World&);
	Builder& setPlayerId(Player::Id);
	Builder& setSpawnPoint(Player::Pos, Player::Pos);
	Builder& setPaintBucket(Bucket);
	Builder& setChatBucket(Bucket);
	Builder& setChatAllowed(bool);
	Builder& setCmdsAllowed(bool);
	Builder& setModifyWorldAllowed(bool);

	//Player build(); // not useful until c++17 (copy ellision)

	friend Player;
};
