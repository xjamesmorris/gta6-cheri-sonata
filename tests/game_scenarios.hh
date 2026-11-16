// SPDX-License-Identifier: MIT
#pragma once
#include "game_renderer.hh"

namespace driving::test
{
	inline void drive_clean(Game &game, uint32_t milliseconds)
	{
		for (uint32_t elapsed = 0; elapsed < milliseconds; elapsed += TickMs)
		{
			for (auto &obstacle : game.obstacles)
			{
				obstacle.active = false;
			}
			const Bounds road = game.road_bounds(game.carY);
			game.carX         = (road.left + road.right - CarWidth) / 2;
			game.police       = {};
			game.advance(TickMs, 0);
		}
	}

	inline bool prepare_scene(Game &game, int scene)
	{
		game.reset(1);
		game.advance(FadeMs, 0);
		if (scene == 1)
		{
			return game.phase == Phase::Wallpaper;
		}
		game.advance(PromptDelayMs, 0);
		if (scene == 2)
		{
			return game.phase == Phase::Prompt;
		}
		if (scene == 27)
		{
			game.advance(0, Left);
			return game.phase == Phase::SourceQr;
		}
		game.advance(0, Right);
		if (scene == 24)
		{
			game.obstacles[0] = {static_cast<int16_t>(game.carX),
			                     CarY - 2,
			                     ObjectKind::MoneyBag,
			                     true};
			game.advance(TickMs, 0);
			if (game.money != MoneyBagValue)
			{
				return false;
			}
			game.carX = game.road_bounds(game.carY).left - 1;
			game.advance(TickMs, Left);
			if (game.protection.roadDenials != 1 ||
			    game.protection.tagDenials != 1 || game.damage != WallDamage)
			{
				return false;
			}
			game.money = 999'999;
			game.stars = 3;
			game.police[0].enabled = true;
			game.police[0].waitMs = 0;
			game.advance(TickMs, 0);
			const uint32_t distance = game.distance;
			game.advance(TickMs, 0);
			const Bounds road = game.road_bounds(game.carY);
			return game.phase == Phase::Playing && !game.protection.failed() &&
			       game.protection.walletDenials == 1 &&
			       game.protection.roadDenials == 1 &&
			       game.protection.tagDenials == 1 &&
			       game.money == MoneyBagValue && game.police[0].active &&
			       game.distance == distance + 2 &&
			       game.carX >= road.left && game.carX + CarWidth <= road.right;
		}
		if (scene == 23)
		{
			game.stars = 3;
			game.police[0] = {58, 55, false, PoliceReturnMs, true};
			game.police[0].explosionMs = PoliceExplosionMs - 160;
			return true;
		}
		if (scene == 18)
		{
			game.obstacles[0] = {33, 35, ObjectKind::Truck, true};
			game.obstacles[1] = {55, 55, ObjectKind::Bus, true};
			game.obstacles[2] = {79, 32, ObjectKind::Trailer, true};
			return true;
		}
		if (scene >= 19 && scene <= 22)
		{
			game.scenery = {
			  static_cast<int16_t>(scene == 21 ? Width - SceneryWidth : 0),
			  55,
			  static_cast<SceneryKind>(scene - 19),
			  true};
			return true;
		}
		if (scene == 3)
		{
			game.advance(TickMs * 60, 0);
			return game.phase == Phase::Playing;
		}
		if (scene == 9)
		{
			game.obstacles[0] = {
			  Width / 2 - 5, Height / 2, ObjectKind::MoneyBag, true};
			return game.phase == Phase::Playing && game.money == 0;
		}
		game.obstacles[0] = {static_cast<int16_t>(game.carX),
		                     CarY - 2,
		                     ObjectKind::MoneyBag,
		                     true};
		game.advance(TickMs, 0);
		const uint8_t targetStars = scene == 5 ? 3 : 5;
		for (unsigned hits = 0;
		     game.stars < targetStars && hits < 3u * targetStars;
		     ++hits)
		{
			game.graceMs      = 0;
			game.obstacles[0] = {static_cast<int16_t>(game.carX),
			                     static_cast<int16_t>(game.carY - 2),
			                     ObjectKind::Car,
			                     true};
			game.advance(TickMs, 0);
		}
		if (game.stars != targetStars || game.phase != Phase::Playing)
		{
			return false;
		}
		if (scene == 4)
		{
			return game.stars == 5 && game.stars_visible() &&
			       game.phase == Phase::Playing;
		}
		if (scene == 5)
		{
			drive_clean(game, EscapeFlashMs);
			return game.stars == 3 && !game.stars_visible() &&
			       game.phase == Phase::Playing;
		}
		if (scene == 6)
		{
			drive_clean(game, RecoveryMs);
			return game.stars == 0 && game.phase == Phase::Playing;
		}
		game.graceMs   = 0;
		game.police[0] = {game.carX, game.carY + 2, true, 0, true};
		game.advance(TickMs, 0);
		if (scene == 7)
		{
			return game.phase == Phase::Wasted;
		}
		if (scene == 25 || scene == 26)
		{
			game.advance(WastedHoldMs +
			               (scene == 25 ? WastedFadeMs / 2 : WastedFadeMs), 0);
			const auto colors = game.rgb_colors();
			return game.phase == Phase::Wasted && colors[0] == colors[1] &&
			       (scene == 25 ? (colors[0] >> 16) != 0 &&
			                              (colors[0] & 0xff) != 0 &&
			                              game.wasted_color_level() > 0 &&
			                              game.wasted_color_level() < 256
			                    : colors[0] == 0 && game.wasted_color_level() == 0);
		}
		if (scene >= 8 && scene <= 13)
		{
			game.advance(0, Press);
			game.advance(PostFadeMs, 0);
			const int64_t expectedMoney = MoneyBagValue - game.bill_total();
			if (scene == 11)
			{
				game.advance(BillFadeMs + BillLineMs * 2 + BillLineMs / 2, 0);
				return game.phase == Phase::Billing && game.billedItems == 2;
			}
			if (scene == 12 || scene == 13)
			{
				if (scene == 12)
				{
					game.advance(BillFadeMs / 2, 0);
				}
				else
				{
					game.advance(BillReadyMs, 0);
					game.advance(0, Right);
					game.advance(PostFadeMs / 2, Right);
				}
				return game.phase == Phase::Billing &&
				       game.brightness() == 64 &&
				       game.money ==
				         (scene == 12 ? MoneyBagValue : expectedMoney);
			}
			game.advance(BillReadyMs, 0);
			game.advance(0, scene == 8 ? Left : Right);
			game.advance(PostFadeMs, 0);
			if (scene == 8)
			{
				return game.phase == Phase::FadeIn && game.brightness() == 0 &&
				       game.money == 0;
			}
			game.advance(PostFadeMs, 0);
			return game.phase == Phase::Playing && game.money == expectedMoney;
		}
		if (scene == 14)
		{
			game.reset(1);
			game.advance(FadeMs + PromptDelayMs, 0);
			game.advance(0, Right);
			game.stars     = 3;
			game.police[0] = {game.carX + 15, game.carY + 30, true, 0, true};
			game.damage    = 60;
			return game.police_nearby() && game.rgb_colors()[0] == 0x0000ff;
		}
		if (scene == 15)
		{
			game.reset(1);
			game.advance(FadeMs + PromptDelayMs, 0);
			game.advance(0, Right);
			game.obstacles[0] = {
			  Width / 2 - 5, 50, ObjectKind::MoneyBag, true, JackpotValue};
			return true;
		}
		if (scene == 16 || scene == 17)
		{
			game.reset(1);
			game.advance(FadeMs + PromptDelayMs, 0);
			game.advance(0, Right);
			if (scene == 16)
			{
				game.obstacles[0] = {
				  Width / 2 - 7, 55, ObjectKind::GoldPile, true, GoldValue * 2};
				game.carHits = 1;
			}
			else
			{
				game.stars     = 5;
				game.police[0] = {30, 125, true, 0, true};
				game.police[1] = {76, 145, true, 0, true};
				game.police[2] = {58, 45, true, 0, true};
				game.carY      = MinCarY;
				game.damage    = 90;
			}
			return true;
		}
		return false;
	}

	inline uint16_t scaled_pixel(const Frame &frame, size_t offset)
	{
		const uint16_t color = static_cast<uint16_t>(
		  frame.pixels[offset] | (frame.pixels[offset + 1] << 8));
		const uint32_t red   = (((color >> 11) & 31) * frame.scale) >> 8;
		const uint32_t green = (((color >> 5) & 63) * frame.scale) >> 8;
		const uint32_t blue  = ((color & 31) * frame.scale) >> 8;
		return static_cast<uint16_t>((red << 11) | (green << 5) | blue);
	}
} // namespace driving::test
