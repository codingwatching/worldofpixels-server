//#include <utility>
#include <tuple>
#include <functional>

class Client;

namespace prdetail {

template<typename F, typename T, std::size_t... Is>
constexpr decltype(auto) sapplyImpl(F& f, Client& c, T&& t, std::index_sequence<Is...>) {
	return std::invoke(f, c, std::get<Is>(std::forward<T>(t))...);
}

template<typename F, typename T>
constexpr decltype(auto) sapply(F& f, Client& c, T&& t) {
	return sapplyImpl(f, c, std::forward<T>(t),
		std::make_index_sequence<std::tuple_size<std::remove_reference_t<T>>::value>{});
}

} // namespace prdetail

template<typename Packet, typename Func>
void PacketReader::on(Func f) {
	handlers.emplace(Packet::code, [f{std::move(f)}] (Client& c, const u8 * data, sz_t size) {
		prdetail::sapply(f, c, Packet::fromBuffer(data, size));
	});
}
