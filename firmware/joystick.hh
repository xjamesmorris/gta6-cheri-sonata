// SPDX-License-Identifier: MIT
#pragma once
#include "game.hh"

namespace driving
{
	constexpr uint8_t joystick_input(uint32_t bits)
	{
		return ((bits & (1u << 8)) ? Left : 0) |
		       ((bits & (1u << 12)) ? Right : 0) |
		       ((bits & (1u << 9)) ? Up : 0) |
		       ((bits & (1u << 11)) ? Down : 0) |
		       ((bits & (1u << 10)) ? Press : 0);
	}
} // namespace driving
