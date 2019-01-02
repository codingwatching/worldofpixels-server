#pragma once

#include <string>
#include <atomic>

#include <explints.hpp>

class Client;

enum server_messages : u8 {
	SET_ID,
	UPDATE,
	CHUNKDATA,
	TELEPORT,
	PERMISSIONS,
	CAPTCHA_REQUIRED,
	SET_PQUOTA,
	CHUNK_PROTECTED
};

struct pinfo_t {
	i32 x;
	i32 y;
	u8 r;
	u8 g;
	u8 b;
	u8 tool;
};

struct pixpkt_t {
	i32 x;
	i32 y;
	u8 r;
	u8 g;
	u8 b;
} __attribute__((packed));

struct pixupd_t {
	u32 id;
	i32 x;
	i32 y;
	u8 r;
	u8 g;
	u8 b;
} __attribute__((packed));

struct chunkpos_t {
	i32 x;
	i32 y;
};

enum captcha_verify_state : u8 {
	CA_WAITING,
	CA_VERIFYING,
	CA_VERIFIED,
	CA_OK,
	CA_INVALID
};
