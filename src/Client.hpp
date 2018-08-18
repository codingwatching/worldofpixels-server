#pragma once

#include <uWS.h>
#include <string>

#include <misc/color.hpp>
#include <misc/explints.hpp>
#include <misc/Bucket.hpp>
#include <types.hpp>

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
	u32 id;
	std::string nick;
	Bucket pixupdlimit;
	Bucket chatlimit;
	i64 lastMovement;
	uWS::WebSocket<uWS::SERVER> * ws;
	World& wrld;
	u16 penalty;
	u8 rank;
	bool stealthadmin;
	bool suspicious;
	pinfo_t pos;
	RGB_u lastclr;

public:
	SocketInfo * si;
	bool mute;

	Client(uWS::WebSocket<uWS::SERVER> *, World&, SocketInfo * si);
	~Client();

	bool can_edit();

	void put_px(const i32 x, const i32 y, const RGB_u);
	void del_chunk(const i32 x, const i32 y, const RGB_u);

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
	World& get_world() const;
	u16 get_penalty() const;
	u8 get_rank() const;
	i64 get_last_move() const;
	u32 get_id() const;

	void set_stealth(bool);
	void set_nick(const std::string&);
	void set_pbucket(u16 rate, u16 per);
	void set_id(u32);
};
