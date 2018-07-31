#pragma once

#include <cstdint>
#include <misc/explints.hpp>

union RGB {
	struct {
		u8 r;
		u8 g;
		u8 b;
	};
	u32 rgb;
};

double ColourDistance(RGB e1, RGB e2);
