#include <type_traits>
#include <stdexcept>

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
