#include "Ipv4.hpp"

#include <arpa/inet.h>
#include <stdexcept>

Ipv4::Ipv4(const char * ip)
: address(inet_addr(ip)) {
	if (address == -1) { // I mean, 255.255.255.255 is not going to be seen here anyways...
		throw std::invalid_argument("Invalid IP address! (" + std::string(ip) + ")");
	}
}

Ipv4::Ipv4(u32 address)
: address(address) { }

u32 Ipv4::get() const {
	return address;
}

std::string Ipv4::toString() const {
	return inet_ntoa({address});
}
