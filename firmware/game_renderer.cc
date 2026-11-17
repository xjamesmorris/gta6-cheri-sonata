// SPDX-License-Identifier: MIT
#include "game_renderer.hh"
#include "source_qr.h"
#include "wallpaper.hh"

namespace
{
	using namespace driving;
	static_assert(artwork::primary::WallpaperWidth == PanelWidth &&
	              artwork::primary::WallpaperHeight == PanelHeight &&
	              artwork::alternate::WallpaperWidth == PanelWidth &&
	              artwork::alternate::WallpaperHeight == PanelHeight);
	std::array<uint8_t, FrameBytes> pixels{};

	constexpr uint16_t White = 0xffff;
	constexpr uint16_t Black = 0;
	constexpr uint16_t Red   = 0xf800;
	constexpr uint16_t Road  = 0x2965;

	// Original 5x7 glyphs, stored as five bits per row.
	constexpr uint8_t Font[26][7] = {
	  {14, 17, 17, 31, 17, 17, 17}, {30, 17, 17, 30, 17, 17, 30},
	  {14, 17, 16, 16, 16, 17, 14}, {30, 17, 17, 17, 17, 17, 30},
	  {31, 16, 16, 30, 16, 16, 31}, {31, 16, 16, 30, 16, 16, 16},
	  {14, 17, 16, 23, 17, 17, 14}, {17, 17, 17, 31, 17, 17, 17},
	  {31, 4, 4, 4, 4, 4, 31},      {7, 2, 2, 2, 18, 18, 12},
	  {17, 18, 20, 24, 20, 18, 17}, {16, 16, 16, 16, 16, 16, 31},
	  {17, 27, 21, 21, 17, 17, 17}, {17, 25, 25, 21, 19, 19, 17},
	  {14, 17, 17, 17, 17, 17, 14}, {30, 17, 17, 30, 16, 16, 16},
	  {14, 17, 17, 17, 21, 18, 13}, {30, 17, 17, 30, 20, 18, 17},
	  {15, 16, 16, 14, 1, 1, 30},   {31, 4, 4, 4, 4, 4, 4},
	  {17, 17, 17, 17, 17, 17, 14}, {17, 17, 17, 17, 17, 10, 4},
	  {17, 17, 17, 21, 21, 21, 10}, {17, 17, 10, 4, 10, 17, 17},
	  {17, 17, 10, 4, 4, 4, 4},     {31, 1, 2, 4, 8, 16, 31}};

	constexpr uint8_t SmallLetters[26][5] = {
	  {2, 5, 7, 5, 5}, {6, 5, 6, 5, 6}, {3, 4, 4, 4, 3}, {6, 5, 5, 5, 6},
	  {7, 4, 6, 4, 7}, {7, 4, 6, 4, 4}, {3, 4, 5, 5, 3}, {5, 5, 7, 5, 5},
	  {7, 2, 2, 2, 7}, {1, 1, 1, 5, 2}, {5, 5, 6, 5, 5}, {4, 4, 4, 4, 7},
	  {5, 7, 7, 5, 5}, {5, 7, 7, 7, 5}, {2, 5, 5, 5, 2}, {6, 5, 6, 4, 4},
	  {2, 5, 5, 7, 3}, {6, 5, 6, 5, 5}, {3, 4, 2, 1, 6}, {7, 2, 2, 2, 2},
	  {5, 5, 5, 5, 7}, {5, 5, 5, 5, 2}, {5, 5, 7, 7, 5}, {5, 5, 2, 5, 5},
	  {5, 5, 2, 2, 2}, {7, 1, 2, 4, 7}};
	constexpr uint8_t SmallDigits[10][5] = {{7, 5, 5, 5, 7},
	                                        {2, 6, 2, 2, 7},
	                                        {6, 1, 2, 4, 7},
	                                        {6, 1, 2, 1, 6},
	                                        {5, 5, 7, 1, 1},
	                                        {7, 4, 6, 1, 6},
	                                        {3, 4, 6, 5, 2},
	                                        {7, 1, 2, 2, 2},
	                                        {2, 5, 2, 5, 2},
	                                        {2, 5, 3, 1, 6}};

