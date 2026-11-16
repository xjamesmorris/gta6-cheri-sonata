// SPDX-License-Identifier: MIT
#pragma once

#include "game.hh"

namespace driving
{
	struct Frame
	{
		const uint8_t *pixels;
		uint32_t       scale;
	};

	Frame render(const Game &game);
} // namespace driving
