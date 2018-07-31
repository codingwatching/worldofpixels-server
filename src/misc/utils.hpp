#pragma once

#include <string>
#include <vector>

#include <misc/explints.hpp>

sz_t getUTF8strlen(const std::string&);
bool makedir(const std::string&);
bool fileExists(const std::string&);
i64 js_date_now();
std::vector<std::string> tokenize(const std::string&, char delimiter = ' ', bool trimEmpty = false);