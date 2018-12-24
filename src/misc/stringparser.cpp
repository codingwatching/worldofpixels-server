#include "stringparser.hpp"

#include <stdexcept>
#include <type_traits>
#include <limits>
#include <misc/explints.hpp>

template<typename T>
typename std::enable_if<!std::is_same<T, bool>::value && std::is_integral<T>::value, T>::type
fromString(const std::string& s) {
	using L = std::numeric_limits<T>;
	if /*constexpr*/ (L::is_signed) {
		i64 n = std::stoll(s);
		if (n > L::max() || n < L::min()) {
			throw std::out_of_range("Value too big");
		}

		return n;
	} else {
		if (s.size() == 0 || s[0] == '-') {
			throw std::out_of_range("Value too small");
		}

		u64 n = std::stoull(s);
		if (n > L::max()) {
			throw std::out_of_range("Value too big");
		}

		return n;
	}
}

template<typename T>
typename std::enable_if<std::is_same<T, bool>::value, T>::type
fromString(const std::string& s) {
	if (s.size() != 0) {
		switch(s[0]) {
			case 't':
			case 'T':
				return true;
				
			case 'f':
			case 'F':
				return false;
		}
	}

	throw std::invalid_argument("Invalid bool");
}

template<typename T>
typename std::enable_if<std::is_same<T, std::string>::value, T>::type
fromString(const std::string& s) {
	return s;
}

template<typename T>
typename std::enable_if<std::is_same<T, float>::value, T>::type
fromString(const std::string& s) {
	return std::stof(s);
}

template<typename T>
typename std::enable_if<std::is_same<T, double>::value, T>::type
fromString(const std::string& s) {
	return std::stod(s);
}


// explicit instantiations
template u8 fromString<u8>(const std::string&);
template u16 fromString<u16>(const std::string&);
template u32 fromString<u32>(const std::string&);
template u64 fromString<u64>(const std::string&);

template i8 fromString<i8>(const std::string&);
template i16 fromString<i16>(const std::string&);
template i32 fromString<i32>(const std::string&);
template i64 fromString<i64>(const std::string&);

template bool fromString<bool>(const std::string&);
template std::string fromString<std::string>(const std::string&);
template float fromString<float>(const std::string&);
template double fromString<double>(const std::string&);
