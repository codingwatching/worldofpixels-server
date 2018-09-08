#include "Packet.hpp"

#include <uWS.h>

#include <fstream>
#include <type_traits>
#include <typeindex>
#include <array>
#include <string>
#include <algorithm>
#include <stdexcept>
#include <tuple>
#include <assert.h>
#include <iostream>

#include <Player.hpp>

#include <misc/BufferHelper.hpp>
#include <misc/explints.hpp>
#include <misc/utils.hpp>

template<typename... Types>
struct are_all_arithmetic;

template<>
struct are_all_arithmetic<> : std::true_type {};

template<typename T, typename... Types>
struct are_all_arithmetic<T, Types...> : std::integral_constant<bool,
	std::is_arithmetic<T>::value &&
	are_all_arithmetic<Types...>::value> {};

// Helper to determine whether there's a const_iterator for T.
template<typename T>
struct has_const_iterator {
private:
    template<typename C> static char test(typename C::const_iterator*);
    template<typename C> static int  test(...);

public:
    enum { value = sizeof(test<T>(0)) == sizeof(char) };
};

//////////////////////////////
// Forward declarations
//////////////////////////////

template<typename>
struct is_tuple : std::false_type {};

template<typename... Ts>
struct is_tuple<std::tuple<Ts...>> : std::true_type {};

template<typename... Ts>
sz_t getSize(const std::tuple<Ts...>& t);

template<typename Container>
typename std::enable_if<has_const_iterator<Container>::value
	&& !has_const_iterator<typename Container::value_type>::value
	&& std::is_arithmetic<typename Container::value_type>::value,
	sz_t>::type
getSize(const Container& c);

template<typename Container>
typename std::enable_if<has_const_iterator<Container>::value
	&& !has_const_iterator<typename Container::value_type>::value
	&& is_tuple<typename Container::value_type>::value,
	sz_t>::type
getSize(const Container& c);

template<typename Container>
typename std::enable_if<has_const_iterator<Container>::value
	&& has_const_iterator<typename Container::value_type>::value,
	sz_t>::type
getSize(const Container& c);

sz_t getSize(const std::type_index& ti);

template<typename N>
typename std::enable_if<std::is_arithmetic<N>::value,
	sz_t>::type
writeToBuf(u8 ** b, const N& num, sz_t remaining);

template<typename... Ts>
sz_t writeToBuf(u8 ** b, const std::tuple<Ts...>& t, sz_t remaining);

template<typename Container>
typename std::enable_if<has_const_iterator<Container>::value,
	sz_t>::type
writeToBuf(u8 ** b, const Container& c, sz_t remaining);

sz_t writeToBuf(u8 ** b, const std::type_index& ti, sz_t remaining);

template<typename N>
typename std::enable_if<std::is_arithmetic<N>::value,
	N>::type
readFromBuf(u8 ** b, sz_t remaining);

template<typename Container>
typename std::enable_if<has_const_iterator<Container>::value
	&& !has_const_iterator<typename Container::value_type>::value,
	Container>::type
readFromBuf(u8 ** b, sz_t remaining);

template<typename Container>
typename std::enable_if<has_const_iterator<Container>::value
	&& has_const_iterator<typename Container::value_type>::value,
	Container>::type
readFromBuf(u8 ** b, sz_t remaining);

template<class Tuple>
typename std::enable_if<is_tuple<Tuple>::value,
	Tuple>::type
readFromBuf(u8 ** b, sz_t remaining);

template<class T>
typename std::enable_if<std::is_same<T, std::type_index>::value,
	std::type_index>::type
readFromBuf(u8 ** b, sz_t remaining);

//////////////////////////////

template<typename T, std::size_t N>
constexpr T add(const std::array<T, N>& arr) {
	T result = 0;
	for (sz_t i = 0; i < N; i++) {
		result += arr[i];
	}

	return result;
}

template<typename>
struct is_tuple_arithmetic : std::false_type {};

