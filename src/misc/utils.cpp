#include "utils.hpp"

#ifndef __WIN32
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <unistd.h>
#else
	#include <windows.h>
#endif

#include <algorithm>
#include <chrono>
#include <random>

bool makeDir(const std::string& dir) {
#ifndef __WIN32
	return mkdir(dir.c_str(), 0755) == 0;
#else
	/* conversion from windows bool to c++ bool necessary? */
	return CreateDirectory(dir.c_str(), nullptr) == TRUE;
#endif
}

bool fileExists(const std::string& path) {
#ifndef __WIN32
	return access(path.c_str(), F_OK) != -1;
#else
	DWORD dwAttrib = GetFileAttributes(path.c_str());

	return dwAttrib != INVALID_FILE_ATTRIBUTES;
#endif
}

sz_t getUtf8StrLen(const std::string& str) {
	sz_t j = 0, i = 0, x = 1;
	while (i < str.size()) {
		if (x > 4) { /* Invalid unicode */
			return -1;
		}

		if ((str[i] & 0xC0) != 0x80) {
			j += x == 4 ? 2 : 1;
			x = 1;
		} else {
			x++;
		}
		i++;
	}
	if (x == 4) {
		j++;
	}
	return (j);
}

i64 jsDateNow() {
	namespace c = std::chrono;

	auto time = c::system_clock::now().time_since_epoch();
	return c::duration_cast<c::milliseconds>(time).count();
}

std::vector<std::string> tokenize(const std::string& str,
		char delimiter, bool trimEmpty) {
	sz_t pos, lastPos = 0, length = str.length();
	std::vector<std::string> tokens;
	while (lastPos < length + 1) {
		pos = str.find_first_of(delimiter, lastPos);
		if (pos == std::string::npos) {
			pos = length;
		}

		if (pos != lastPos || !trimEmpty) {
			tokens.emplace_back(str.data() + lastPos, (sz_t)pos - lastPos);
		}

		lastPos = pos + 1;
	}

	return tokens;
}

std::string randomStr(sz_t size) { // WARNING: RNG not thread safe
	std::string str(size, '\0');

	std::generate(str.begin(), str.end(), [] {
		static const char alphanum[] =
			"0123456789?_@#$!"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"abcdefghijklmnopqrstuvwxyz";

		// std::random_device{}() returns a random number, used as seed for the
		// std::default_random_engine
		static std::default_random_engine rng(std::random_device{}());
		static std::uniform_int_distribution<sz_t> get(0, sizeof(alphanum) - 2);

		return alphanum[get(rng)];
	});

	return str;
}
