#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include <misc/explints.hpp>
#include <misc/color.hpp>

class Client;
class World;
class UserInfo;

class Player {
	Client& cl;
	World& world;
	const u32 playerId;
	i32 x;
	i32 y;
	bool chatAllowed;
	bool cmdsAllowed;
	bool modifyWorldAllowed;

public:
	Player(const Player&) = delete;

	Player(Client&, World&, u32, i32, i32);
	~Player();

	bool canChat() const;
	bool canUseCommands() const;
	bool canModifyWorld() const;

	Client& getClient() const;
	World& getWorld() const;
	UserInfo& getUserInfo() const;

	i32 getX() const;
	i32 getY() const;
	u32 getPlayerId() const;

	void teleportTo(i32 x, i32 y);
	void tell(const std::string&);

	void tryPaint(i32 x, i32 y, RGB_u);
	void tryMoveTo(i32 x, i32 y);
	void tryChat(std::string);

	void send(const u8 *, sz_t, bool text = false);
	void send(const nlohmann::json&);

	bool operator ==(const Player&) const;
};
