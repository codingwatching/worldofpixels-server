#include "PrepMsg.hpp"

#include <uWS.h>

PrepMsg::PrepMsg(void * p)
: message(p) { }

PrepMsg::PrepMsg(const PrepMsg& p)
: message(p.message) {
	using uwsPrepMsg = uWS::WebSocket<uWS::SERVER>::PreparedMessage;
	if (message != nullptr) {
		static_cast<uwsPrepMsg *>(message)->references++;
	}
}

PrepMsg::~PrepMsg() {
	delPrepared();
}

void * PrepMsg::getPrepared() const {
	return message;
}

void PrepMsg::setPrepared(u8 * buf, sz_t s) {
	delPrepared();
	message = uWS::WebSocket<uWS::SERVER>::prepareMessage(
		reinterpret_cast<char *>(buf), s, uWS::BINARY, false, nullptr
	);
}

void PrepMsg::delPrepared() {
	using uwsPrepMsg = uWS::WebSocket<uWS::SERVER>::PreparedMessage;
	if (message != nullptr) {
		uWS::WebSocket<uWS::SERVER>::finalizeMessage(
			static_cast<uwsPrepMsg *>(message)
		);
	}
}