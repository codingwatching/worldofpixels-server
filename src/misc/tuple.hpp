#pragma once

#include <tuple>
#include <utility>

template<std::size_t O, std::size_t... Is>
constexpr std::index_sequence<(O + Is)...> applyOffsetToSequence(std::index_sequence<Is...>) {
	return {};
}

namespace detail {
	template<typename BaseTuple, std::size_t... Is>
	constexpr auto subTupleFromSequence(std::index_sequence<Is...>)
	-> std::tuple<std::tuple_element_t<Is, BaseTuple>...> {
		return {};
	}

	template<typename Tuple, std::size_t N, std::size_t S = std::tuple_size<Tuple>::value>
	constexpr auto sliceTuple() {
		return subTupleFromSequence<Tuple>(applyOffsetToSequence<N>(std::make_index_sequence<S - N>{}));
	}
}

template<typename Tuple, std::size_t N>
struct sliceTuple {
	using type = decltype(detail::sliceTuple<Tuple, N>());
};

template<typename F>
struct lambdaToTuple : public lambdaToTuple<decltype(&F::operator())> {};

template<typename ClassType, typename ReturnType, typename... Args>
struct lambdaToTuple<ReturnType(ClassType::*)(Args...) const> {
	using type = std::tuple<Args...>;
};

namespace detail {
	template<typename F, typename Tuple, typename... Args, std::size_t... Is>
	constexpr auto mApplyImpl(F&& f, Tuple&& t, std::index_sequence<Is...>, Args&&... preArgs) {
		return std::forward<F>(f)(std::forward<Args>(preArgs)..., std::get<Is>(std::forward<Tuple>(t))...);
	}
}

template<typename F, typename Tuple, typename... Args>
constexpr auto multiApply(F&& f, Tuple&& t, Args&&... preArgs) {
    return detail::mApplyImpl(
        std::forward<F>(f), std::forward<Tuple>(t),
        std::make_index_sequence<std::tuple_size<std::remove_reference_t<Tuple>>::value>{},
        std::forward<Args>(preArgs)...);
}
