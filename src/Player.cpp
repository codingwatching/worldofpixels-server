#include "Player.hpp"

#include <iostream>

#include <Client.hpp>
#include <World.hpp>

Player::Player(Client& c, World& w, u32 pid, i32 startX, i32 startY)
: cl(c),
  world(w),
  playerId(pid),
  x(0),
  y(0),
  chatAllowed(true), // TODO: get from world
  cmdsAllowed(true),
  modifyWorldAllowed(true) {
  	if (startX != x || startY != y) {
		teleportTo(startX, startY);
  	}
  	std::cout << "New player on world: " << w.getWorldName() << ", PID: " << pid << ", UID: " << getUserInfo().uid << std::endl;
}

Player::~Player() {
	world.removePlayer(*this);
}

bool Player::canChat() const {
	return chatAllowed;
}

bool Player::canUseCommands() const {
	return cmdsAllowed;
}

bool Player::canModifyWorld() const {
	return modifyWorldAllowed;
}

Client& Player::getClient() const {
	return cl;
}

World& Player::getWorld() const {
	return world;
}

UserInfo& Player::getUserInfo() const {
	return cl.getUserInfo();
}

i32 Player::getX() const {
	return x;
}

i32 Player::getY() const {
	return y;
}

u32 Player::getPlayerId() const {
	return playerId;
}

void Player::teleportTo(i32 newX, i32 newY) {
	x = newX;
	y = newY;
	world.updatePlayer(*this);
}

void Player::tell(const std::string& s) {
	send({
		{"t", "pm"},
		{"from", 0},
		{"msg", s}
	});
}

void Player::tryPaint(i32 x, i32 y, RGB_u rgb) {
	world.paint(x, y, rgb);
}

void Player::tryMoveTo(i32 newX, i32 newY) {
	x = newX;
	y = newY;
	world.updatePlayer(*this);
}

void Player::tryChat(std::string s) {
	world.chat(p, std::move(s));
}

void Player::send(const u8 * buf, sz_t len, bool text) {
	cl.send(buf, len, text);
}

void Player::send(const nlohmann::json& j) {
	std::string s(j.dump());
	send(reinterpret_cast<const u8*>(s.data()), s.size(), true);
}

bool Player::operator ==(const Player& p) const {
	// compare client classes, which will compare socket pointers
	return cl == p.cl;
}
