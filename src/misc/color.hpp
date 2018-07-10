#pragma once

#include <cstdint>

struct RGB {
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

double ColourDistance(RGB e1, RGB e2);
