#pragma once

#ifdef __WIN32
#define WIN32_EXPORT __declspec(dllexport)
#else
#define WIN32_EXPORT
#endif

namespace uS {
	struct Socket;
	struct Loop;
	struct Timer;
	struct Poll;
	struct Async;
	struct Timepoint;
}

namespace uWS {
	/*enum {
		CLIENT,
		SERVER
	};*/

	struct Header;
	struct HttpRequest;
	struct HttpResponse;
	struct Hub;

	template<bool isServer>
	struct Group;

	template<bool isServer>
	struct WIN32_EXPORT HttpSocket;

	template<bool isServer>
	struct WebSocketState;

	template <const bool isServer>
	struct WIN32_EXPORT WebSocket;
}

#undef WIN32_EXPORT