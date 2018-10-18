#include "PacketReader.hpp"

#include <Client.hpp>

#include <iostream>
#include <uWS.h>

PacketReader::PacketReader(uWS::Hub& h) {
	h.onMessage([this] (uWS::WebSocket<uWS::SERVER> * ws, const char * msg, sz_t len, uWS::OpCode oc) {
		Client * cl = static_cast<Client *>(ws->getUserData());
		if (cl == nullptr || len == 0) {
			return;
		}

		auto search = handlers.find(OpCode(msg[0]));
		if (oc == uWS::OpCode::TEXT || search == handlers.end()) {
			ws->close(1003);
			return;
		}

		cl->updateLastActionTime();

		try {
			search->second(*cl, reinterpret_cast<const u8 *>(msg + 1), len - 1);
		} catch (const std::length_error& e) {
			std::cout << "Exception on packet read: " << e.what() << std::endl;
			ws->close(1002);
		}
	});
}
