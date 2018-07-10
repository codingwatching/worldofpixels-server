#pragma once

#include <uWS.h>
#include <string>

#include <misc/color.hpp>
#include <misc/explints.hpp>
#include <misc/Bucket.hpp>
#include <types.hpp>

class Bucket;
class World;

class Client {
public:
	enum Rank : u8 {
		NONE = 0,
		USER = 1,
		MODERATOR = 2,
		ADMIN = 3
	};
private:
	std::string nick;
	Bucket dellimit;
	Bucket pixupdlimit;
	Bucket chatlimit;
	i64 lastMovement;
	uWS::WebSocket<uWS::SERVER> * ws;
	World * const wrld;
	u16 penalty;
	bool handledelete;
	u8 rank;
	bool stealthadmin;
	bool suspicious;
	bool compressionEnabled;
	pinfo_t pos;
	RGB lastclr;

public:
	const u32 id;
	SocketInfo * si;
	bool mute;

	Client(const u32 id, uWS::WebSocket<uWS::SERVER> *, World * const, SocketInfo * si);
	~Client();

	bool can_edit();

	void get_chunk(const i32 x, const i32 y) const;
	void put_px(const i32 x, const i32 y, const RGB);
	void del_chunk(const i32 x, const i32 y, const RGB);

	void teleport(const i32 x, const i32 y);
	void move(const pinfo_t&);
	const pinfo_t * get_pos();

	bool can_chat();
	void chat(const std::string&);
	void tell(const std::string&);

	void updated();

	void disconnect();

	void promote(u8, u16);

	bool warn();

	bool is_mod() const;
	bool is_admin() const;
	uWS::WebSocket<uWS::SERVER> * get_ws() const;
	std::string get_nick() const;
	World * get_world() const;
	u16 get_penalty() const;
	u8 get_rank() const;
	i64 get_last_move() const;

	void set_stealth(bool);
	void set_nick(const std::string&);
	void set_pbucket(u16 rate, u16 per);
};
