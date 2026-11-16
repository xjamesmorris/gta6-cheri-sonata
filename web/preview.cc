// SPDX-License-Identifier: MIT
#include "game.hh"
#include "game_renderer.hh"

namespace
{
	driving::Game  game;
	driving::Frame frame{};
} // namespace

extern "C"
{
	void preview_init(uint32_t seed)
	{
		game = driving::Game{};
		game.reset(seed);
	}

	uint32_t preview_artwork()
	{
		return game.alternateArtwork;
	}

	void preview_toggle_artwork()
	{
		game.change_artwork();
	}

	uint32_t preview_width()
	{
		return driving::Width;
	}

	uint32_t preview_height()
	{
		return driving::Height;
	}

	void preview_advance(uint32_t milliseconds, uint32_t input)
	{
		game.advance(milliseconds, static_cast<uint8_t>(input));
	}

	const uint8_t *preview_render()
	{
		frame = driving::render(game);
		return frame.pixels;
	}

	uint32_t preview_scale()
	{
		return frame.scale;
	}

	uint32_t preview_phase()
	{
		return static_cast<uint32_t>(game.phase);
	}

	uint32_t preview_stars()
	{
		return game.stars;
	}

	int64_t preview_money()
	{
		return game.money;
	}

	int64_t preview_bill_total()
	{
		return game.bill_total();
	}

	uint32_t preview_led_mask()
	{
		return game.led_mask();
	}

	uint32_t preview_rgb1()
	{
		return game.rgb_colors()[0];
	}

	uint32_t preview_rgb2()
	{
		return game.rgb_colors()[1];
	}

	uint32_t preview_damage()
	{
		return game.damage;
	}

	int preview_x()
	{
		return game.carX;
	}

	int preview_y()
	{
		return game.carY;
	}

	uint32_t preview_pending_hits()
	{
		return game.carHits;
	}

	uint32_t preview_distance()
	{
		return game.distance;
	}

	uint32_t preview_protection_origin()
	{
		return static_cast<uint32_t>(driving::Protection::origin());
	}

	uint32_t preview_protection_failed()
	{
		return game.protection.failed();
	}

	uint32_t preview_tag_denials()
	{
		return game.protection.tagDenials;
	}

	uint32_t preview_wallet_denials()
	{
		return game.protection.walletDenials;
	}

	uint32_t preview_road_denials()
	{
		return game.protection.roadDenials;
	}
}
