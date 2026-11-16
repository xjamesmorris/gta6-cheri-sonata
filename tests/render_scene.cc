// SPDX-License-Identifier: MIT
#include "game_renderer.hh"
#include "game_scenarios.hh"

#include <cstdio>
#include <cstdlib>

int main(int argc, char **argv)
{
	if (argc != 4 && argc != 5)
	{
		std::fputs("Usage: render_scene SCENE OUTPUT.rgb565 OUTPUT.ppm [PROTECTION.json]\n",
		           stderr);
		return 2;
	}
	driving::Game game;
	if (!driving::test::prepare_scene(game, std::atoi(argv[1])) ||
	    game.protection.failed())
	{
		std::fputs("Invalid or failed scene\n", stderr);
		return 1;
	}
	const auto frame  = driving::render(game);
	std::FILE *output = std::fopen(argv[2], "wb");
	if (output == nullptr)
	{
		std::perror(argv[2]);
		return 1;
	}
	for (size_t i = 0; i < driving::FrameBytes; i += 2)
	{
		const uint16_t color   = driving::test::scaled_pixel(frame, i);
		const uint8_t  bytes[] = {static_cast<uint8_t>(color),
		                          static_cast<uint8_t>(color >> 8)};
		if (std::fwrite(bytes, 1, 2, output) != 2)
		{
			std::perror("Writing scene");
			std::fclose(output);
			return 1;
		}
	}
	if (std::fclose(output) != 0)
	{
		std::perror("Closing scene");
		return 1;
	}
	output = std::fopen(argv[3], "wb");
	if (output == nullptr)
	{
		std::perror(argv[3]);
		return 1;
	}
	if (std::fprintf(
	      output, "P6 %d %d 63\n", driving::PanelWidth, driving::PanelHeight) <
	    0)
	{
		std::perror("Writing PPM header");
		std::fclose(output);
		return 1;
	}
	for (size_t i = 0; i < driving::FrameBytes; i += 2)
	{
		const uint16_t color   = driving::test::scaled_pixel(frame, i);
		const uint8_t  red     = (color >> 11) & 31;
		const uint8_t  green   = (color >> 5) & 63;
		const uint8_t  blue    = color & 31;
		const uint8_t  bytes[] = {
          static_cast<uint8_t>((red << 1) | (red >> 4)),
          green,
          static_cast<uint8_t>((blue << 1) | (blue >> 4))};
		if (std::fwrite(bytes, 1, 3, output) != 3)
		{
			std::perror("Writing PPM pixels");
			std::fclose(output);
			return 1;
		}
	}
	if (std::fclose(output) != 0)
	{
		std::perror("Closing PPM");
		return 1;
	}
	if (argc == 5)
	{
		output = std::fopen(argv[4], "w");
		if (output == nullptr)
		{
			std::perror(argv[4]);
			return 1;
		}
		const auto totals = driving::protection_fault_totals();
		if (std::fprintf(output,
		                 "{\"origin\":%u,\"tag\":%u,\"bounds\":%u,\"store\":%u,\"local\":%u}\n",
		                 static_cast<unsigned>(driving::Protection::origin()),
		                 totals.tag, totals.bounds, totals.store, totals.local) < 0)
		{
			std::perror("Writing protection expectations");
			std::fclose(output);
			return 1;
		}
		if (std::fclose(output) != 0)
		{
			std::perror("Closing protection expectations");
			return 1;
		}
	}
}
