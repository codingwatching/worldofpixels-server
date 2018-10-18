#pragma once

#include <string>
#include <functional>

#include <nlohmann/json_fwd.hpp>

#include <misc/explints.hpp>
#include <misc/color.hpp>
#include <misc/Bucket.hpp>

class World; // using World::Pos = i32;
using WorldPos = i32;
class Client;
class UserInfo;
class PrepMsg;

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
	WorldPos x;
	WorldPos y;
	Bucket chatLimiter;
	Bucket paintLimiter;
	bool chatAllowed;
	bool cmdsAllowed;
	bool modifyWorldAllowed;
	Tid toolId;
	Step pixelStep;

public:
	Player(const Player&) = delete;

	Player(Client&, World&, Id, WorldPos, WorldPos,
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

	WorldPos getX() const;
	WorldPos getY() const;
	Step getStep() const;
	Id getPid() const;

	void teleportTo(WorldPos x, WorldPos y);
	void tell(const std::string&);

	void tryPaint(WorldPos x, WorldPos y, RGB_u);
	void tryMoveTo(WorldPos x, WorldPos y, Step precision, Tid toolId);
	void tryChat(const std::string&);

	void send(const PrepMsg&);

	bool operator ==(const Player&) const;
	bool operator  <(const Player&) const;
};

class Player::Builder {
	Client * cl;
	World * wo;
	Player::Id playerId;
	WorldPos startX;
	WorldPos startY;
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
	Builder& setSpawnPoint(WorldPos, WorldPos);
	Builder& setPaintBucket(Bucket);
	Builder& setChatBucket(Bucket);
	Builder& setChatAllowed(bool);
	Builder& setCmdsAllowed(bool);
	Builder& setModifyWorldAllowed(bool);

	//Player build(); // not useful until c++17 (copy ellision)

	friend Player;
};