	constexpr char Car[CarHeight][CarWidth + 1] = {"  wwwwww  ",
	                                               " drrrrrrd ",
	                                               " drrrrrrd ",
	                                               "drggggggrd",
	                                               "drggggggrd",
	                                               "drggggggrd",
	                                               " drrrrrrd ",
	                                               "ddrrrrrrdd",
	                                               "ddrrrrrrdd",
	                                               " drrrrrrd ",
	                                               " dggggggd ",
	                                               " dggggggd ",
	                                               " drrrrrrd ",
	                                               " drrrrrrd ",
	                                               " drrrrrrd ",
	                                               "  yy  yy  "};

	void pixel(int x, int y, uint16_t color)
	{
		if (x >= 0 && x < Width && y >= 0 && y < Height)
		{
			const size_t offset = (y * PanelWidth + x) * 2;
			pixels[offset]      = static_cast<uint8_t>(color);
			pixels[offset + 1]  = static_cast<uint8_t>(color >> 8);
		}
	}

	void rect(int x, int y, int width, int height, uint16_t color)
	{
		for (int row = 0; row < height; ++row)
		{
			for (int col = 0; col < width; ++col)
			{
				pixel(x + col, y + row, color);
			}
		}
	}

	void text(const char *message, int x, int y, uint16_t color, int scale = 1)
	{
		for (; *message != '\0'; ++message, x += 6 * scale)
		{
			if (*message < 'A' || *message > 'Z')
			{
				continue;
			}
			const auto &glyph = Font[*message - 'A'];
			for (int row = 0; row < 7; ++row)
			{
				for (int col = 0; col < 5; ++col)
				{
					if ((glyph[row] & (16 >> col)) != 0)
					{
						rect(x + col * scale,
						     y + row * scale,
						     scale,
						     scale,
						     color);
					}
				}
			}
		}
	}

	int small_text_width(const char *message)
	{
		int width = 0;
		for (; *message != '\0'; ++message)
		{
			width += *message == ' ' ? 2 : 4;
		}
		return width == 0 ? 0 : width - 1;
	}

	void small_text(const char *message, int x, int y, uint16_t color)
	{
		constexpr uint8_t Dollar[] = {2, 7, 6, 3, 2};
		constexpr uint8_t Minus[]  = {0, 0, 7, 0, 0};
		constexpr uint8_t Comma[]  = {0, 0, 0, 2, 4};
		constexpr uint8_t Colon[]  = {0, 2, 0, 2, 0};
		for (; *message != '\0'; ++message)
		{
			if (*message == ' ')
			{
				x += 2;
				continue;
			}
			const uint8_t *glyph = nullptr;
			if (*message >= 'A' && *message <= 'Z')
			{
				glyph = SmallLetters[*message - 'A'];
			}
			else if (*message >= '0' && *message <= '9')
			{
				glyph = SmallDigits[*message - '0'];
			}
			else if (*message == '$')
			{
				glyph = Dollar;
			}
			else if (*message == '-')
			{
				glyph = Minus;
			}
			else if (*message == ',')
			{
				glyph = Comma;
			}
			else if (*message == ':')
			{
				glyph = Colon;
			}
			if (glyph != nullptr)
			{
				for (int row = 0; row < 5; ++row)
				{
					for (int col = 0; col < 3; ++col)
					{
						if ((glyph[row] & (4 >> col)) != 0)
						{
							pixel(x + col, y + row, color);
						}
					}
				}
			}
			x += 4;
		}
	}

	const char *format_money(int64_t value, char (&buffer)[32])
	{
		uint64_t amount = static_cast<uint64_t>(value);
		if (value < 0)
		{
			amount = uint64_t{0} - amount;
		}
		size_t position  = sizeof(buffer) - 1;
		buffer[position] = '\0';
		unsigned digits  = 0;
		do
		{
			if (digits != 0 && digits % 3 == 0)
			{
				buffer[--position] = ',';
			}
			buffer[--position] = static_cast<char>('0' + amount % 10);
			amount /= 10;
			++digits;
		} while (amount != 0);
		buffer[--position] = '$';
		if (value < 0)
		{
			buffer[--position] = '-';
		}
		return &buffer[position];
	}

