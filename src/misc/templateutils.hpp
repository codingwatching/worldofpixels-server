#pragma once

#include <type_traits>
#include <tuple>
#include <array>

#include <misc/optional.hpp>

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

template<typename>
struct is_std_array : std::false_type {};

template<typename T, std::size_t N>
struct is_std_array<std::array<T, N>> : std::true_type {};

template<typename>
struct is_tuple : std::false_type {};

template<typename... Ts>
struct is_tuple<std::tuple<Ts...>> : std::true_type {};

template<typename>
struct is_optional : std::false_type {};

template<typename T>
struct is_optional<estd::optional<T>> : std::true_type {};

template<typename... Args>
constexpr decltype(auto) add(Args&&... args) {
	return (args + ... + 0);
}

template<typename>
struct is_tuple_arithmetic : std::false_type {};

template<typename... Ts>
struct is_tuple_arithmetic<std::tuple<Ts...>> : are_all_arithmetic<Ts...> {
	// member could be removed if value is false?
	static constexpr std::size_t size = add(sizeof(Ts)...);
};
