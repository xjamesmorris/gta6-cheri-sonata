// Copyright lowRISC Contributors.
// Copyright (c) 2026 Grand Time Auto contributors.
// SPDX-License-Identifier: Apache-2.0
//
// Minimal ST7735 + PWM backlight driver for Sonata v1.3 that eliminates the
// upstream white boot flash (holding pwm_lcd off until GRAM is cleared to
// black) and streams full-row 20 MHz SPI bursts for smooth ~50 FPS fade-ins.
//
// Adapted from libraries/lcd.cc in lowRISC sonata-software v1.3, revision
// 722cdea0207920997b0895ea1f430619b343e10c:
// https://github.com/lowRISC/sonata-software/blob/722cdea0207920997b0895ea1f430619b343e10c/libraries/lcd.cc
//
// Changes made by the Grand Time Auto project in 2026: the SPI, backlight and
// chip-select helpers and the ST7735 bring-up order are kept. The backlight
// now stays off until controller RAM has been cleared to black. The upstream
// font, text, line, rectangle and image drawing API, its destructor and its
// orientation parameter are replaced by a fixed portrait panel with a
// brightness-scaled full-row RGB565 streaming path. See
// THIRD-PARTY-NOTICES.md.

#include "lcd_panel.hh"

#include <cheri.hh>
#include <debug.hh>
#include <platform-pwm.hh>
#include <platform-spi.hh>
#include <thread.h>

namespace
{
	using Debug  = ConditionalDebug<true, "lcd_panel">;
	using LcdPwm = SonataPulseWidthModulation::LcdBacklight;
	using LcdSpi = SonataSpi::Lcd;

	constexpr uint8_t  LcdCsPin        = 0;
	constexpr uint8_t  LcdDcPin        = 1;
	constexpr uint8_t  LcdRstPin       = 2;
	constexpr uint8_t  SpiOutEn        = 3;
	constexpr uint32_t MaxPanelColumns = 160;
	constexpr uint32_t RawMaxCols      = 132;
	constexpr uint32_t RawMaxRows      = 162;

	[[nodiscard, gnu::always_inline]] CHERI::Capability<volatile LcdSpi> spi()
	{
		return MMIO_CAPABILITY(LcdSpi, spi_lcd);
	}

	[[nodiscard, gnu::always_inline]] CHERI::Capability<volatile LcdPwm>
	pwm_bl()
	{
		return MMIO_CAPABILITY(LcdPwm, pwm_lcd);
	}

	void set_chip_select(uint8_t pin, bool high)
	{
		const uint32_t bit = (1u << pin);
		spi()->chipSelects =
		  high ? (spi()->chipSelects | bit) : (spi()->chipSelects & ~bit);
	}

	void clear_gram_black(St7735Context &ctx)
	{
		const size_t savedColOffset = ctx.col_offset;
		const size_t savedRowOffset = ctx.row_offset;
		const size_t savedWidth     = ctx.parent.width;
		const size_t savedHeight    = ctx.parent.height;

		// Clear the entire controller RAM (up to 132x162 on GM=000 panels) so
		// any sub-bezel margin rows left by GM offset detection are black too.
		const uint32_t clearW =
		  (savedColOffset > 0 || savedRowOffset > 0) ? RawMaxCols : savedWidth;
		const uint32_t clearH =
		  (savedColOffset > 0 || savedRowOffset > 0) ? RawMaxRows : savedHeight;

		ctx.col_offset    = 0;
		ctx.row_offset    = 0;
		ctx.parent.width  = clearW;
		ctx.parent.height = clearH;

		uint8_t zeroRow[RawMaxCols * 2] = {};
		lcd_st7735_rgb565_start(&ctx, {{0, 0}, clearW, clearH});
		set_chip_select(SpiOutEn, true);
		for (uint32_t y = 0; y < clearH; ++y)
		{
			spi()->blocking_write(zeroRow, static_cast<uint16_t>(clearW * 2));
		}
		spi()->wait_idle();
		lcd_st7735_rgb565_finish(&ctx);

		ctx.col_offset    = savedColOffset;
		ctx.row_offset    = savedRowOffset;
		ctx.parent.width  = savedWidth;
		ctx.parent.height = savedHeight;
	}
} // namespace

