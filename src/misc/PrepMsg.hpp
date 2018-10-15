#pragma once

#include <misc/explints.hpp>

class PrepMsg {
	// i don't want to include <uWS.h> in headers, and there
	// is no way to forward-declare a nested struct
	void * message;

public:
	PrepMsg(void *);
	PrepMsg(const PrepMsg&);
	~PrepMsg();

	void * getPrepared();

private:
	void setPrepared(u8 *, sz_t);
	void delPrepared();
};