template<typename... Ts>
struct is_tuple_arithmetic<std::tuple<Ts...>> : are_all_arithmetic<Ts...> {
	// member could be removed if value is false?
	static constexpr sz_t size = add<sz_t, sizeof... (Ts)>({sizeof(Ts)...});
};

template<typename N>
typename std::enable_if<std::is_arithmetic<N>::value,
	sz_t>::type
getSize(const N&) {
	return sizeof(N);
}

template<class Tuple, std::size_t... Is>
sz_t getTupleBufferSize(const Tuple& t, std::index_sequence<Is...>) {
	return add<sz_t, sizeof... (Is)>({getSize(std::get<Is>(t))...});
}

template<typename... Ts>
sz_t getSize(const std::tuple<Ts...>& t) {
	if (are_all_arithmetic<Ts...>::value) {
		// hopefully computed at compile time
		return add<sz_t, sizeof... (Ts)>({sizeof(Ts)...});
	} else {
		return getTupleBufferSize(t, std::index_sequence_for<Ts...>{});
	}
}

template<typename Container>
typename std::enable_if<has_const_iterator<Container>::value
	&& !has_const_iterator<typename Container::value_type>::value // <- i don't think this is necessary
	&& std::is_arithmetic<typename Container::value_type>::value, // eg: no vector of strings
	sz_t>::type
getSize(const Container& c) {
	using T = typename Container::value_type;
	return c.size() * sizeof(T) + sizeof(u16);
}

template<typename Container>
typename std::enable_if<has_const_iterator<Container>::value
	&& !has_const_iterator<typename Container::value_type>::value
	&& is_tuple<typename Container::value_type>::value,
	sz_t>::type
getSize(const Container& c) {
	using T = typename Container::value_type;
	if (is_tuple_arithmetic<T>::value) {
		return sizeof(u16) + is_tuple_arithmetic<T>::size * c.size();
	}

	sz_t size = sizeof(u16);
	for (const T& v : c) {
		size += getSize(v);
	}

	return size;
}

template<typename Container>
typename std::enable_if<has_const_iterator<Container>::value
	&& has_const_iterator<typename Container::value_type>::value,
	sz_t>::type
getSize(const Container& c) {
	using T = typename Container::value_type;
	using Tv = typename T::value_type; // type of the inner array's content

	sz_t size = sizeof(u16);
	for (const T& v : c) {
		if (std::is_arithmetic<Tv>::value && (sizeof(Tv) == 1 || __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)) {
			size += sizeof(u16) + v.size() * sizeof(Tv);
		} else {
			size += getSize(v);
		}
	}

	return size;
}

sz_t getSize(const std::type_index& ti) {
	return getSize(demangle(ti));
}

template<typename N> // double pointer!
typename std::enable_if<std::is_arithmetic<N>::value,
	sz_t>::type
writeToBuf(u8 ** b, const N& num, sz_t remaining) {
	assert(remaining >= sizeof(N));
	*b += buf::writeLE(*b, num);
	return sizeof(N);
}

template<class Tuple, std::size_t... Is>
sz_t tupleToBuf(u8 ** b, const Tuple& t, sz_t remaining, std::index_sequence<Is...>) {
	u8 * start = *b;
	u8 useless[] {(writeToBuf(b, std::get<Is>(t), remaining - (*b - start)), u8(0))...};
	return *b - start;
}

template<typename... Ts>
sz_t writeToBuf(u8 ** b, const std::tuple<Ts...>& t, sz_t remaining) {
	return tupleToBuf(b, t, remaining, std::index_sequence_for<Ts...>{});
}

template<typename Container>
typename std::enable_if<has_const_iterator<Container>::value,
	sz_t>::type
writeToBuf(u8 ** b, const Container& c, sz_t remaining) {
	using T = typename Container::value_type;

	if (c.size() > u16(-1)) {
		throw std::runtime_error("array is too damn big");
	}

	sz_t byteSize = sizeof(u16);

	assert(remaining >= byteSize);
	*b += buf::writeLE<u16>(*b, c.size());

	if (std::is_arithmetic<T>::value && (sizeof(T) == 1 || __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)) {
		byteSize += c.size() * sizeof(T);
		assert(remaining >= byteSize);
		*b = reinterpret_cast<u8 *>(
			std::copy(c.cbegin(), c.cend(), reinterpret_cast<T *>(*b))
		);
	} else {
		for (const T& v : c) {
			byteSize += writeToBuf(b, v, remaining - byteSize);
		}
	}

	return byteSize;
}