	void money_bag(int x, int y)
	{
		rect(x + 3, y, 4, 2, 0x0540);
		rect(x + 2, y + 2, 6, 1, 0xbda0);
		for (int row = 3; row < 12; ++row)
		{
			const int inset = row < 5 || row == 11 ? 2 : 1;
			rect(x + inset, y + row, 10 - inset * 2, 1, 0x07a0);
		}
		small_text("$", x + 4, y + 5, 0xffe0);
	}

	void gold_pile(int x, int y, bool sparkle)
	{
		auto ingot = [](int bx, int by) {
			rect(bx, by, 6, 4, 0xb480);
			rect(bx, by, 6, 2, 0xffe0);
			rect(bx + 1, by, 4, 1, 0xfffb);
		};
		ingot(x + 4, y);
		ingot(x + 1, y + 4);
		ingot(x + 7, y + 4);
		ingot(x, y + 8);
		ingot(x + 8, y + 8);
		if (sparkle)
		{
			rect(x + 11, y, 1, 5, White);
			rect(x + 9, y + 2, 5, 1, White);
		}
	}

	void car(int x, int y, uint16_t body, bool reversed = false)
	{
		for (int row = 0; row < CarHeight; ++row)
		{
			for (int col = 0; col < CarWidth; ++col)
			{
				uint16_t color = Black;
				switch (Car[reversed ? CarHeight - 1 - row : row]
				           [reversed ? CarWidth - 1 - col : col])
				{
					case ' ':
						continue;
					case 'r':
						color = body;
						break;
					case 'g':
						color = 0x9e7b;
						break;
					case 'w':
						color = White;
						break;
					case 'y':
						color = Red;
						break;
					default:
						break;
				}
				pixel(x + col, y + row, color);
			}
		}
	}

	void traffic_vehicle(const Obstacle &vehicle)
	{
		const int x = vehicle.x;
		const int y = vehicle.y;
		if (vehicle.kind == ObjectKind::Car)
		{
			car(x, y, 0xd81b);
			return;
		}
		const int      width   = vehicle.width();
		const int      height  = vehicle.height();
		const bool     bus     = vehicle.kind == ObjectKind::Bus;
		const bool     trailer = vehicle.kind == ObjectKind::Trailer;
		const uint16_t body    = bus ? 0xff80 : (trailer ? 0x0619 : 0xfc40);
		rect(x + 1, y, width - 2, bus ? height : 9, Black);
		rect(x + 2, y + 1, width - 4, bus ? height - 2 : 7, body);
		rect(x + 3, y + 3, width - 6, 3, 0x9e7b);
		rect(x + 3, y, 2, 1, White);
		rect(x + width - 5, y, 2, 1, White);
		if (bus)
		{
			for (int row = 9; row < height - 3; row += 5)
			{
				rect(x + 2, y + row, 2, 3, 0x528a);
				rect(x + width - 4, y + row, 2, 3, 0x528a);
			}
			rect(x + width / 2 - 1, y + 10, 2, height - 15, White);
		}
		else
		{
			const int cargo = trailer ? 12 : 9;
			rect(x + 5, y + 8, 4, cargo - 7, Black);
			rect(x + 1, y + cargo, width - 2, height - cargo, Black);
			rect(x + 2,
			     y + cargo + 1,
			     width - 4,
			     height - cargo - 2,
			     trailer ? 0xce79 : 0xac84);
			for (int row = cargo + 3; row < height - 2; row += 4)
			{
				rect(x + 3, y + row, width - 6, 1, trailer ? White : 0x7b42);
			}
		}
		for (int row : {4, height - 7})
		{
			rect(x, y + row, 2, 4, Black);
			rect(x + width - 2, y + row, 2, 4, Black);
		}
		if (trailer)
		{
			rect(x, y + height - 12, 2, 4, Black);
			rect(x + width - 2, y + height - 12, 2, 4, Black);
		}
		rect(x + 3, y + height - 1, 2, 1, Red);
		rect(x + width - 5, y + height - 1, 2, 1, Red);
	}