namespace wallpaper_lcd::internal
{
	void __cheri_libcall panel_init(LCD_Interface *lcdIntf, St7735Context *ctx)
	{
		// Keep the backlight completely off while resetting and initialising
		// the ST7735 so the panel never flashes white or shows GM test stripes.
		set_chip_select(LcdDcPin, false);
		pwm_bl()->output_set(/*period=*/0, /*duty_cycle=*/0);
		set_chip_select(LcdCsPin, false);

		spi()->init(false, false, true, 2);

		set_chip_select(LcdRstPin, false);
		thread_millisecond_wait(150);
		set_chip_select(LcdRstPin, true);

		lcdIntf->handle    = nullptr;
		lcdIntf->spi_write = [](void *, uint8_t *data, size_t len) -> uint32_t {
			set_chip_select(SpiOutEn, true);
			spi()->blocking_write(data, static_cast<uint16_t>(len));
			spi()->wait_idle();
			return static_cast<uint32_t>(len);
		};
		lcdIntf->spi_read = [](void *, uint8_t *data, size_t len) -> uint32_t {
			set_chip_select(SpiOutEn, false);
			spi()->blocking_read(data, static_cast<uint16_t>(len));
			spi()->wait_idle();
			return static_cast<uint32_t>(len);
		};
		lcdIntf->gpio_write = [](void *, bool csHigh, bool dcHigh) -> uint32_t {
			set_chip_select(LcdCsPin, csHigh);
			set_chip_select(LcdDcPin, dcHigh);
			return 0;
		};
		lcdIntf->reset             = nullptr;
		lcdIntf->set_backlight_pwm = nullptr;
		lcdIntf->timer_delay       = [](void *, uint32_t ms) {
			thread_millisecond_wait(ms);
		};

		lcd_st7735_init(ctx, lcdIntf);

		size_t w   = 0;
		size_t h   = 0;
		Result res = lcd_st7735_check_frame_buffer_resolution(ctx, &w, &h);
		if (res.code != 0)
		{
			Debug::log("Warning: Unable to determine LCD offset. Try slower "
			           "SPI clock.");
		}
		else if (w != 160)
		{
			lcd_st7735_set_frame_buffer_resolution(ctx, w, h);
		}

		// Switch SPI to full 20 MHz clock, run ST7735 init scripts, set
		// 128x160 portrait mode, clear GRAM to pitch black, then turn on the
		// backlight cleanly with black pixels already in GRAM.
		spi()->init(false, false, true, 0);
		lcd_st7735_startup(ctx);
		lcd_st7735_set_orientation(ctx, LCD_Rotate90);
		clear_gram_black(*ctx);
		pwm_bl()->output_set(/*period=*/1, /*duty_cycle=*/255);
	}
} // namespace wallpaper_lcd::internal

namespace wallpaper_lcd
{
	void __cheri_libcall Panel::draw_rgb565_scaled(const uint8_t *rgb565,
	                                               uint32_t       width,
	                                               uint32_t       height,
	                                               uint32_t       scale)
	{
		if (width == 0 || width > MaxPanelColumns || height == 0)
		{
			return;
		}
		if (scale > 256)
		{
			scale = 256;
		}

		uint8_t rowBuf[MaxPanelColumns * 2];
		lcd_st7735_rgb565_start(&ctx, {{0, 0}, width, height});
		set_chip_select(SpiOutEn, true);

		const uint8_t *src = rgb565;
		for (uint32_t y = 0; y < height; ++y)
		{
			for (uint32_t x = 0; x < width; ++x)
			{
				const uint16_t px =
				  static_cast<uint16_t>(src[0]) |
				  (static_cast<uint16_t>(src[1]) << 8);
				src += 2;

				const uint32_t r5 = ((px >> 11) & 0x1Fu) * scale >> 8;
				const uint32_t g6 = ((px >> 5) & 0x3Fu) * scale >> 8;
				const uint32_t b5 = (px & 0x1Fu) * scale >> 8;

				// ST7735 MADCTL has ST77_MADCTL_RGB (BGR order) set: send
				// big-endian 16-bit (b5 << 11) | (g6 << 5) | r5.
				const uint16_t wire =
				  static_cast<uint16_t>((b5 << 11) | (g6 << 5) | r5);
				rowBuf[2 * x]     = static_cast<uint8_t>(wire >> 8);
				rowBuf[2 * x + 1] = static_cast<uint8_t>(wire & 0xFFu);
			}
			spi()->blocking_write(rowBuf, static_cast<uint16_t>(width * 2));
		}
		spi()->wait_idle();
		lcd_st7735_rgb565_finish(&ctx);
	}

	void __cheri_libcall Panel::touch_pixel_rgb565(uint32_t       x,
	                                               uint32_t       y,
	                                               const uint8_t *pixelRgb565)
	{
		lcd_st7735_draw_rgb565(&ctx, {{x, y}, 1, 1}, pixelRgb565);
	}
} // namespace wallpaper_lcd
