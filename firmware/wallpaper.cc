// SPDX-License-Identifier: MIT

#include "game.hh"
#include "game_renderer.hh"
#include "joystick.hh"
#include "lcd_panel.hh"

#include <compartment.h>
#include <debug.hh>
#include <optional>
#include <platform-gpio.hh>
#include <platform-rgbctrl.hh>
#include <riscvreg.h>
#include <thread.h>

using Debug = ConditionalDebug<true, "wallpaper">;

namespace
{
	static_assert(SonataGpioBoard::LEDCount == 8 &&
	              SonataGpioBoard::LEDMask == 0xff);

	void write_leds(uint8_t mask)
	{
		auto gpio = MMIO_CAPABILITY_WITH_PERMISSIONS(
		  SonataGpioBoard, gpio_board, true, true, false, false);
		gpio->output = mask & SonataGpioBoard::LEDMask;
	}

	bool write_rgb(const std::array<uint32_t, 2> &colors)
	{
		auto rgb = MMIO_CAPABILITY_WITH_PERMISSIONS(
		  SonataRgbLedController, rgbled, true, true, false, false);
		const uint64_t start = rdcycle64();
		while ((rgb->status & SonataRgbLedController::StatusIdle) == 0)
		{
			if (rdcycle64() - start >= CPU_TIMER_HZ / 100)
			{
				Debug::log("FAIL: RGB controller did not become idle");
				return false;
			}
		}
		// Physical RGB1 and RGB2 are SDK Led0 and Led1.
		for (size_t i = 0; i < colors.size(); ++i)
		{
			auto channel = [&](unsigned shift) {
				return static_cast<uint8_t>(
				  (((colors[i] >> shift) & 0xff) * 25 + 127) / 255);
			};
			rgb->rgb(i == 0 ? SonataRgbLed::Led0 : SonataRgbLed::Led1,
			         channel(16),
			         channel(8),
			         channel(0));
		}
		rgb->update();
		return true;
	}

#ifndef DRIVING_SIM_SCENE
	static_assert(CPU_TIMER_HZ % 1'000 == 0,
	              "CPU_TIMER_HZ must be an integer multiple of 1 kHz");
	constexpr uint64_t CyclesPerMs = CPU_TIMER_HZ / 1'000;
	static_assert(SonataGpioBoard::Left == (1u << 8) &&
	              SonataGpioBoard::Up == (1u << 9) &&
	              SonataGpioBoard::Pressed == (1u << 10) &&
	              SonataGpioBoard::Down == (1u << 11) &&
	              SonataGpioBoard::Right == (1u << 12));

	uint8_t read_input()
	{
		auto gpio = MMIO_CAPABILITY_WITH_PERMISSIONS(
		  SonataGpioBoard, gpio_board, true, true, false, false);
		return driving::joystick_input(gpio->debouncedInput);
	}
#endif

	std::optional<wallpaper_lcd::Panel> panel;
} // namespace

#ifdef DRIVING_SIM_SCENE
#	ifndef SIMULATION
#		error "Driving scene fixtures must never be built for hardware"
#	endif
void run_driving_scene(wallpaper_lcd::Panel &lcd);
#endif