	void police_explosion(const Police &cop)
	{
		const uint32_t elapsed = PoliceExplosionMs - cop.explosionMs;
		const bool     smoke   = elapsed >= 440;
		const int      radius  =
		  elapsed < 240 ? 4 + static_cast<int>(elapsed / 40) : (smoke ? 7 : 10);
		for (int y = -radius; y <= radius; ++y)
		{
			for (int x = -radius; x <= radius; ++x)
			{
				const int distance = x * x + y * y;
				if (distance <= radius * radius)
				{
					const uint16_t color =
					  smoke ? (distance < radius * radius / 2 ? 0xbdf7 : 0x738e)
					        : (distance < radius * radius / 4
					             ? White
					             : (distance < radius * radius / 2 ? 0xffe0
					                                                 : 0xfb00));
					pixel(cop.x + CarWidth / 2 + x,
					      cop.y + CarHeight / 2 + y,
					      color);
				}
			}
		}
	}

	void roadside_scenery(const Game &game)
	{
		if (!game.scenery.active)
		{
			return;
		}
		const int x = game.scenery.x;
		const int y = game.scenery.y;
		switch (game.scenery.kind)
		{
			case SceneryKind::ZebraCrossing:
			{
				for (int row = 0; row < 12; ++row)
				{
					const Bounds road = game.road_bounds(y + row);
					rect(road.left + 3, y + row, RoadWidth - 6, 1, Road);
					for (int col = road.left + 5; col < road.right - 6;
					     col += 8)
					{
						rect(col, y + row, 4, 1, White);
					}
				}
				const Bounds road = game.road_bounds(y + 15);
				rect(road.left + 4, y + 15, RoadWidth - 8, 1, White);
				break;
			}
			case SceneryKind::Billboard:
				rect(x + 5, y + 15, 2, 13, 0x528a);
				rect(x + 19, y + 15, 2, 13, 0x528a);
				rect(x, y, SceneryWidth, 16, Black);
				rect(x + 1, y + 1, SceneryWidth - 2, 14, 0x800e);
				small_text("BUY", x + 7, y + 3, White);
				small_text("MORE", x + 5, y + 9, 0xffe0);
				break;
			case SceneryKind::PoliceStation:
				rect(x + 2, y + 8, SceneryWidth - 4, 20, 0xbdf7);
				rect(x, y, SceneryWidth, 9, 0x001f);
				small_text("POLICE", x + 1, y + 2, White);
				rect(x + 4, y + 12, 4, 5, 0x0619);
				rect(x + 18, y + 12, 4, 5, 0x0619);
				rect(x + 10, y + 16, 6, 12, 0x528a);
				break;
			case SceneryKind::GasStation:
				rect(x, y + 8, SceneryWidth, 20, 0x738e);
				rect(x, y, SceneryWidth, 9, Red);
				small_text("GAS", x + 7, y + 2, White);
				rect(x + 2, y + 9, 1, 16, White);
				rect(x + 23, y + 9, 1, 16, White);
				for (int pump : {6, 16})
				{
					rect(x + pump, y + 14, 5, 10, White);
					rect(x + pump + 1, y + 15, 3, 3, Black);
					rect(x + pump + 5, y + 18, 2, 7, Black);
				}
				break;
		}
	}

	void stars(const Game &game)
	{
		constexpr uint8_t Shape[7] = {8, 8, 127, 62, 28, 54, 99};
		rect(Width - 52, 3, 49, 11, Black);
		for (int i = 0; i < 5; ++i)
		{
			uint16_t color = 0x5aeb;
			switch (game.star_appearance(static_cast<size_t>(i)))
			{
				case StarAppearance::Pending:
					color = 0xffe0;
					break;
				case StarAppearance::Wanted:
					color = Red;
					break;
				case StarAppearance::Expiring:
					color = 0x07e0;
					break;
				case StarAppearance::Off:
					break;
			}
			for (int y = 0; y < 7; ++y)
			{
				for (int x = 0; x < 7; ++x)
				{
					if ((Shape[y] & (64 >> x)) != 0)
					{
						pixel(Width - 50 + i * 9 + x, 5 + y, color);
					}
				}
			}
		}
	}

	void balance(const Game &game)
	{
		char        buffer[32];
		const char *amount = format_money(game.money, buffer);
		const int   width  = small_text_width(amount);
		rect(2, 3, width + 4, 9, Black);
		small_text(amount, 4, 5, game.money < 0 ? Red : 0x07e0);
	}

