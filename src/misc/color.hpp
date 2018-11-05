#pragma once

#include <cstdint>
#include <misc/explints.hpp>

union RGB_u {
	struct {
		u8 r;
		u8 g;
		u8 b;
		u8 a;
	};
	u32 rgb;
};

double ColourDistance(RGB_u e1, RGB_u e2);
