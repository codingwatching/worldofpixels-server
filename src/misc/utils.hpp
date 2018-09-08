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