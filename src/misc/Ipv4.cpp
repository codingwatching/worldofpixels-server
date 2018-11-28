#include "Ipv4.hpp"

#include <arpa/inet.h>
#include <stdexcept>

#include <nlohmann/json.hpp>

Ipv4::Ipv4(const char * ip)
: address(inet_addr(ip)) {
	if (address == -1) { // I mean, 255.255.255.255 is not going to be seen here anyways...
		throw std::invalid_argument("Invalid IP address! (" + std::string(ip) + ")");
	}
}

Ipv4::Ipv4() // required by json's serializer
: address(0) { }

Ipv4::Ipv4(u32 address)
: address(address) { }

u32 Ipv4::get() const {
	return address;
}

std::string Ipv4::toString() const {
	return inet_ntoa({address});
}

bool Ipv4::operator ==(const Ipv4& b) const {
	return address == b.address;
}

bool Ipv4::operator  <(const Ipv4& b) const {
	return address < b.address;
}

void to_json(nlohmann::json& j, const Ipv4& ip) {
	j = ip.toString();
}

void from_json(const nlohmann::json& j, Ipv4& ip) {
	ip = Ipv4(j.get<std::string>().c_str());
}
