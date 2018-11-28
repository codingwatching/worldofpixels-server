#include "utils.hpp"

#ifndef __WIN32
	#include <sys/types.h>
	#include <sys/stat.h>
	#include <unistd.h>
#else
	#include <windows.h>
#endif

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <chrono>
#include <random>
#include <locale>
#include <map>

#ifdef __GNUG__
	#define HAVE_CXA_DEMANGLE
	#include <memory>
	#include <cxxabi.h>
#endif

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

// std::random_device{}() returns a random number, used as seed for the
// std::default_random_engine
static std::default_random_engine rng(std::random_device{}());

std::default_random_engine& getRandomEngine() {
	return rng;
}

u8 randByte() {
	return rng();
}

std::string randomStr(sz_t size) { // WARNING: RNG not thread safe
	std::string str(size, '\0');

	std::generate(str.begin(), str.end(), [] {
		static const char alphanum[] =
			"0123456789?_@#$!"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"abcdefghijklmnopqrstuvwxyz";

		static std::uniform_int_distribution<sz_t> get(0, sizeof(alphanum) - 2);

		return alphanum[get(rng)];
	});

	return str;
}

// trim from start (in place)
void ltrim(std::string& s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [] (char ch) {
        return !std::isspace(ch);
    }));
}

// trim from end (in place)
void rtrim(std::string& s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [] (char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

// trim from both ends (in place)
void trim(std::string& s) {
    ltrim(s);
    rtrim(s);
}

// could go in a different file
std::map<std::type_index, std::string> typeCache;
std::map<std::string, std::type_index> typeMap;

const std::string& demangle(std::type_index type) { // XXX: also not thread safe
	auto search = typeCache.find(type);

	if (search == typeCache.end()) {
#ifdef HAVE_CXA_DEMANGLE
		int s = -1;
		std::unique_ptr<char, void(*)(void*)> result(
			abi::__cxa_demangle(type.name(), NULL, NULL, &s),
			std::free
		);

		std::string name(s == 0 ? result.get() : type.name());
#else
		std::string name(type.name());
#endif
		search = typeCache.emplace(type, name).first;
		typeMap.emplace(name, type);
	}

	return search->second;
}

std::type_index strToType(const std::string& s) {
	return typeMap.at(s); // throws if not found
}

void urldecode(std::string& s) {
	sz_t pos = 0;
	while ((pos = s.find('%', pos)) != std::string::npos) {
		const char * p = s.c_str() + pos; // "%??..."
		if (s.size() - pos < 2 || !std::isxdigit(p[2])) {
			throw std::length_error("Wrong URL percent encoding");
		}

		char tmp = p[3]; // str[str.size()] is guaranteed to be '\0'
		s[pos + 3] = '\0';

		unsigned long ch = std::strtoul(p + 1, nullptr, 16);
		if (ch == -1ul) {
			throw std::logic_error("Invalid URL percent encoding");
		}

		s[pos] = static_cast<char>(ch & 0xFF);

		s[pos + 3] = tmp; // if the string ends here we would always be writing '\0'

		s.erase(pos + 1, 2);
	}
}

void urldecode(std::vector<std::string>& v) {
	for (auto& s : v) {
		urldecode(s);
	}
}
