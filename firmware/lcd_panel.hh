// Copyright lowRISC Contributors.
// Copyright (c) 2026 Grand Time Auto contributors.
// SPDX-License-Identifier: Apache-2.0
//
// Adapted from libraries/lcd.hh in lowRISC sonata-software v1.3, revision
// 722cdea0207920997b0895ea1f430619b343e10c:
// https://github.com/lowRISC/sonata-software/blob/722cdea0207920997b0895ea1f430619b343e10c/libraries/lcd.hh
//
// Changes made by the Grand Time Auto project in 2026: SonataLcd is reduced
// to a Panel with a fixed portrait orientation, no destructor and two RGB565
// streaming entry points. The Point, Rect, Color and Font types and the
// bitmap font includes are removed. See lcd_panel.cc and
// THIRD-PARTY-NOTICES.md.
#pragma once

#include <cheri.hh>
#include <stddef.h>
#include <stdint.h>

extern "C"
{
#include "core/lcd_base.h"
#include "st7735/lcd_st7735.h"
}

namespace wallpaper_lcd
{
	namespace internal
	{
		void __cheri_libcall panel_init(LCD_Interface *, St7735Context *);
	} // namespace internal

	struct Size
	{
		uint32_t width;
		uint32_t height;
	};

	class Panel
	{
		private:
		LCD_Interface lcdIntf;
		St7735Context ctx;

		public:
		/// Initialises the ST7735 in 128x160 portrait mode with the backlight
		/// held off until GRAM is cleared to pitch black, then turns the
		/// backlight on.
		Panel()
		{
			internal::panel_init(&lcdIntf, &ctx);
		}

		Size resolution() const
		{
			return {ctx.parent.width, ctx.parent.height};
		}

		/// Streams a width x height little-endian RGB565 image scaled by
		/// `scale` in [0, 256] (0 = black, 256 = exact 100% pixels) using
		/// full-row 20 MHz SPI bursts.
		void __cheri_libcall draw_rgb565_scaled(const uint8_t *rgb565,
		                                        uint32_t       width,
		                                        uint32_t       height,
		                                        uint32_t       scale);

		/// Re-sends a single pixel at (x, y) from a full-brightness
		/// little-endian RGB565 pixel pair (flushes Verilator's 1 Hz lcd.ppm).
		void __cheri_libcall touch_pixel_rgb565(uint32_t       x,
		                                        uint32_t       y,
		                                        const uint8_t *pixelRgb565);
	};
} // namespace wallpaper_lcd
