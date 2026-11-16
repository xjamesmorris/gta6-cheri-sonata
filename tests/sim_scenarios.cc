// SPDX-License-Identifier: MIT
#include "game.hh"
#include "game_renderer.hh"
#include "game_scenarios.hh"
#include "lcd_panel.hh"

#include <debug.hh>
#include <thread.h>

#if !defined(SIMULATION) || !defined(DRIVING_SIM_SCENE)
#	error "Scene fixtures are simulator-only"
#endif

void run_driving_scene(wallpaper_lcd::Panel &lcd)
{
	using Debug = ConditionalDebug<true, "wallpaper">;
	if (DRIVING_SIM_SCENE == 24 && !driving::run_protection_selftest(true))
	{
		Debug::log("FAIL: extended protection checks");
		return;
	}
	driving::Game game;
	if (DRIVING_SIM_SCENE == 1)
	{
		for (int i = 0; i < 2; ++i)
		{
			const auto frame = driving::render(game);
			lcd.draw_rgb565_scaled(frame.pixels,
			                       driving::PanelWidth,
			                       driving::PanelHeight,
			                       frame.scale);
			game.advance(driving::FadeMs / 2, 0);
		}
	}
	if (!driving::test::prepare_scene(game, DRIVING_SIM_SCENE))
	{
		Debug::log("FAIL: invalid driving scene {}", DRIVING_SIM_SCENE);
		return;
	}
	if (game.protection.failed())
	{
		Debug::log("FAIL: scene protection invariant");
		return;
	}
	if (DRIVING_SIM_SCENE == 24 &&
	    (game.protection.lastEvent != driving::ProtectedEvent::WalletStore ||
	     driving::classify_denial(
	       game.protection.lastStatus, game.protection.lastFault,
	       driving::StoreCause,
	       game.protection.lastBefore == game.protection.lastAfter &&
	         game.protection.lastAfter == game.money) != driving::ProtectionError::None))
	{
		Debug::log("FAIL: gameplay wallet denial evidence");
		return;
	}
	const auto frame = driving::render(game);
	lcd.draw_rgb565_scaled(
	  frame.pixels, driving::PanelWidth, driving::PanelHeight, frame.scale);
	if (DRIVING_SIM_SCENE == 1)
	{
		Debug::log("Wallpaper ready");
	}
	driving::log_protection_fault_totals();
	Debug::log("Scene ready {}", DRIVING_SIM_SCENE);
	// The model dumps during row writes, throttled by host wall time. Re-send
	// this frozen frame until the host observes a complete matching snapshot.
	while (true)
	{
		thread_millisecond_wait(10);
		lcd.draw_rgb565_scaled(
		  frame.pixels, driving::PanelWidth, driving::PanelHeight, frame.scale);
	}
}