	void damage_meter(const Game &game)
	{
		const uint16_t color =
		  game.damage == 100
		    ? 0x901f
		    : (game.damage >= 90 ? Red : (game.damage >= 50 ? 0xfd20 : 0xffe0));
		rect(2, 15, Width - 4, 12, Black);
		rect(4, 17, Width - 8, 9, 0x5aeb);
		rect(5, 18, Width - 10, 7, Black);
		rect(5, 18, game.damage * (Width - 10) / 100, 7, color);
		small_text("DMG", 9, 20, Black);
		small_text("DMG", 8, 19, White);
	}

	void protection_status(const Game &game)
	{
		const auto &state = game.protection;
		if (state.tagDenials == 0 && state.roadDenials == 0 &&
		    state.walletDenials == 0)
		{
			return;
		}
		char label[] = "T:000 B:000 W:000";
		const uint32_t counts[] = {
		  state.tagDenials, state.roadDenials, state.walletDenials};
		for (size_t i = 0; i < 3; ++i)
		{
			const uint32_t count = counts[i] > 999 ? 999 : counts[i];
			label[i * 6 + 2] = static_cast<char>('0' + count / 100);
			label[i * 6 + 3] = static_cast<char>('0' + count / 10 % 10);
			label[i * 6 + 4] = static_cast<char>('0' + count % 10);
		}
		const int width = small_text_width(label);
		rect((Width - width) / 2 - 2, Height - 8, width + 4, 7, Black);
		small_text(label, (Width - width) / 2, Height - 7, 0x07e0);
	}

	void pickup_label(const char *message, int centerX, int y, uint16_t color)
	{
		const int width = small_text_width(message);
		int       x     = centerX - width / 2;
		x = x < 2 ? 2 : (x > Width - width - 2 ? Width - width - 2 : x);
		small_text(message, x + 1, y + 1, Black);
		small_text(message, x, y, color);
	}

	void receipt(const Game &game)
	{
		rect(0, 0, Width, Height, 0x0862);
		rect(4, 7, Width - 8, 1, 0x7bef);
		rect(4, Height - 8, Width - 8, 1, 0x7bef);
		constexpr char Title[] = "ITEMIZED REGRET";
		small_text(Title, (Width - small_text_width(Title)) / 2, 14, 0xffe0);
		auto row = [](const char *label, int64_t value, int y, uint16_t color) {
			char        buffer[32];
			const char *amount = format_money(value, buffer);
			small_text(label, 8, y, White);
			small_text(amount, Width - small_text_width(amount) - 8, y, color);
		};
		row("CASH",
		    game.billOpeningMoney,
		    31,
		    game.billOpeningMoney < 0 ? Red : 0x07e0);
		for (size_t i = 0; i < BillItemCount; ++i)
		{
			if (game.phaseMs >= BillFadeMs + i * BillLineMs)
			{
				row(game.bill[i].label,
				    -game.bill_item_shown(i),
				    40 + static_cast<int>(i) * 12,
				    game.phaseMs < BillFadeMs + (i + 1) * BillLineMs ? 0xffe0
				                                                     : Red);
			}
		}
		rect(8, 105, Width - 16, 1, 0x7bef);
		const int64_t charges = game.bill_shown_total();
		row("CHARGES", -charges, 111, Red);
		const int64_t remaining = game.billOpeningMoney - charges;
		row("BALANCE", remaining, 124, remaining < 0 ? Red : 0x07e0);
		const char *footer = game.phaseMs >= BillReadyMs
		                       ? "LEFT RESET  RIGHT DRIVE"
		                       : "COUNTING YOUR LOSSES";
		small_text(footer, (Width - small_text_width(footer)) / 2, 140, 0x9cf3);
	}

	void source_code()
	{
		constexpr int Scale = (Width - 4) / SourceQrSize;
		constexpr int Size = SourceQrSize * Scale;
		static_assert(Scale >= 2 && Size <= Height - 32);
		constexpr int Left = (Width - Size) / 2;
		constexpr int Top = (Height - Size) / 2;
		rect(0, 0, Width, Height, Black);
		rect(Left, Top, Size, Size, White);
		for (uint32_t y = 0; y < SourceQrSize; ++y)
		{
			for (uint32_t x = 0; x < SourceQrSize; ++x)
			{
				if ((SourceQrBits[y][x / 8] & (0x80 >> (x % 8))) != 0)
				{
					rect(Left + static_cast<int>(x) * Scale,
					     Top + static_cast<int>(y) * Scale, Scale, Scale, Black);
				}
			}
		}
		constexpr char Title[] = "SOURCE CODE";
		constexpr char Return[] = "ANY INPUT TO RETURN";
		small_text(Title, (Width - small_text_width(Title)) / 2, 5, White);
		small_text(Return, (Width - small_text_width(Return)) / 2, Height - 10, White);
	}

