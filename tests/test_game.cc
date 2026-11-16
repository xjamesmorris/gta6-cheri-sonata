// SPDX-License-Identifier: MIT
#include "game.hh"
#include "game_renderer.hh"
#include "game_scenarios.hh"
#include "joystick.hh"
#include "wallpaper.hh"

#include <cassert>
#include <cstdio>
#include <cstring>

using namespace driving;

namespace
{
	Game playing(uint32_t seed = 1)
	{
		Game game;
		game.reset(seed);
		game.advance(FadeMs + PromptDelayMs, 0);
		game.advance(0, Right);
		assert(game.phase == Phase::Playing);
		return game;
	}

	void hit(Game &game, uint8_t input = 0, ObjectKind kind = ObjectKind::Car)
	{
		game.obstacles[0] = {static_cast<int16_t>(game.carX),
		                     static_cast<int16_t>(game.carY - 2),
		                     kind,
		                     true};
		game.advance(TickMs, input);
	}

	void safe_tick(Game &game)
	{
		for (auto &obstacle : game.obstacles)
		{
			obstacle.active = false;
		}
		const auto road = game.road_bounds(game.carY);
		game.carX       = (road.left + road.right - CarWidth) / 2;
		game.police     = {};
		game.advance(TickMs, 0);
	}

	void open_bill(Game &game)
	{
		game.advance(0, 0);
		game.advance(0, Press);
		game.advance(PostFadeMs, 0);
		assert(game.phase == Phase::Billing);
	}

	uint16_t at(const Frame &frame, int x, int y)
	{
		const size_t offset = (y * PanelWidth + x) * 2;
		return static_cast<uint16_t>(frame.pixels[offset] |
		                             (frame.pixels[offset + 1] << 8));
	}

	void expect_rgb(const Game &game, uint32_t first, uint32_t second)
	{
		const auto colors = game.rgb_colors();
		assert(colors[0] == first && colors[1] == second);
	}