sz_t writeToBuf(u8 ** b, const std::type_index& ti, sz_t remaining) {
	return writeToBuf(b, demangle(ti), remaining);
}

template<typename N>
typename std::enable_if<std::is_arithmetic<N>::value,
	N>::type
readFromBuf(u8 ** b, sz_t remaining) {
	if (remaining < sizeof(N)) {
		throw std::length_error("Buffer too small");
	}

	u8 * readAt = *b;
	*b += sizeof(N);
	return buf::readLE<N>(readAt);
}

template<typename Container>
typename std::enable_if<has_const_iterator<Container>::value
	&& !has_const_iterator<typename Container::value_type>::value,
	Container>::type // reads array of static-sized elements
readFromBuf(u8 ** b, sz_t remaining) {
	using T = typename Container::value_type;

	if (remaining < sizeof(u16)) {
		throw std::length_error("Buffer too small");
	}

	u8 * readAt = *b;
	u16 size = buf::readLE<u16>(readAt);

	if (remaining - sizeof(u16) < size * sizeof(T)) {
		throw std::length_error("Buffer corrupted");
	}

	*b += sizeof(u16) + size * sizeof(T);
	readAt += sizeof(u16);
	return Container(
		reinterpret_cast<T *>(readAt),
		reinterpret_cast<T *>(*b)
	); // guaranteed copy ellsion in c++17
}

template<typename Container>
typename std::enable_if<has_const_iterator<Container>::value
	&& has_const_iterator<typename Container::value_type>::value,
	Container>::type // reads array of arrays
readFromBuf(u8 ** b, sz_t remaining) {
	using T = typename Container::value_type;
	//using Tv = typename T::value_type;

	if (remaining < sizeof(u16)) {
		throw std::length_error("Buffer too small");
	}

	u16 size = buf::readLE<u16>(*b);
	*b += sizeof(u16);
	remaining -= sizeof(u16);

	Container c;
	c.reserve(size);
	while (size-- > 0) {
		u8 * prev = *b;
		c.emplace_back(readFromBuf<T>(b, remaining));
		remaining -= *b - prev;
	}

	return c; // NRVO pls
}

// idk if this is optimal
template<class Tuple, std::size_t... Is>
Tuple tupleFromBuf(u8 ** b, sz_t remaining, std::index_sequence<Is...>) {
	u8 * start = *b;
	return Tuple{readFromBuf<typename std::tuple_element<Is, Tuple>::type>(b, remaining - (*b - start))...};
}

template<class Tuple>
typename std::enable_if<is_tuple<Tuple>::value,
	Tuple>::type
readFromBuf(u8 ** b, sz_t remaining) {
	return tupleFromBuf<Tuple>(b, remaining, std::make_index_sequence<std::tuple_size<Tuple>::value>{});
}

template<class T>
typename std::enable_if<std::is_same<T, std::type_index>::value,
	std::type_index>::type
readFromBuf(u8 ** b, sz_t remaining) {
	return strToType(readFromBuf<std::string>(b, remaining));
}

template<u8 opCode, typename... Args>
Packet<opCode, Args...>::Packet(Args... args) {
	const sz_t size = isStatic()
		? staticBufSize()
		: sizeof(opCode) + add<sz_t, sizeof... (Args)>({getSize(args)...});

	u8 buffer[size]; // XXX: stack allocated, careful with big buffers
	u8 * start = &buffer[0];
	u8 * to = start;

	*to++ = opCode;

	if (isStatic()) {
		// should be faster, would be even better with c++17 fold expressions
		u8 useless[] {((to += buf::writeLE(to, args)), u8(0))...};
	} else {
		u8 useless[] {(writeToBuf(&to, args, size - (to - start)), u8(0))...};
		assert((to - start) == size);
	}

	std::ofstream("packet.bin", std::ios::binary | std::ios::trunc).write((const char*)start, size);
	/*uwsPreparedMessage = uWS::WebSocket<uWS::SERVER>::prepareMessage(
		reinterpret_cast<char *>(start), size, uWS::BINARY, false
	);*/
}