	void desaturate(uint32_t colorLevel)
	{
		if (colorLevel == 256)
		{
			return;
		}
		const uint32_t grayLevel = 256 - colorLevel;
		for (size_t i = 0; i < FrameBytes; i += 2)
		{
			const uint16_t color =
			  static_cast<uint16_t>(pixels[i] | (pixels[i + 1] << 8));
			const uint32_t red   = (color >> 11) & 31;
			const uint32_t green = (color >> 5) & 63;
			const uint32_t blue  = color & 31;
			// Five-bit luminance, with green expanded for neutral LCD grays.
			const uint32_t gray =
			  (77 * red + 150 * (green >> 1) + 29 * blue + 128) >> 8;
			const uint32_t grayGreen = (gray << 1) | (gray >> 4);
			const uint32_t outRed =
			  (red * colorLevel + gray * grayLevel + 128) >> 8;
			const uint32_t outGreen =
			  (green * colorLevel + grayGreen * grayLevel + 128) >> 8;
			const uint32_t outBlue =
			  (blue * colorLevel + gray * grayLevel + 128) >> 8;
			const uint16_t result =
			  static_cast<uint16_t>((outRed << 11) | (outGreen << 5) | outBlue);
			pixels[i]     = static_cast<uint8_t>(result);
			pixels[i + 1] = static_cast<uint8_t>(result >> 8);
		}
	}

