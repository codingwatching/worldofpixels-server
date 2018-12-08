//#include <utility>
#include <tuple>
#include <functional>
#include <misc/tuple.hpp>

class Client;

template<typename Packet, typename Func>
void PacketReader::on(Func f) {
	handlers.emplace(Packet::code, [f{std::move(f)}] (Client& c, const u8 * data, sz_t size) {
		multiApply(f, Packet::fromBuffer(data, size), c);
	});
}
