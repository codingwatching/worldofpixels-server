#pragma once

#include <string>
#include <vector>
#include <typeindex>

#include <misc/explints.hpp>

sz_t getUtf8StrLen(const std::string&);
bool makeDir(const std::string&);
bool fileExists(const std::string&);
i64 jsDateNow();
std::vector<std::string> tokenize(const std::string&, char delimiter = ' ', bool trimEmpty = false);
std::string randomStr(sz_t size);
void rtrim(std::string&);
void ltrim(std::string&);
void trim(std::string&);
const std::string& demangle(std::type_index);
std::type_index strToType(const std::string& s);

constexpr u8 popc(u32 n) {
	n = (n & 0x55555555) + ((n >> 1) & 0x55555555);
	n = (n & 0x33333333) + ((n >> 2) & 0x33333333);
	n = (n & 0x0F0F0F0F) + ((n >> 4) & 0x0F0F0F0F);
	n = (n & 0x00FF00FF) + ((n >> 8) & 0x00FF00FF);
	n = (n & 0x0000FFFF) + ((n >> 16)& 0x0000FFFF);
	return n;
}