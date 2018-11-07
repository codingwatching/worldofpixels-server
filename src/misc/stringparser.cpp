#include "stringparser.hpp"

#include <stdexcept>
#include <type_traits>
#include <limits>
#include <misc/explints.hpp>

template<>
bool fromString(const std::string& s) {
	if (s == "true") {
		return true;
	} else if (s == "false") {
		return false;
	}

	throw std::invalid_argument("Invalid bool");
}

template<>
std::string fromString(const std::string& s) {
	return s;
}

template<>
float fromString(const std::string& s) {
	return std::stof(s);
}

template<>
double fromString(const std::string& s) {
	return std::stod(s);
}

template<typename T>
typename std::enable_if<std::is_integral<T>::value, T>::type
fromString(const std::string& s) {
	using L = std::numeric_limits<T>;
	if /*constexpr*/ (L::is_signed) {
		i64 n = std::stoll(s);
		if (n > L::max() || n < L::min()) {
			throw std::out_of_range("Value too big");
		}

		return n;
	} else {
		u64 n = std::stoull(s);
		if (n > L::max()) {
			throw std::out_of_range("Value too big");
		}

		return n;
	}
}

template u8 fromString<u8>(const std::string&);
template u16 fromString<u16>(const std::string&);
template u32 fromString<u32>(const std::string&);
template u64 fromString<u64>(const std::string&);

template i8 fromString<i8>(const std::string&);
template i16 fromString<i16>(const std::string&);
template i32 fromString<i32>(const std::string&);
template i64 fromString<i64>(const std::string&);