template<u8 opCode, typename... Args>
Packet<opCode, Args...>::~Packet() {
	/*using PrepMsg = uWS::WebSocket<uWS::SERVER>::PreparedMessage;
	uWS::WebSocket<uWS::SERVER>::finalizeMessage(
		static_cast<PrepMsg *>(uwsPreparedMessage)
	);*/
}

template<u8 opCode, typename... Args>
void * Packet<opCode, Args...>::getPrepared() {
	return uwsPreparedMessage;
}

template<u8 opCode, typename... Args>
constexpr bool Packet<opCode, Args...>::isStatic() {
	return are_all_arithmetic<Args...>::value;
}

template<u8 opCode, typename... Args>
constexpr sz_t Packet<opCode, Args...>::staticBufSize() {
	return sizeof(opCode) + add<sz_t, sizeof... (Args)>({sizeof(Args)...});
}

template<u8 opCode, typename... Args>
std::tuple<Args...> Packet<opCode, Args...>::fromBuffer(u8 * buffer, sz_t size) {
	u8 * start = buffer;

	// fast size check
	if (isStatic() && staticBufSize() - sizeof(opCode) != size) {
		throw std::length_error("Buffer too small/big");
	}

	return std::tuple<Args...>{readFromBuf<Args>(&buffer, size - (buffer - start))...};
}

template<u8 opCode, typename... Args>
void Packet<opCode, Args...>::one(uWS::WebSocket<uWS::SERVER> * ws, Args... args) {
	// i have to repeat the code if i don't want to copy the args
	// again to a separate function... i think
	const sz_t size = isStatic()
		? staticBufSize()
		: sizeof(opCode) + add<sz_t, sizeof... (Args)>({getSize(args)...});

	u8 buffer[size];
	u8 * start = &buffer[0];
	u8 * to = start;

	*to++ = opCode;

	if (isStatic()) {
		u8 useless[] {((to += buf::writeLE(to, args)), u8(0))...};
	} else {
		u8 useless[] {(writeToBuf(&to, args, size - (to - start)), u8(0))...};
		assert((to - start) == size);
	}

	//ws->send(reinterpret_cast<char *>(start), size, uWS::BINARY);
}

// Packet definitions
template class Packet<tc::AUTH_PROGRESS, std::type_index>;
template class Packet<tc::AUTH_OK, u64, std::string, bool>;
template class Packet<tc::AUTH_ERROR, std::type_index>;

template class Packet<tc::CMD_LIST, std::vector<std::tuple<std::string, std::vector<std::tuple<std::string, std::string, std::vector<std::tuple<std::string, std::type_index, bool>>>>>>>; // string = jsonstring
template class Packet<tc::CMD_RESULT, bool, std::string>;


//template class Packet<tc::SHOW_PLAYERS, std::vector<PlayerInfo>>;
template class Packet<tc::HIDE_PLAYERS, std::vector<Player::Id>>;
template class Packet<tc::CHAT_MESSAGE, std::string>;

//using TestP = Packet<tc::CHAT_MESSAGE, std::vector<std::vector<std::vector<std::string>>>>;
template class Packet<tc::CHAT_MESSAGE, std::vector<std::tuple<u8, std::string>>>;
//using Test2 = Packet<tc::CHAT_MESSAGE, bool, u32, std::string, u8, std::vector<i16>, std::vector<std::string>, std::tuple<u8, u8>>;
using asdf = Packet<tc::AUTH_OK, u64, std::string, bool>;
int main() {
	asdf(123456, "hola que tal", true);
	//Test2 p(true, 1234567, "hello", 50, {-40, 60}, {"abc", "def", "ghi"}, {30, 20});
}