void __cheri_compartment("wallpaper") wallpaper_run()
{
	Debug::log("Starting slot-2 Grand Time Auto");
	write_leds(0);

	// Initialises ST7735 in 128x160 portrait mode with backlight held off
	// until GRAM is cleared to black, then enables backlight (zero white
	// flash).
	panel.emplace();
	const bool          rgbReady = write_rgb({});
	wallpaper_lcd::Size res      = panel->resolution();
	if (res.width != driving::PanelWidth || res.height != driving::PanelHeight)
	{
		Debug::log("Unexpected LCD resolution {}x{} (expected {}x{})",
		           static_cast<int>(res.width),
		           static_cast<int>(res.height),
		           driving::PanelWidth,
		           driving::PanelHeight);
	}
	else if (rgbReady)
	{
		if (!driving::run_protection_selftest(false))
		{
			Debug::log("FAIL: hardware protection startup checks");
			driving::Game stopped;
			stopped.protection.fail(driving::ProtectionError::Infrastructure);
			const auto frame = driving::render(stopped);
			panel->draw_rgb565_scaled(frame.pixels, driving::PanelWidth,
			                          driving::PanelHeight, frame.scale);
			while (true)
			{
				thread_millisecond_wait(60'000);
			}
		}
#ifdef DRIVING_SIM_SCENE
		run_driving_scene(*panel);
#else
		driving::Game  game;
		game.reset();
		Debug::log("Protection backend: CHERIoT hardware");
		const uint64_t start           = rdcycle64();
		uint64_t       lastMs          = 0;
		uint32_t       lastRevision    = UINT32_MAX;
		uint32_t       lastScale       = UINT32_MAX;
		driving::Phase lastPhase       = driving::Phase::FadeIn;
		bool           overrunReported = false;
		uint8_t        lastLeds        = 0;
		std::array<uint32_t, 2> lastRgb{};
		bool protectionErrorReported = false;
		uint32_t lastDenials = 0;
		while (true)
		{
			const uint64_t nowMs   = (rdcycle64() - start) / CyclesPerMs;
			const uint64_t elapsed = nowMs - lastMs;
			lastMs                 = nowMs;
			if (elapsed > 200 && !overrunReported)
			{
				Debug::log("Game clock overrun; bounding catch-up");
				overrunReported = true;
			}
			game.advance(static_cast<uint32_t>(elapsed > 200 ? 200 : elapsed),
			             read_input());
			if (game.protection.failed() && !protectionErrorReported)
			{
				Debug::log("FAIL: protection error {} (detail {})",
				           static_cast<unsigned>(game.protection.error),
				           game.protection.errorDetail);
				protectionErrorReported = true;
				lastRevision = UINT32_MAX;
			}
			const uint32_t denials = game.protection.tagDenials +
			                         game.protection.roadDenials +
			                         game.protection.walletDenials;
			if (denials != lastDenials && denials != 0)
			{
				const auto fault = driving::decode_fault(game.protection.lastFault);
				Debug::log("CAP BLOCKED: event {}, mcause {}, mtval {}",
				           static_cast<unsigned>(game.protection.lastEvent),
				           fault.mcause, fault.mtval);
			}
			lastDenials = denials;
			const uint8_t leds = game.led_mask();
			if (leds != lastLeds)
			{
				write_leds(leds);
				lastLeds = leds;
			}
			const auto rgb = game.rgb_colors();
			if (rgb != lastRgb)
			{
				if (!write_rgb(rgb))
				{
					break;
				}
				lastRgb = rgb;
			}
			const uint32_t scale = game.brightness();
			if (game.revision != lastRevision || game.phase != lastPhase ||
			    scale != lastScale)
			{
				const driving::Frame frame = driving::render(game);
				panel->draw_rgb565_scaled(frame.pixels,
				                          driving::PanelWidth,
				                          driving::PanelHeight,
				                          frame.scale);
				if (game.phase == driving::Phase::Wallpaper &&
				    lastPhase != driving::Phase::Wallpaper)
				{
					Debug::log("Wallpaper ready");
				}
				if (game.phase != lastPhase)
				{
					Debug::log("Screen {}", static_cast<int>(game.phase));
				}
				lastRevision = game.revision;
				lastPhase    = game.phase;
				lastScale    = scale;
			}
			else if (game.phase == driving::Phase::FadeIn &&
			         !game.protection.failed())
			{
				thread_microsecond_spin(500);
			}
			else
			{
				thread_millisecond_wait(10);
			}
		}
#endif
	}

	while (true)
	{
		thread_millisecond_wait(60'000);
	}
}
