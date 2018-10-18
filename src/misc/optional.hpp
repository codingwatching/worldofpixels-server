#pragma once

#if __has_include(<optional>)
	#include <optional>
	namespace estd {
		using std::optional;
		using std::nullopt_t;
		using std::nullopt;
	}
#elif __has_include(<experimental/optional>)
	#include <experimental/optional>
	//#pragma message("Using experimental std::optional lib")
	namespace estd {
		using std::experimental::optional;
		using std::experimental::nullopt_t;
		using std::experimental::nullopt;
	}
#else
	#error "<optional> is not supported"
#endif
