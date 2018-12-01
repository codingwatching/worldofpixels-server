#pragma once

#include <string>

#include <misc/explints.hpp>

// Assumes no newlines or extra characters in encoded string
int base64Decode(const char * encoded, sz_t encLength, u8 * out, sz_t outMaxLen);
std::string base64Encode(const u8 *, sz_t);
