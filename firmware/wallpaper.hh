// SPDX-License-Identifier: MIT
#pragma once

#include <stdint.h>

#if defined(DRIVING_ARTWORK_PRIMARY) && defined(DRIVING_ARTWORK_ALTERNATE)
#	error "Select only one initial wallpaper artwork"
#endif

namespace driving::artwork
{
	namespace primary
	{
#include "wallpaper_image.h"
	}
	namespace alternate
	{
#include "wallpaper_image_alternate.h"
	}

	inline const uint8_t *pixels(bool useAlternate)
	{
		return useAlternate ? alternate::WallpaperRgb565 : primary::WallpaperRgb565;
	}
}
