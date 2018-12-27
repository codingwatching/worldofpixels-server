#include <type_traits>
#include <stdexcept>

#include <misc/byteswap.hpp>
#include <misc/stringparser.hpp>
#include <misc/templateutils.hpp>
#include <misc/explints.hpp>

namespace detail {

template<typename T>
typename std::enable_if<is_optional<T>::value, T>::type
getValue(char * buf, sz_t size) {
	if (!buf) {
		return estd::nullopt;
	}

	return fromString<typename T::value_type>(std::string(buf, size));
}

template<typename T>
typename std::enable_if<!is_optional<T>::value, T>::type
getValue(char * buf, sz_t size) {
	if (!buf) {
		throw std::invalid_argument("Value was null on non-nullable result");
	}

	return fromString<T>(std::string(buf, size));
}

template<typename T>
typename std::enable_if<std::is_trivial<T>::value
		&& !std::is_null_pointer<T>::value, const char *>::type
getDataPointer(const T& value) {
	return reinterpret_cast<const char *>(&value);
}

template<typename T>
typename std::enable_if<std::is_same<T, std::string>::value, const char *>::type
getDataPointer(const T& value) {
	return value.c_str();
}

template<typename T>
typename std::enable_if<std::is_null_pointer<T>::value
		|| std::is_same<T, estd::nullopt_t>::value, const char *>::type
getDataPointer(const T&) {
	return nullptr;
}

template<typename T>
typename std::enable_if<is_optional<T>::value, const char *>::type
getDataPointer(const T& value) {
	return value ? getDataPointer(*value) : nullptr;
}

template<sz_t N>
const char * getDataPointer(const char(& arr)[N]) {
	return &arr[0];
}

template<typename T>
typename std::enable_if<std::is_trivial<T>::value, int>::type
getSize(const T&) {
	return sizeof(T);
}

template<typename T>
typename std::enable_if<std::is_same<T, std::string>::value, int>::type
getSize(const T& value) {
	return value.size();
}

template<typename T>
typename std::enable_if<std::is_same<T, estd::nullopt_t>::value, int>::type
getSize(const T&) {
	return 0;
}

template<typename T>
typename std::enable_if<is_optional<T>::value, int>::type
getSize(const T& value) {
	return value ? getSize(*value) : 0;
}

template<sz_t N>
int getSize(const char(& arr)[N]) {
	return N - 1; // trim null terminator
}

template<typename T>
typename std::enable_if<std::is_trivial<T>::value
		&& !std::is_array<T>::value
		&& !std::is_null_pointer<T>::value, void>::type
byteSwap(T& value) {
	switch (sizeof(T)) {
		case 8:
			value = bswap_64(value);
			break;

		case 4:
			value = bswap_32(value);
			break;

		case 2:
			value = bswap_16(value);
			break;
	}
}

template<typename T>
typename std::enable_if<is_optional<T>::value
		&& std::is_trivial<typename T::value_type>::value, void>::type
byteSwap(T& value) {
	if (value) {
		byteSwap(*value);
	}
}

template<typename T>
typename std::enable_if<!std::is_trivial<T>::value
		|| std::is_array<T>::value
		|| std::is_null_pointer<T>::value, void>::type
byteSwap(T&) { }

}

template<typename... Ts>
template<std::size_t... Is>
AsyncPostgres::TemplatedQuery<Ts...>::TemplatedQuery(std::index_sequence<Is...>, std::string cmd, Ts&&... params)
: Query(std::move(cmd), realValues, realLengths, realFormats, sizeof... (Ts)),
  valueStorage(std::forward<Ts>(params)...),
  realValues{detail::getDataPointer(std::get<Is>(valueStorage))...},
  realLengths{detail::getSize(std::get<Is>(valueStorage))...},
  realFormats{(Is, 1)...} { // set as many ones as there are type indexes
	(detail::byteSwap(std::get<Is>(valueStorage)), ...);
}

template<typename... Ts>
AsyncPostgres::TemplatedQuery<Ts...>::TemplatedQuery(std::string cmd, Ts&&... params)
: TemplatedQuery(std::index_sequence_for<Ts...>{}, std::move(cmd), std::forward<Ts>(params)...) { }

template<typename... Ts>
AsyncPostgres::Query& AsyncPostgres::query(std::string command, Ts&&... params) {
	if (!busy) {
		signalCompletion();
	}

	// dereference the iterator returned by emplace, and the unique_ptr
	return **queries.emplace(queries.end(), std::make_unique<TemplatedQuery<Ts...>>(std::move(command), std::forward<Ts>(params)...));
}

template<typename Func, typename Tuple>
void AsyncPostgres::Result::forEach(Func f) {
	for (Row r : *this) {
		multiApply(f, r.getImpl<Tuple>(std::make_index_sequence<std::tuple_size<Tuple>::value>{}));
	}
}

template<typename Tuple, std::size_t... Is>
Tuple AsyncPostgres::Result::Row::getImpl(std::index_sequence<Is...>) {
	return {detail::getValue<typename std::tuple_element<Is, Tuple>::type>(
		PQgetisnull(r, rowIndex, Is)
		? nullptr
		: PQgetvalue(r, rowIndex, Is),
		PQgetlength(r, rowIndex, Is)
	)...};
}

template<typename... Ts>
std::tuple<Ts...> AsyncPostgres::Result::Row::get() {
	return getImpl<std::tuple<Ts...>>(std::index_sequence_for<Ts...>{});
}