	void driving_scene(const Game &game)
	{
		for (size_t i = 0; i < FrameBytes; i += 2)
		{
			pixels[i]     = 0x47;
			pixels[i + 1] = 0x24;
		}
		for (int y = 0; y < Height; ++y)
		{
			const Bounds   bounds = game.road_bounds(y);
			const uint32_t world  = game.distance + Height - 1 - y;
			rect(bounds.left, y, bounds.right - bounds.left, 1, Road);
			const uint16_t curb = (world / 8) % 2 == 0 ? White : Red;
			rect(bounds.left, y, 2, 1, curb);
			rect(bounds.right - 2, y, 2, 1, curb);
			if (world % 24 < 12)
			{
				rect((bounds.left + bounds.right) / 2 - 1, y, 2, 1, 0xff4a);
			}
		}
		roadside_scenery(game);
		for (const auto &obstacle : game.obstacles)
		{
			if (!obstacle.active)
			{
				continue;
			}
			if (obstacle.kind == ObjectKind::MoneyBag ||
			    obstacle.kind == ObjectKind::GoldPile)
			{
				if (obstacle.kind == ObjectKind::GoldPile)
				{
					gold_pile(
					  obstacle.x, obstacle.y, (game.activeMs / 200) % 2 == 0);
				}
				else
				{
					money_bag(obstacle.x, obstacle.y);
				}
				char buffer[32];
				pickup_label(format_money(obstacle.value, buffer),
				             obstacle.x + obstacle.width() / 2,
				             obstacle.y + obstacle.height() + 2,
				             obstacle.value >= JackpotValue ? 0xffe0 : White);
			}
			else if (obstacle.is_vehicle())
			{
				traffic_vehicle(obstacle);
			}
			else if (obstacle.kind == ObjectKind::Cone)
			{
				for (int y = 0; y < 8; ++y)
				{
					const int half = y / 2;
					rect(obstacle.x + 3 - half,
					     obstacle.y + y,
					     2 + half * 2,
					     1,
					     y == 3 || y == 4 ? White : 0xfb00);
				}
				rect(obstacle.x, obstacle.y + 8, 8, 2, Black);
			}
			else
			{
				rect(obstacle.x, obstacle.y, 18, 8, Black);
				for (int y = 1; y < 7; ++y)
				{
					for (int x = 1; x < 17; ++x)
					{
						if (((x + y) / 3) % 2 == 0)
						{
							pixel(obstacle.x + x, obstacle.y + y, 0xffe0);
						}
					}
				}
			}
		}
		for (size_t i = 0; i < game.police_limit(); ++i)
		{
			const auto &cop = game.police[i];
			if (cop.explosionMs != 0)
			{
				police_explosion(cop);
				continue;
			}
			if (!cop.active)
			{
				continue;
			}
			car(cop.x, cop.y, 0x001f, i == 2);
			const bool flash = ((game.activeMs / 125) + i) % 2 == 0;
			rect(cop.x + 2, cop.y + 7, 3, 2, flash ? Red : White);
			rect(cop.x + 5, cop.y + 7, 3, 2, flash ? White : 0x001f);
		}
		if (game.car_visible())
		{
			car(game.carX, game.carY, game.damage >= 70 ? 0xfc40 : 0x067f);
			if (game.damage >= 50)
			{
				rect(game.carX + 2,
				     game.carY + CarHeight + 2 +
				       static_cast<int>((game.activeMs / 80) % 3),
				     3,
				     2,
				     0x9cf3);
			}
		}
		if (game.pickupMs != 0 && game.phase == Phase::Playing)
		{
			char      buffer[32];
			const int y =
			  game.carY - 8 - static_cast<int>((1'000 - game.pickupMs) / 100);
			pickup_label(format_money(game.lastPickup, buffer),
			             game.carX + CarWidth / 2,
			             y,
			             0xffe0);
			if (game.pickedGold || game.lastPickup >= JackpotValue)
			{
				pickup_label(game.pickedGold ? "GOLD RUSH" : "JACKPOT",
				             game.carX + CarWidth / 2,
				             y - 8,
				             0xffe0);
			}
		}
		protection_status(game);
		stars(game);
		damage_meter(game);
		balance(game);
	}
} // namespace

namespace driving
{
	Frame render(const Game &game)
	{
		if (game.protection.failed())
		{
			rect(0, 0, Width, Height, Black);
			small_text("PROTECTION ERROR", 16, 55, Red);
			small_text("GAME STOPPED", 25, 75, White);
			small_text("RESET REQUIRED", 21, 95, White);
			return {pixels.data(), 256};
		}
		if (game.phase == Phase::FadeIn || game.phase == Phase::Wallpaper)
		{
			return {artwork::pixels(game.alternateArtwork), game.brightness()};
		}
		if (game.phase == Phase::Prompt)
		{
			const auto *wallpaper = artwork::pixels(game.alternateArtwork);
			for (size_t i = 0; i < FrameBytes; ++i)
			{
				pixels[i] = wallpaper[i];
			}
			constexpr char Prompt[] = "RIGHT: PLAY STORY MODE";
			const int x = (Width - small_text_width(Prompt)) / 2;
			const int y = Height - 7;
			small_text(Prompt, x + 1, y + 1, Black);
			small_text(Prompt, x, y, White);
		}
		else if (game.phase == Phase::SourceQr)
		{
			source_code();
		}
		else if (game.phase == Phase::Billing)
		{
			receipt(game);
		}
		else
		{
			driving_scene(game);
			if (game.phase == Phase::Wasted)
			{
				for (size_t i = 0; i < FrameBytes; i += 2)
				{
					const uint16_t color =
					  static_cast<uint16_t>(pixels[i] | (pixels[i + 1] << 8));
					const uint16_t dim = (color & 0xf7deu) >> 1;
					pixels[i]          = static_cast<uint8_t>(dim);
					pixels[i + 1]      = static_cast<uint8_t>(dim >> 8);
				}
				protection_status(game);
				stars(game);
				damage_meter(game);
				balance(game);
				text("WASTED",
				     (Width - 70) / 2 + 1,
				     (Height - 14) / 2 + 1,
				     Black,
				     2);
				text("WASTED", (Width - 70) / 2, (Height - 14) / 2, Red, 2);
				constexpr char Next[] = "PRESS FOR THE DAMAGE";
				small_text(Next,
				           (Width - small_text_width(Next)) / 2,
				           Height - 16,
				           White);
				desaturate(game.wasted_color_level());
			}
		}
		return {pixels.data(), game.brightness()};
	}
} // namespace driving