	void screen_flow()
	{
		Game game;
		assert(render(game).scale == 0);
		game.advance(FadeMs / 2, Press);
		assert(game.brightness() == 64);
		game.advance(FadeMs / 2, Press);
		assert(game.phase == Phase::Wallpaper && game.brightness() == 256);
		assert(std::memcmp(render(game).pixels, artwork::pixels(game.alternateArtwork), FrameBytes) ==
		       0);
		game.advance(PromptDelayMs - 1, Press);
		assert(game.phase == Phase::Wallpaper);
		game.advance(1, Press);
		game.advance(100, Press);
		assert(game.phase == Phase::Prompt);
		game.advance(0, 0);
		game.advance(0, Up);
		assert(game.phase == Phase::Prompt);
		game.advance(0, 0);
		game.advance(0, Right);
		assert(game.phase == Phase::Playing);
		game.damage = 100;
		game.advance(TickMs, Press);
		assert(game.phase == Phase::Wasted);
		const uint32_t distance = game.distance;
		game.advance(RecoveryMs, Press);
		assert(game.phase == Phase::Wasted && game.distance == distance);
		game.advance(0, 0);
		game.advance(0, Press);
		assert(game.phase == Phase::Wasted);
		game.advance(PostFadeMs / 2, 0);
		assert(render(game).scale == 64);
		game.advance(PostFadeMs / 2, 0);
		assert(game.phase == Phase::Billing && render(game).scale == 0);
		const int64_t total = game.bill_total();
		game.advance(BillReadyMs, Press);
		game.advance(60'000, Press);
		assert(game.phase == Phase::Billing && game.money == -total);
		game.advance(0, 0);
		game.advance(0, Left);
		game.advance(PostFadeMs, Left);
		assert(game.phase == Phase::FadeIn && render(game).scale == 0);
		assert(game.stars == 0 && game.cleanMs == 0 && game.graceMs == 0);
		assert(game.money == 0);
	}

	void title_navigation()
	{
		Game game;
		game.advance(FadeMs + PromptDelayMs, 0);
		const bool initialArtwork = game.alternateArtwork;
		game.advance(0, Down);
		assert(game.phase == Phase::Prompt && game.alternateArtwork != initialArtwork);
		game.advance(1'000, Down);
		assert(game.phase == Phase::Prompt && game.alternateArtwork != initialArtwork);
		for (uint8_t input : {Left, Right, Up, Down, Press})
		{
			game.advance(0, 0);
			game.advance(0, Left);
			assert(game.phase == Phase::SourceQr);
			game.advance(1'000, Left);
			assert(game.phase == Phase::SourceQr);
			game.advance(0, 0);
			game.advance(0, input);
			assert(game.phase == Phase::Prompt && game.alternateArtwork != initialArtwork);
			game.advance(TickMs, input);
			assert(game.phase == Phase::Prompt);
		}
		game.advance(0, 0);
		game.advance(0, Right);
		assert(game.phase == Phase::Playing && game.alternateArtwork != initialArtwork);
		assert(game.protection.credit(500, game.money));
		const uint32_t distance = game.distance;
		game.change_artwork();
		assert(game.phase == Phase::Playing && game.money == 500 &&
		       game.distance == distance && game.alternateArtwork == initialArtwork);
		game.change_artwork();
		game.reset();
		assert(game.alternateArtwork != initialArtwork && game.money == 0);
		game.advance(FadeMs, 0);
		assert(std::memcmp(render(game).pixels, artwork::pixels(game.alternateArtwork), FrameBytes) == 0);
	}

	void collisions_and_recovery()
	{
		uint8_t targets = 0;
		for (uint32_t seed = 0; seed < 32; ++seed)
		{
			Game          game   = playing(seed);
			const uint8_t needed = game.hitsNeeded;
			assert(needed == 2 || needed == 3);
			targets |= static_cast<uint8_t>(1u << (needed - 2));
			for (uint8_t n = 1; n <= needed; ++n)
			{
				game.graceMs = 0;
				hit(game);
				assert(game.stars == (n == needed ? 1 : 0));
				assert(game.carHits == (n == needed ? 0 : n));
				assert(game.star_appearance(0) ==
				       (n == needed ? StarAppearance::Wanted
				                    : StarAppearance::Pending));
			}
		}
		assert(targets == 3);

		Game game = playing();
		hit(game);
		assert(game.carHits == 1 && game.stars == 0 &&
		       game.damage == CarDamage);
		assert(game.led_mask() == 0x80);
		game.progressMs = 250;
		assert(game.led_mask() == 0);
		hit(game);
		assert(game.carHits == 1 && game.damage == CarDamage);
		game.graceMs = 0;
		hit(game, 0, ObjectKind::Cone);
		assert(game.carHits == 1 && game.stars == 0);
		game.cleanMs = EscapeFlashMs + 500 - TickMs;
		safe_tick(game);
		assert(game.star_appearance(0) == StarAppearance::Expiring);
		game.cleanMs = RecoveryMs - TickMs;
		safe_tick(game);
		assert(game.stars == 0 && game.carHits == 0 && game.led_mask() == 0);

		game       = playing();
		game.stars = 5;
		hit(game);
		assert(game.phase == Phase::Playing && game.stars == 5 &&
		       game.damage == CarDamage);
	}

	void escaping_and_money()
	{
		for (uint8_t count : {1, 3, 5})
		{
			Game game  = playing();
			game.stars = count;
			for (uint32_t elapsed = 0; elapsed < EscapeFlashMs - TickMs;
			     elapsed += TickMs)
			{
				safe_tick(game);
				assert(game.stars == count && game.stars_visible());
			}
			safe_tick(game);
			assert(game.cleanMs == EscapeFlashMs && !game.stars_visible());
			while (game.cleanMs < EscapeFlashMs + 500)
			{
				safe_tick(game);
			}
			assert(game.stars_visible() && game.stars == count);
			while (game.cleanMs < RecoveryMs - TickMs)
			{
				safe_tick(game);
				assert(game.stars == count);
			}
			safe_tick(game);
			assert(game.stars == 0 && game.cleanMs == 0);
		}

		Game game         = playing();
		game.stars        = 2;
		game.cleanMs      = EscapeFlashMs;
		game.graceMs      = GraceMs;
		game.obstacles[0] = {static_cast<int16_t>(game.carX),
		                     CarY - 2,
		                     ObjectKind::MoneyBag,
		                     true};
		game.advance(TickMs, 0);
		assert(game.money == MoneyBagValue && game.stars == 2);
		assert(game.cleanMs == EscapeFlashMs + TickMs && !game.stars_visible());
		game.advance(TickMs, 0);
		assert(game.money == MoneyBagValue);
		hit(game);
		assert(game.stars == 2 && game.cleanMs == 0 && game.stars_visible());

		game.graceMs      = 0;
		game.obstacles[0] = {static_cast<int16_t>(game.carX),
		                     CarY - 2,
		                     ObjectKind::MoneyBag,
		                     true};
		game.obstacles[1] = {
		  static_cast<int16_t>(game.carX), CarY - 2, ObjectKind::Car, true};
		game.advance(TickMs, 0);
		assert(game.money == MoneyBagValue * 2 && game.stars == 2 &&
		       game.carHits == 1 && game.cleanMs == 0);
		game.damage = 100;
		game.advance(TickMs, 0);
		assert(game.phase == Phase::Wasted && game.money == MoneyBagValue * 2);
		game.advance(RecoveryMs, 0);
		assert(game.money == MoneyBagValue * 2);
		open_bill(game);
		const int64_t debt = MoneyBagValue * 2 - game.bill_total();
		assert(game.phase == Phase::Billing && game.money == MoneyBagValue * 2);
		game.advance(BillReadyMs, 0);
		game.advance(0, 0);
		game.advance(0, Right);
		game.advance(PostFadeMs, Right);
		assert(game.phase == Phase::Playing && game.money == debt && debt < 0);
		assert(game.damage == 0 && game.stars == 0 && game.brightness() == 0);
		game.advance(PostFadeMs, Right);
		assert(game.distance == 0 && game.brightness() == 256);
		game.advance(0, 0);
		game.obstacles[0] = {static_cast<int16_t>(game.carX),
		                     CarY - 2,
		                     ObjectKind::MoneyBag,
		                     true};
		game.advance(TickMs, 0);
		assert(game.money == debt + MoneyBagValue && game.stars == 0);
		game.reset();
		assert(game.money == 0);
	}

	void billing_sequence()
	{
		bool    varied        = false;
		int64_t previousTotal = 0;
		for (uint32_t seed = 1; seed <= 16; ++seed)
		{
			Game game   = playing(seed);
			game.damage = 100;
			game.advance(TickMs, 0);
			open_bill(game);
			assert(game.phase == Phase::Billing && game.money == 0);
			assert(game.bill[0].amount == HospitalBill);
			assert(game.bill[1].amount == 100 * RepairPerPoint);
			const int64_t total = game.bill_total();
			varied |= seed > 1 && total != previousTotal;
			previousTotal = total;
			for (size_t i = 0; i < BillItemCount; ++i)
			{
				assert(game.bill[i].label != nullptr &&
				       game.bill[i].amount > 0);
				for (size_t j = 0; j < i; ++j)
				{
					assert(
					  std::strcmp(game.bill[i].label, game.bill[j].label) != 0);
				}
			}
			game.advance(BillFadeMs / 2, Press);
			assert(render(game).scale == 64 && game.money == 0);
			game.advance(BillFadeMs / 2, Press);
			assert(render(game).scale == 256 && game.bill_shown_total() == 0);
			assert(at(render(game), 4, 7) == 0x7bef);
			int64_t settled = 0;
			for (size_t i = 0; i < BillItemCount; ++i)
			{
				game.advance(BillLineMs / 2, Press);
				assert(game.bill_item_shown(i) > 0 &&
				       game.bill_item_shown(i) < game.bill[i].amount);
				assert(game.money == -settled && game.billedItems == i);
				assert(game.bill_shown_total() > settled);
				game.advance(BillLineMs / 2 - 1, 0);
				assert(game.money == -settled);
				game.advance(1, Press);
				settled += game.bill[i].amount;
				assert(game.money == -settled && game.billedItems == i + 1);
				game.advance(0, Press);
				assert(game.money == -settled);
			}
			assert(settled == total);
			game.advance(60'000, Right);
			assert(game.phase == Phase::Billing && game.money == -total);
			game.advance(0, 0);
			game.advance(0, Left | Right);
			assert(game.phase == Phase::Billing && game.brightness() == 256);
			const uint8_t choice = seed % 2 == 0 ? Left : Right;
			game.advance(0, choice);
			game.advance(PostFadeMs / 2, choice);
			assert(render(game).scale == 64 && game.money == -total);
			game.advance(PostFadeMs / 2, choice);
			assert(game.phase ==
			       (choice == Left ? Phase::FadeIn : Phase::Playing));
			assert(game.money == (choice == Left ? 0 : -total));
			assert(render(game).scale == 0);
		}
		assert(varied);
	}

	void damage_loot_and_police()
	{
		Game game = playing();
		game.carX = game.road_bounds(CarY).left - 1;
		game.advance(TickMs, Left);
		assert(game.stars == 0 && game.damage == WallDamage);
		assert(game.carX >= game.road_bounds(CarY).left + 8);
		game.carX = game.road_bounds(CarY).left - 1;
		game.advance(TickMs, Left);
		assert(game.damage == WallDamage);
		for (int i = 1; i < 100; ++i)
		{
			for (auto &obstacle : game.obstacles)
				obstacle.active = false;
			game.wallGraceMs = 0;
			game.carX        = game.road_bounds(CarY).left - 1;
			game.advance(TickMs, Left);
			assert(game.stars == 0 && game.damage == (i + 1) * WallDamage);
		}
		assert(game.phase == Phase::Wasted);
		expect_rgb(game, 0xff0000, 0xff0000);
		game.advance(250, 0);
		expect_rgb(game, 0xff0000, 0xff0000);
		open_bill(game);
		assert(game.bill[1].amount == 100 * RepairPerPoint);
		expect_rgb(game, 0, 0);
		for (ObjectKind kind :
		     {ObjectKind::Cone, ObjectKind::Barrier, ObjectKind::Car,
		      ObjectKind::Truck, ObjectKind::Bus, ObjectKind::Trailer})
		{
			game = playing();
			hit(game, 0, kind);
			const bool vehicle =
			  kind != ObjectKind::Cone && kind != ObjectKind::Barrier;
			assert(game.damage == (kind == ObjectKind::Cone ? 2 : 5));
			assert(game.stars == 0 && game.carHits == (vehicle ? 1 : 0));
			expect_rgb(game, 0, 0xff8000);
		}

		for (int64_t value : {int64_t{500},
		                      int64_t{1'000},
		                      int64_t{2'500},
		                      int64_t{5'000},
		                      JackpotValue})
		{
			game              = playing();
			game.obstacles[0] = {static_cast<int16_t>(game.carX),
			                     CarY - 2,
			                     ObjectKind::MoneyBag,
			                     true,
			                     value};
			game.advance(TickMs, 0);
			assert(game.money == value && game.lastPickup == value);
			assert(game.stars == 0 && game.damage == 0 &&
			       !game.obstacles[0].active);
			expect_rgb(game, 0, 0x00ff00);
			game.advance(TickMs, 0);
			assert(game.money == value);
		}
		game              = playing();
		game.obstacles[0] = {static_cast<int16_t>(game.carX),
		                     static_cast<int16_t>(game.carY - 2),
		                     ObjectKind::GoldPile,
		                     true,
		                     GoldValue * 2};
		game.advance(TickMs, 0);
		assert(game.money == GoldValue * 2 && game.pickedGold &&
		       game.damage == 0 && game.stars == 0 && game.carHits == 0);

		for (uint8_t count = 0; count <= 5; ++count)
		{
			game       = playing(count);
			game.stars = count;
			game.advance(TickMs, 0);
			const size_t allowed = count < 3 ? 0 : count - 2;
			for (size_t i = 0; i < PoliceCount; ++i)
			{
				assert(game.police[i].enabled == (i < allowed));
			}
			if (allowed > 1)
			{
				const uint32_t otherWait = game.police[1].waitMs;
				game.police[0].waitMs    = 0;
				game.advance(TickMs, 0);
				assert(game.police[0].active && !game.police[1].active);
				assert(game.police[1].waitMs == otherWait - TickMs);
			}
			for (auto &cop : game.police)
				cop.waitMs = 0;
			game.advance(TickMs, 0);
			size_t active = 0;
			for (size_t i = 0; i < PoliceCount; ++i)
			{
				if (game.police[i].active)
				{
					++active;
					assert(i == 2 ? game.police[i].y < 0
					              : game.police[i].y >= Height);
				}
			}
			assert(active == allowed);
			if (allowed != 0)
			{
				assert(game.police_nearby());
				expect_rgb(game, 0x0000ff, 0);
			}
			if (allowed == 3)
			{
				const int topY  = game.police[2].y;
				const int rearY = game.police[0].y;
				game.advance(TickMs, 0);
				assert(game.police[2].y > topY && game.police[0].y < rearY);
			}
		}

		game           = playing();
		game.stars     = 4;
		game.carHits   = game.hitsNeeded - 1;
		game.police[0] = {game.carX, game.carY + 2, true, 0, true};
		game.advance(TickMs, 0);
		assert(game.phase == Phase::Playing && game.stars == 5 &&
		       game.damage == PoliceDamage);
		game.police[0] = {game.carX, game.carY + 2, true, 0, true};
		game.graceMs   = GraceMs;
		game.advance(TickMs, 0);
		assert(game.phase == Phase::Wasted && game.damage == 100);
		expect_rgb(game, 0xff0000, 0xff0000);

		game           = playing();
		game.stars     = 3;
		game.police[0] = {game.carX, game.carY - 60, true, 0, true};
		game.cleanMs   = RecoveryMs - TickMs;
		assert(game.police_nearby());
		expect_rgb(game, 0x0000ff, 0);
		game.advance(TickMs, 0);
		assert(game.stars == 0 && !game.police[0].active);
		expect_rgb(game, 0, 0);
		game.stars     = 4;
		game.police[0] = {game.carX, -CarHeight, true, 0, true};
		game.police[1] = {game.carX, Height, true, 0, true};
		game.advance(TickMs, 0);
		assert(!game.police[0].active && game.police[1].active);
		assert(game.police[0].waitMs >= PoliceReturnMs);
		const uint32_t wait = game.police[0].waitMs;
		game.advance(TickMs, 0);
		assert(!game.police[0].active && game.police[0].waitMs < wait);
	}

	void large_vehicle_collisions()
	{
		struct Vehicle
		{
			ObjectKind kind;
			int        height;
		};
		for (const Vehicle vehicle :
		     {Vehicle{ObjectKind::Truck, 24},
		      Vehicle{ObjectKind::Bus, 30},
		      Vehicle{ObjectKind::Trailer, 38}})
		{
			Game game = playing();
			game.obstacles[0] = {
			  static_cast<int16_t>(game.carX - 10),
			  static_cast<int16_t>(game.carY - vehicle.height + 3),
			  vehicle.kind,
			  true};
			game.advance(TickMs, 0);
			assert(game.phase == Phase::Playing && game.damage == CarDamage &&
			       game.carHits == 1 && !game.obstacles[0].active);
		}
	}

	void police_course_correction()
	{
		for (size_t i = 0; i < PoliceCount; ++i)
		{
			Game game  = playing();
			game.stars = 5;
			game.carY  = MinCarY;
			game.carX  = 75;
			auto &cop  = game.police[i];
			cop = {45, game.carY + (i == 2 ? -62 : 62), true, 0, true};
			game.advance(TickMs, 0);
			assert(cop.courseCorrected && cop.targetX == 75 && cop.x == 46);
			game.carX = 35;
			for (int tick = 0; tick < 5; ++tick)
			{
				game.advance(TickMs, 0);
				assert(cop.active && cop.targetX == 75 && cop.x == 47 + tick);
				assert(game.damage == 0);
			}
			cop.y = i == 2 ? Height - 1 : -CarHeight;
			game.advance(TickMs, 0);
			assert(!cop.active);
			cop.waitMs = 0;
			game.advance(TickMs, 0);
			assert(cop.active && !cop.courseCorrected &&
			       cop.targetX == cop.x);
		}
	}

	void gold_indicator()
	{
		Game game = playing();
		game.obstacles[0] = {static_cast<int16_t>(game.carX),
		                     static_cast<int16_t>(game.carY - 2),
		                     ObjectKind::GoldPile,
		                     true,
		                     GoldValue * 2};
		game.advance(TickMs, 0);
		expect_rgb(game, 0, 0xffad00);
		unsigned pulses = 0;
		bool     wasOn  = false;
		for (uint32_t elapsed = 0; elapsed < GoldFlashMs + 250;
		     elapsed += TickMs)
		{
			const bool on = game.rgb_colors()[1] == 0xffad00;
			pulses += on && !wasOn;
			wasOn = on;
			if (elapsed == 120)
			{
				game.obstacles[0] = {static_cast<int16_t>(game.carX),
				                     static_cast<int16_t>(game.carY - 2),
				                     ObjectKind::MoneyBag,
				                     true};
			}
			game.advance(TickMs, 0);
		}
		assert(pulses == 2 && game.goldFlashMs == 0);
		expect_rgb(game, 0, 0);
		assert(game.money == GoldValue * 2 + MoneyBagValue);

		game.activeMs    = 0;
		game.goldFlashMs = GoldFlashMs;
		game.stars       = 3;
		game.police[0]   = {game.carX + 20, game.carY + 30, true, 0, true};
		expect_rgb(game, 0x0000ff, 0xffad00);
		game.phase = Phase::Wasted;
		for (int i = 0; i < 8; ++i)
		{
			game.advance(250, 0);
			expect_rgb(game, 0xff0000, 0xff0000);
		}
		open_bill(game);
		expect_rgb(game, 0, 0);
		game.reset();
		assert(game.goldFlashMs == 0);
		expect_rgb(game, 0, 0);
	}

	void police_explosions()
	{
		auto collision = [](uint32_t seed, ObjectKind kind) {
			Game game  = playing(seed);
			game.stars = 3;
			game.police[0] = {35, 48, true, 0, true};
			game.obstacles[0] = {35, 48, kind, true};
			return game;
		};
		unsigned explosions = 0;
		uint32_t blastSeed  = UINT32_MAX;
		uint32_t safeSeed   = UINT32_MAX;
		for (uint32_t seed = 0; seed < 300; ++seed)
		{
			Game game = collision(seed, ObjectKind::Car);
			game.advance(TickMs, 0);
			assert(game.damage == 0 && game.money == 0 && game.stars == 3);
			const bool exploded = game.police[0].explosionMs != 0;
			assert(game.police[0].active != exploded);
			if (exploded)
			{
				++explosions;
				blastSeed = seed;
			}
			else
			{
				safeSeed = seed;
			}
		}
		assert(explosions > 75 && explosions < 125);
		assert(blastSeed != UINT32_MAX && safeSeed != UINT32_MAX);

		Game game = collision(safeSeed, ObjectKind::Car);
		game.advance(TickMs, 0);
		for (int tick = 0; tick < 10; ++tick)
		{
			game.obstacles[0].y =
			  static_cast<int16_t>(game.police[0].y + 2);
			game.advance(TickMs, 0);
			assert(game.police[0].active &&
			       game.police[0].explosionMs == 0 &&
			       game.police[0].inContact);
		}
		game.obstacles[0].active = false;
		game.advance(TickMs, 0);
		assert(!game.police[0].inContact);

		for (ObjectKind kind :
		     {ObjectKind::Cone, ObjectKind::Barrier, ObjectKind::Truck,
		      ObjectKind::Bus, ObjectKind::Trailer})
		{
			game = collision(blastSeed, kind);
			game.advance(TickMs, 0);
			assert(!game.police[0].active &&
			       game.police[0].explosionMs == PoliceExplosionMs);
			assert(game.damage == 0);
		}
		for (ObjectKind kind : {ObjectKind::MoneyBag, ObjectKind::GoldPile})
		{
			game = collision(blastSeed, kind);
			game.advance(TickMs, 0);
			assert(game.police[0].active &&
			       game.police[0].explosionMs == 0);
		}

		game = collision(blastSeed, ObjectKind::Car);
		game.obstacles[0].active = false;
		game.police[0].x = game.road_bounds(48).left;
		game.advance(TickMs, 0);
		assert(game.police[0].explosionMs == PoliceExplosionMs);

		game = collision(blastSeed, ObjectKind::Car);
		game.obstacles[0].active = false;
		game.stars = 4;
		game.police[1] = {35, 50, true, 0, true};
		game.advance(TickMs, 0);
		assert(game.police[0].explosionMs == PoliceExplosionMs &&
		       game.police[1].inContact);

		game = collision(blastSeed, ObjectKind::Car);
		game.obstacles[0].active = false;
		game.police[0] = {game.carX, game.carY + 2, true, 0, true};
		game.advance(TickMs, 0);
		assert(game.police[0].explosionMs == PoliceExplosionMs);
		assert(game.phase == Phase::Playing && game.damage == PoliceDamage);

		game = collision(blastSeed, ObjectKind::Car);
		game.advance(TickMs, 0);
		const int blastX = game.police[0].x + CarWidth / 2;
		const int blastY = game.police[0].y + CarHeight / 2;
		assert(at(render(game), blastX, blastY) == 0xffff);
		game.advance(440, 0);
		assert(at(render(game), blastX, blastY) == 0xbdf7);
		game.advance(200, 0);
		assert(game.police[0].explosionMs == 0 && !game.police[0].active);
		assert(game.damage == 0 && at(render(game), blastX, blastY) == 0x2965);
		game.police[0].waitMs = 0;
		game.advance(TickMs, 0);
		assert(game.police[0].active && !game.police[0].inContact);

		game = collision(blastSeed, ObjectKind::Car);
		game.advance(TickMs, 0);
		game.phase = Phase::Wasted;
		const uint32_t distance = game.distance;
		game.advance(PoliceExplosionMs, 0);
		assert(game.police[0].explosionMs == 0 && game.distance == distance);
	}

	void pickup_and_damage_indicators()
	{
		for (ObjectKind kind : {ObjectKind::MoneyBag, ObjectKind::GoldPile})
		{
			Game game = playing();
			game.obstacles[0] = {static_cast<int16_t>(game.carX),
			                     static_cast<int16_t>(game.carY - 2),
			                     kind,
			                     true,
			                     kind == ObjectKind::GoldPile ? GoldValue
			                                                  : MoneyBagValue};
			game.advance(TickMs, 0);
			expect_rgb(game, 0, 0x00ff00);
			game.advance(PickupFlashMs - TickMs, 0);
			expect_rgb(game, 0, 0x00ff00);
			game.advance(TickMs, 0);
			expect_rgb(game, 0, 0);
			assert(game.goldFlashMs == 0);
		}

		Game game = playing();
		hit(game);
		expect_rgb(game, 0, 0xff8000);
		game.advance(DamageFlashMs, 0);
		expect_rgb(game, 0, 0);
		hit(game);
		assert(game.damage == CarDamage);
		expect_rgb(game, 0, 0);

		game      = playing();
		game.carX = game.road_bounds(game.carY).left - 1;
		game.advance(TickMs, Left);
		expect_rgb(game, 0, 0xff8000);
		test::drive_clean(game, DamageFlashMs);
		expect_rgb(game, 0, 0);
		game.carX = game.road_bounds(game.carY).left - 1;
		game.advance(TickMs, Left);
		assert(game.damage == WallDamage);
		expect_rgb(game, 0, 0);

		game           = playing();
		game.stars     = 3;
		game.police[0] = {game.carX + 20, game.carY + 45, true, 0, true};
		game.obstacles[1] = {static_cast<int16_t>(game.carX),
		                     static_cast<int16_t>(game.carY - 2),
		                     ObjectKind::MoneyBag,
		                     true};
		hit(game, 0, ObjectKind::Cone);
		assert(game.money == MoneyBagValue && game.damage == ConeDamage);
		expect_rgb(game, 0x0000ff, 0xff8000);
	}

	void wasted_screen_and_led_fade()
	{
		auto chroma = [](const Frame &frame) {
			uint32_t sum = 0;
			for (int y = 0; y < Height; ++y)
			{
				for (int x = 0; x < Width; ++x)
				{
					const uint16_t color = at(frame, x, y);
					const uint32_t red   = (color >> 11) & 31;
					const uint32_t green = (color >> 6) & 31;
					const uint32_t blue  = color & 31;
					sum += red > green ? red - green : green - red;
					sum += green > blue ? green - blue : blue - green;
					sum += blue > red ? blue - red : red - blue;
				}
			}
			return sum;
		};
		Game game = playing();
		game.goldFlashMs   = GoldFlashMs;
		game.pickupFlashMs = PickupFlashMs;
		game.damage       = 100;
		game.advance(TickMs, 0);
		expect_rgb(game, 0xff0000, 0xff0000);
		const uint32_t colored = chroma(render(game));
		assert(colored > 0);
		const uint32_t holdRevision = game.revision;
		game.advance(WastedHoldMs, 0);
		expect_rgb(game, 0xff0000, 0xff0000);
		assert(game.revision == holdRevision && chroma(render(game)) == colored);
		uint32_t previous = 255;
		bool     purpleSeen = false;
		for (uint32_t elapsed = 0; elapsed < WastedFadeMs; elapsed += 100)
		{
			const uint32_t previousLevel = game.wasted_color_level();
			const uint32_t previousRevision = game.revision;
			game.advance(100, 0);
			const auto colors = game.rgb_colors();
			const uint32_t red = colors[0] >> 16;
			const uint32_t blue = colors[0] & 0xff;
			const uint32_t peak = red > blue ? red : blue;
			assert(colors[0] == colors[1] && (colors[0] & 0xff00) == 0);
			assert(peak <= previous);
			purpleSeen |= red > 0 && blue >= red;
			previous = peak;
			if (game.wasted_color_level() != previousLevel)
			{
				assert(game.revision > previousRevision);
			}
			if (elapsed + 100 == 3'000)
			{
				const uint32_t halfway = chroma(render(game));
				assert(halfway > 0 && halfway < colored && red > 0 && blue > 0);
			}
			if (elapsed + 100 == 4'000)
			{
				assert(colors[0] != 0 && chroma(render(game)) > 0);
			}
		}
		assert(purpleSeen && game.phase == Phase::Wasted);
		expect_rgb(game, 0, 0);
		const auto gray = render(game);
		assert(chroma(gray) == 0 && gray.scale == 256);
		assert(at(gray, (Width - 70) / 2, (Height - 14) / 2) != 0);
		const uint32_t finalRevision = game.revision;
		game.advance(UINT32_MAX - (WastedHoldMs + WastedFadeMs) + 1, 0);
		expect_rgb(game, 0, 0);
		assert(game.revision == finalRevision && chroma(render(game)) == 0);
		open_bill(game);
		expect_rgb(game, 0, 0);
		game.advance(BillFadeMs, 0);
		assert(chroma(render(game)) > 0);
	}

	void decorative_scenery()
	{
		for (SceneryKind kind : {SceneryKind::ZebraCrossing,
		                         SceneryKind::Billboard,
		                         SceneryKind::PoliceStation,
		                         SceneryKind::GasStation})
		{
			Game game    = playing();
			game.stars   = 2;
			game.cleanMs = 500;
			game.scenery = {static_cast<int16_t>(game.carX),
			                static_cast<int16_t>(game.carY - 2),
			                kind,
			                true};
			game.advance(TickMs, 0);
			assert(game.damage == 0 && game.carHits == 0 && game.stars == 2 &&
			       game.money == 0 && game.cleanMs == 500 + TickMs);
			assert(game.scenery.y == game.carY);
			game.scenery.y = Height - 1;
			game.advance(TickMs, 0);
			assert(!game.scenery.active);
			game.reset();
			assert(!game.scenery.active);
		}
	}

	void world_and_steering()
	{
		assert(joystick_input((1u << 8) | (1u << 10)) == (Left | Press));
		assert(joystick_input((1u << 12) | (1u << 9)) == (Right | Up));
		assert(joystick_input((1u << 11) | (1u << 16) | 7u) == Down);
		Game      game = playing();
		const int x    = game.carX;
		game.advance(TickMs - 1, Left);
		assert(game.carX == x);
		game.advance(1, Right);
		assert(game.carX == x + 4);
		game.advance(TickMs, Left);
		game.advance(TickMs, Left | Right);
		assert(game.carX == x);
		game.advance(TickMs, Left);
		assert(game.carX < x);
		game.advance(TickMs, Right);
		assert(game.carX == x);
		game.obstacles[0] = {70, Height - 1, ObjectKind::Cone, true};
		game.advance(TickMs, 0);
		assert(!game.obstacles[0].active);
		const int startY = game.carY;
		game.advance(TickMs, Up);
		assert(game.carY < startY);
		game.advance(TickMs, Down);
		assert(game.carY == startY);
		game.advance(TickMs, Up | Down);
		assert(game.carY == startY);
		for (int i = 0; i < 40; ++i)
			game.advance(TickMs, Up);
		assert(game.carY == MinCarY);
		game.graceMs               = 0;
		const uint8_t previousHits = game.carHits;
		hit(game);
		assert(game.carHits != previousHits || game.stars != 0);
		for (int i = 0; i < 40; ++i)
			game.advance(TickMs, Down);
		assert(game.carY == MaxCarY);

		bool     jackpot      = false;
		bool     gold         = false;
		uint8_t  lootValues   = 0;
		uint8_t  vehicleKinds = 0;
		uint8_t  sceneryKinds = 0;
		unsigned objects      = 0;
		unsigned vehicles     = 0;
		for (uint32_t seed = 0; seed < 64; ++seed)
		{
			game = playing(seed);
			uint32_t lastScenery = 0;
			for (int tick = 0; tick < 1000; ++tick)
			{
				// Stay invulnerable to inspect generation independently of
				// death.
				game.graceMs      = GraceMs;
				const auto road   = game.road_bounds(CarY);
				game.carX         = (road.left + road.right - CarWidth) / 2;
				const auto oldTop = game.road_bounds(0);
				game.advance(TickMs, 0);
				const auto moved = game.road_bounds(2);
				assert(oldTop.left == moved.left &&
				       oldTop.right == moved.right);
				for (int y = 0; y < Height; ++y)
				{
					const auto bounds = game.road_bounds(y);
					assert(bounds.left >= RoadMargin &&
					       bounds.right <= Width - RoadMargin);
					assert(bounds.right - bounds.left == RoadWidth);
				}
				for (const auto &obstacle : game.obstacles)
				{
					if (obstacle.active)
					{
						if (obstacle.y == -obstacle.height())
						{
							++objects;
							vehicles += obstacle.is_vehicle();
							if (obstacle.is_vehicle())
							{
								vehicleKinds |= static_cast<uint8_t>(
								  1u << static_cast<unsigned>(obstacle.kind));
							}
						}
						const auto bounds =
						  game.road_bounds(obstacle.y + obstacle.height() / 2);
						assert(obstacle.x >= bounds.left);
						assert(obstacle.x + obstacle.width() <= bounds.right);
						if (obstacle.kind == ObjectKind::MoneyBag)
						{
							jackpot |= obstacle.value == JackpotValue;
							if (obstacle.value == 500)
								lootValues |= 1;
							else if (obstacle.value == 1'000)
								lootValues |= 2;
							else if (obstacle.value == 2'500)
								lootValues |= 4;
							else if (obstacle.value == 5'000)
								lootValues |= 8;
							else
								assert(obstacle.value == JackpotValue);
						}
						else if (obstacle.kind == ObjectKind::GoldPile)
						{
							gold = true;
							assert(obstacle.value == GoldValue ||
							       obstacle.value == GoldValue * 2);
						}
					}
				}
				if (game.scenery.active)
				{
					if (game.scenery.y == -SceneryHeight)
					{
						if (lastScenery != 0)
						{
							assert(game.distance - lastScenery >= 1'000 &&
							       game.distance - lastScenery <= 2'002);
						}
						else
						{
							assert(game.distance >= 800 &&
							       game.distance <= 1'600);
						}
						lastScenery = game.distance;
						sceneryKinds |= static_cast<uint8_t>(
						  1u << static_cast<unsigned>(game.scenery.kind));
					}
					if (game.scenery.kind != SceneryKind::ZebraCrossing)
					{
						for (int row = 0; row < SceneryHeight; ++row)
						{
							const auto bounds =
							  game.road_bounds(game.scenery.y + row);
							assert(game.scenery.x + SceneryWidth <=
							         bounds.left - 2 ||
							       game.scenery.x >= bounds.right + 2);
						}
					}
				}
			}
		}
		assert(jackpot && gold && lootValues == 15);
		assert(objects >= 64 * 30 && vehicles * 2 > objects);
		assert(vehicleKinds ==
		       ((1u << static_cast<unsigned>(ObjectKind::Car)) |
		        (1u << static_cast<unsigned>(ObjectKind::Truck)) |
		        (1u << static_cast<unsigned>(ObjectKind::Bus)) |
		        (1u << static_cast<unsigned>(ObjectKind::Trailer))));
		assert(sceneryKinds == 15);
	}

	void descending_leds()
	{
		Game              game       = playing();
		constexpr uint8_t Expected[] = {0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8};
		for (uint8_t count = 0; count <= 5; ++count)
		{
			game.stars   = count;
			game.cleanMs = 0;
			assert(game.led_mask() == Expected[count]);
			game.cleanMs = EscapeFlashMs;
			assert(game.led_mask() == 0);
			game.cleanMs = EscapeFlashMs + 500;
			assert(game.led_mask() == Expected[count]);
		}
		game.phase   = Phase::Wasted;
		game.stars   = 5;
		game.cleanMs = EscapeFlashMs;
		assert(game.led_mask() == 0xf8);
		for (Phase phase :
		     {Phase::FadeIn, Phase::Wallpaper, Phase::Prompt, Phase::Billing})
		{
			game.phase = phase;
			assert(game.led_mask() == 0);
		}
	}

	void scene_fixtures()
	{
		for (int scene = 1; scene <= 27; ++scene)
		{
			Game game;
			assert(test::prepare_scene(game, scene));
			render(game);
		}
	}

	void protected_gameplay()
	{
		assert(Protection::origin() == ProtectionOrigin::Emulated);
		Game game = playing();
		assert(game.protection.credit(100'000, game.money));
		Game copy = game;
		assert(copy.protection.credit(500, copy.money));
		int64_t actual = 0;
		assert(game.protection.read_wallet(actual) && actual == 100'000);

		game.money = 7;
		game.damage = 100;
		game.advance(TickMs, 0);
		open_bill(game);
		assert(game.billOpeningMoney == 100'000 && game.money == 100'000);
		game.advance(BillReadyMs, 0);
		const int64_t balance = game.money;
		game.money = 42;
		game.advance(0, Right);
		game.advance(PostFadeMs, 0);
		assert(game.phase == Phase::Playing && game.money == balance);
		assert(game.protection.read_wallet(actual) && actual == balance);
		game.reset();
		assert(game.money == 0 && game.protection.read_wallet(actual) && actual == 0);

		game = playing();
		game.carX = game.road_bounds(game.carY).left - 1;
		game.advance(TickMs, Left);
		assert(game.protection.roadDenials == 1 &&
		       game.protection.tagDenials == 1 && game.damage == WallDamage);
		const auto frame = render(game);
		size_t denialPixels = 0;
		for (int y = Height - 8; y < Height; ++y)
		{
			for (int x = 0; x < Width; ++x)
			{
				denialPixels += at(frame, x, y) == 0x07e0;
			}
		}
		assert(denialPixels > 0);
		game.advance(TickMs * 3, Left);
		assert(game.protection.roadDenials == 2 &&
		       game.protection.tagDenials == 1 && !game.protection.failed());

		game = playing();
		assert(game.protection.credit(500, game.money));
		game.money = 999'999;
		game.stars = 3;
		game.police[0].enabled = true;
		game.police[0].waitMs = 0;
		game.advance(TickMs, 0);
		assert(game.protection.walletDenials == 1 && game.money == 500 &&
		       !game.protection.failed());

		for (size_t width = 1; width <= RoadWidth; ++width)
		{
			Protection probe;
			assert(probe.check_road(0, static_cast<int>(width - 1), width) ==
			       RoadAccess::Allowed);
			assert(probe.check_road(static_cast<int>(width), -1, width) ==
			       RoadAccess::Blocked);
			assert(probe.check_road(-1, static_cast<int>(width), width) ==
			       RoadAccess::Blocked);
			assert(probe.roadDenials == 2 && !probe.failed());
		}
	}

	void protection_failure_checks()
	{
		constexpr uint64_t tag =
		  (uint64_t{1} << 63) | (uint64_t{1} << 48) |
		  (uint64_t{CheriException} << 32) | (10u << 5) | TagCause;
		assert(classify_denial(-1, tag, TagCause, true) == ProtectionError::None);
		assert(classify_denial(0, tag, TagCause, true) ==
		       ProtectionError::UnexpectedSuccess);
		for (int status : {-140, -141, 1})
		{
			assert(classify_denial(status, tag, TagCause, true) ==
			       ProtectionError::Infrastructure);
		}
		for (uint64_t missing : {uint64_t{0}, uint64_t{0xffffffff}})
		{
			assert(classify_denial(-1, missing, TagCause, true) ==
			       ProtectionError::Infrastructure);
		}
		assert(classify_denial(-1, tag, BoundsCause, true) ==
		       ProtectionError::WrongFault);
		assert(classify_denial(-1, tag, TagCause, false) ==
		       ProtectionError::DataChanged);
		assert(classify_denial(-1, tag + (uint64_t{1} << 48), TagCause, true) ==
		       ProtectionError::Infrastructure);
		int64_t next = 17;
		assert(changed_balance(INT64_MAX, 1, false, next) == MoneyResult::Overflow &&
		       next == 17);
		assert(changed_balance(MinimumBalance, 1, true, next) == MoneyResult::Overflow &&
		       next == 17);
		assert(changed_balance(0, -1, false, next) == MoneyResult::Invalid);

		Game game = playing();
		assert(!game.protection.debit(-1, game.money));
		const uint32_t distance = game.distance;
		game.advance(10'000, Right);
		assert(game.protection.failed() && game.distance == distance);
		expect_rgb(game, 0xff0000, 0xff0000);
		assert(render(game).scale == 256);

		game = playing();
		assert(game.protection.credit(INT64_MAX, game.money));
		game.obstacles[0] = {static_cast<int16_t>(game.carX),
		                     static_cast<int16_t>(game.carY - 2),
		                     ObjectKind::MoneyBag,
		                     true,
		                     1};
		game.advance(TickMs * 5, 0);
		assert(game.protection.failed() && game.money == INT64_MAX &&
		       game.distance == 2);
		game.advance(TickMs, 0);
		assert(game.distance == 2);

		game = playing();
		assert(game.protection.debit(INT64_MAX, game.money));
		game.damage = 100;
		game.advance(TickMs, 0);
		open_bill(game);
		game.advance(BillReadyMs, 0);
		assert(game.protection.failed() && game.billedItems == 1 &&
		       game.money == -INT64_MAX);
		game.advance(BillReadyMs, 0);
		assert(game.billedItems == 1 && game.money == -INT64_MAX);
	}

	void rendering()
	{
		Game game;
		game.advance(FadeMs + PromptDelayMs, 0);
		const Frame prompt  = render(game);
		size_t      changed = 0;
		for (int x = 0; x < Width; ++x)
		{
			for (int y = 0; y < Height; ++y)
			{
				const size_t offset = (y * PanelWidth + x) * 2;
				if (std::memcmp(
				      prompt.pixels + offset, artwork::pixels(game.alternateArtwork) + offset, 2))
				{
					assert(x >= 4 && x < Width - 3 && y >= Height - 15 &&
					       y <= Height - 2);
					++changed;
				}
			}
		}
		assert(changed > 100);
		game       = playing();
		game.stars = 5;
		assert(at(render(game), Width - 47, 5) == 0xf800);
		game.cleanMs = EscapeFlashMs;
		assert(at(render(game), Width - 47, 5) == 0x5aeb);
		game.cleanMs = EscapeFlashMs + 500;
		assert(at(render(game), Width - 47, 5) == 0x07e0);
		game.phase = Phase::Wasted;
		assert(at(render(game), (Width - 70) / 2, (Height - 14) / 2) == 0xf800);
		assert(at(render(game), Width - 47, 5) == 0xf800);
		game              = playing();
		game.obstacles[0] = {-5, -8, ObjectKind::Car, true};
		render(game); // Sanitizers check clipping at both axes.
		game.money = -24'000;
		assert(at(render(game), 4, 7) == 0xf800);
		game.money = 0;
		assert(at(render(game), 5, 5) == 0x07e0);
		game.money = INT64_MIN;
		render(game);
		game.money = INT64_MAX;
		render(game);
		game.obstacles[0] = {60, 60, ObjectKind::MoneyBag, true};
		assert(at(render(game), 64, 63) == 0x07a0);
		for (ObjectKind kind :
		     {ObjectKind::Truck, ObjectKind::Bus, ObjectKind::Trailer})
		{
			game = playing();
			game.obstacles[0] = {50, 45, kind, true};
			const int tail = 45 + game.obstacles[0].height() - 4;
			assert(at(render(game), 54, tail) != 0x2965);
			game.obstacles[0].active = false;
			assert(at(render(game), 54, tail) == 0x2965);
			game.obstacles[0] = {-5, -8, kind, true};
			render(game);
			game.obstacles[0] = {Width - 4, Height - 5, kind, true};
			render(game);
		}
		for (SceneryKind kind : {SceneryKind::ZebraCrossing,
		                         SceneryKind::Billboard,
		                         SceneryKind::PoliceStation,
		                         SceneryKind::GasStation})
		{
			game = playing();
			game.scenery = {0, 50, kind, true};
			if (kind == SceneryKind::ZebraCrossing)
			{
				assert(at(render(game), game.road_bounds(52).left + 5, 52) ==
				       0xffff);
			}
			else
			{
				assert(at(render(game), 0, 50) != 0x2447);
			}
			game.scenery.x = -8;
			game.scenery.y = -12;
			render(game);
			game.scenery.x = Width - 8;
			game.scenery.y = Height - 5;
			render(game);
		}
		game         = playing();
		game.carHits = 1;
		assert(at(render(game), Width - 47, 5) == 0xffe0);
		game.progressMs = 250;
		assert(at(render(game), Width - 47, 5) == 0x5aeb);
		for (uint8_t damage : {49, 50, 89, 90, 99, 100})
		{
			game.damage = damage;
			const uint16_t expected =
			  damage == 100
			    ? 0x901f
			    : (damage >= 90 ? 0xf800 : (damage >= 50 ? 0xfd20 : 0xffe0));
			assert(at(render(game), 50, 20) == expected);
		}
		game         = Game{};
		game.phase   = Phase::Billing;
		game.phaseMs = BillFadeMs;
		game.bill[0] = {"PARKING TIP: MIKE", 1'250};
		assert(at(render(game), 51, 41) == 0xffff);
		assert(at(render(game), 51, 42) != 0xffff);
	}
} // namespace

int main()
{
	screen_flow();
	title_navigation();
	collisions_and_recovery();
	escaping_and_money();
	billing_sequence();
	damage_loot_and_police();
	large_vehicle_collisions();
	police_course_correction();
	police_explosions();
	gold_indicator();
	pickup_and_damage_indicators();
	wasted_screen_and_led_fade();
	decorative_scenery();
	world_and_steering();
	descending_leds();
	scene_fixtures();
	protected_gameplay();
	protection_failure_checks();
	rendering();
	std::puts("game: all native checks passed");
}
