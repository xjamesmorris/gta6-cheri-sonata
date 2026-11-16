// SPDX-License-Identifier: MIT
#include "game.hh"

namespace
{
	uint32_t mix(uint32_t value)
	{
		value ^= value >> 16;
		value *= 0x7feb352du;
		value ^= value >> 15;
		value *= 0x846ca68bu;
		return value ^ (value >> 16);
	}

	int clamp(int value, int low, int high)
	{
		return value < low ? low : (value > high ? high : value);
	}

	bool overlaps_car(int carX, int carY, int x, int y, int width, int height)
	{
		return carX + 1 < x + width - 1 &&
		       carX + driving::CarWidth - 1 > x + 1 &&
		       carY + 1 < y + height - 1 &&
		       carY + driving::CarHeight - 1 > y + 1;
	}

	struct FeeOffer
	{
		const char *label;
		int64_t     base;
		int64_t     step;
		uint32_t    variants;
	};

	constexpr FeeOffer Fees[] = {{"BOND", 20'000, 5'000, 9},
	                             {"BONUS FOR SAUL", 9'999, 2'500, 9},
	                             {"DONATE POLICE", 5'000, 1'000, 10},
	                             {"DONATE ATT GEN", 20'000, 2'000, 9},
	                             {"DONATE JUDGE", 30'000, 4'000, 10},
	                             {"DONATE WITNESS", 10'000, 1'500, 8},
	                             {"PARKING TIP: MIKE", 1'250, 500, 10},
	                             {"IMPOUND FEES", 8'000, 1'000, 8},
	                             {"TOWING FEES", 1'500, 1'000, 10},
	                             {"INSURANCE COPAY", 10'000, 1'500, 10},
	                             {"CONVENIENCE FEE", 999, 1'000, 6}};
} // namespace

namespace driving
{
	int Obstacle::width() const
	{
		if (kind == ObjectKind::GoldPile || kind == ObjectKind::Truck ||
		    kind == ObjectKind::Bus || kind == ObjectKind::Trailer)
		{
			return 14;
		}
		return kind == ObjectKind::Cone
		         ? 8
		         : (kind == ObjectKind::Barrier ? 18 : CarWidth);
	}

	int Obstacle::height() const
	{
		switch (kind)
		{
			case ObjectKind::Truck:
				return 24;
			case ObjectKind::Bus:
				return 30;
			case ObjectKind::Trailer:
				return TrailerHeight;
			default:
				break;
		}
		return kind == ObjectKind::MoneyBag || kind == ObjectKind::GoldPile
		         ? 12
		         : (kind == ObjectKind::Cone
		              ? 10
		              : (kind == ObjectKind::Barrier ? 8 : CarHeight));
	}

	bool Obstacle::is_vehicle() const
	{
		return kind == ObjectKind::Car || kind == ObjectKind::Truck ||
		       kind == ObjectKind::Bus || kind == ObjectKind::Trailer;
	}

	void Game::reset(uint32_t seed)
	{
		const bool selectedArtwork = alternateArtwork;
		*this    = Game{};
		alternateArtwork = selectedArtwork;
		baseSeed = seed;
		protection.reset_wallet(money);
	}

	void Game::change_artwork()
	{
		alternateArtwork = !alternateArtwork;
		++revision;
	}

	void Game::reset_run_state(uint32_t seed)
	{
		const Protection saved = protection;
		const bool selectedArtwork = alternateArtwork;
		*this = Game{};
		protection = saved;
		alternateArtwork = selectedArtwork;
		baseSeed = seed;
		protection.read_wallet(money);
	}

	void Game::start_run(bool fade)
	{
		const uint32_t savedSeed  = baseSeed;
		const uint32_t nextRun    = runs + 1;
		reset_run_state(savedSeed);
		runs         = nextRun;
		roadSeed     = mix(baseSeed ^ runs);
		randomState  = roadSeed;
		sceneryState = mix(roadSeed ^ 0x7f4a7c15u);
		nextScenery  = 800 + sceneryState % 801;
		hitsNeeded =
		  static_cast<uint8_t>(2 + (mix(roadSeed ^ 0xa511e9b3u) & 1));
		phase          = Phase::Playing;
		playFadeMs     = fade ? PostFadeMs : 0;
		waitForNeutral = fade;
		++revision;
	}

	void Game::start_demo()
	{
		const uint32_t seed = baseSeed;
		reset(seed);
		if (!protection.failed())
		{
			start_run();
			demoActive = true;
		}
	}

	void Game::begin_bill()
	{
		if (!protection.read_wallet(money))
		{
			++revision;
			return;
		}
		billOpeningMoney          = money;
		billedItems               = 0;
		bill[0]                   = {"HOSPITAL BILL", HospitalBill};
		bill[1]                   = {"AUTO REPAIRS", repair_cost()};
		constexpr size_t FeeCount = sizeof(Fees) / sizeof(Fees[0]);
		static_assert(BillItemCount - 2 <= FeeCount);
		std::array<size_t, FeeCount> choices{};
		for (size_t i = 0; i < FeeCount; ++i)
		{
			choices[i] = i;
		}
		for (size_t i = 0; i < BillItemCount - 2; ++i)
		{
			randomState              = mix(randomState + 0x9e3779b9u);
			const size_t    selected = i + randomState % (FeeCount - i);
			const FeeOffer &fee      = Fees[choices[selected]];
			choices[selected]        = choices[i];
			bill[i + 2]              = {fee.label,
			                            fee.base +
			                              ((randomState >> 8) % fee.variants) * fee.step};
		}
		phase     = Phase::Billing;
		phaseMs   = 0;
		fadingOut = false;
		armed     = false;
		++revision;
	}

	void Game::enter_wasted(uint8_t input)
	{
		phase   = Phase::Wasted;
		phaseMs = 0;
		damage  = 100;
		cleanMs = 0;
		armed   = (input & Press) == 0;
	}

	void Game::advance(uint32_t elapsedMs, uint8_t input)
	{
		if (protection.failed())
		{
			return;
		}
		input &= Left | Right | Up | Down | Press;
		if (demoActive)
		{
			if (input == 0)
			{
				demoStopArmed = true;
			}
			else if (demoStopArmed)
			{
				const uint32_t seed = baseSeed;
				reset(seed);
				phase = Phase::Prompt;
				++revision;
				return;
			}
			input = 0;
		}
		if (phase == Phase::Wasted)
		{
			const uint32_t previousColor = wasted_color_level();
			const uint32_t duration = demoActive ? DemoEndMs : WastedHoldMs + WastedFadeMs;
			const uint32_t remaining = duration - phaseMs;
			phaseMs += elapsedMs < remaining ? elapsedMs : remaining;
			if (advance_explosions(elapsedMs) ||
			    wasted_color_level() != previousColor)
			{
				++revision;
			}
			if (demoActive && phaseMs == DemoEndMs)
			{
				const bool canStop = demoStopArmed;
				start_demo();
				demoStopArmed = canStop;
				return;
			}
		}
		if (fadingOut)
		{
			const uint32_t remaining = PostFadeMs - fadeOutMs;
			fadeOutMs += elapsedMs < remaining ? elapsedMs : remaining;
			++revision;
			if (fadeOutMs == PostFadeMs)
			{
				if (phase == Phase::Wasted)
				{
					begin_bill();
				}
				else if (returnToGame)
				{
					start_run(true);
				}
				else
				{
					const uint32_t seed = baseSeed;
					reset(seed);
					splashFadeMs = PostFadeMs;
					++revision;
				}
			}
			return;
		}
		if (phase == Phase::FadeIn || phase == Phase::Wallpaper)
		{
			// Consume only this screen's duration, avoiding timer overflow and
			// never carrying a held start input into the prompt.
			while (phase == Phase::FadeIn || phase == Phase::Wallpaper)
			{
				const uint32_t duration =
				  phase == Phase::FadeIn ? splashFadeMs : PromptDelayMs;
				const uint32_t remaining = duration - phaseMs;
				if (elapsedMs < remaining)
				{
					phaseMs += elapsedMs;
					return;
				}
				elapsedMs -= remaining;
				phaseMs = 0;
				phase =
				  phase == Phase::FadeIn ? Phase::Wallpaper : Phase::Prompt;
				++revision;
			}
			armed = input == 0;
			return;
		}

		if (phase == Phase::Prompt || phase == Phase::SourceQr)
		{
			if (input == 0)
			{
				armed = true;
			}
			else if (armed)
			{
				armed = false;
				if (phase == Phase::SourceQr)
				{
					phase = Phase::Prompt;
					phaseMs = 0;
					++revision;
				}
				else if (input == Right)
				{
					start_run();
				}
				else if (input == Down)
				{
					change_artwork();
				}
				else if (input == Press)
				{
					start_demo();
				}
				else if (input == Left)
				{
					phase = Phase::SourceQr;
					phaseMs = 0;
					++revision;
				}
			}
			return;
		}

		if (phase == Phase::Wasted)
		{
			if ((input & Press) == 0)
			{
				armed = true;
			}
			else if (armed)
			{
				fadingOut = true;
				fadeOutMs = 0;
				++revision;
			}
			return;
		}

		if (phase == Phase::Billing)
		{
			const bool     wasReady  = phaseMs == BillReadyMs;
			const uint32_t remaining = BillReadyMs - phaseMs;
			phaseMs += elapsedMs < remaining ? elapsedMs : remaining;
			while (billedItems < BillItemCount &&
			       phaseMs >= BillFadeMs + (billedItems + 1) * BillLineMs)
			{
				const int64_t amount = bill[billedItems++].amount;
				if (!protection.debit(amount, money))
				{
					++revision;
					return;
				}
			}
			if (!wasReady)
			{
				++revision;
				armed = phaseMs == BillReadyMs && input == 0;
			}
			else if (input == 0)
			{
				armed = true;
			}
			else if (armed && (((input & Left) != 0) != ((input & Right) != 0)))
			{
				returnToGame = (input & Right) != 0;
				fadingOut    = true;
				fadeOutMs    = 0;
				++revision;
			}
			return;
		}

		if (phaseMs < playFadeMs)
		{
			const uint32_t remaining = playFadeMs - phaseMs;
			phaseMs += elapsedMs < remaining ? elapsedMs : remaining;
			++revision;
			if (input == 0)
			{
				waitForNeutral = false;
			}
			return;
		}
		if (waitForNeutral)
		{
			waitForNeutral = input != 0;
			input          = 0;
		}

		// The adapters bound real-time catch-up. Consume elapsed time without
		// overflowing even when deterministic tests advance longer intervals.
		while (elapsedMs >= TickMs - remainderMs)
		{
			elapsedMs -= TickMs - remainderMs;
			remainderMs = 0;
			tick(input);
			if (phase == Phase::Wasted || protection.failed())
			{
				return;
			}
		}
		remainderMs += elapsedMs;
	}

	int Game::road_center(uint32_t worldY) const
	{
		constexpr uint32_t StraightLength = Height + 64;
		constexpr int      MinCenter      = RoadWidth / 2 + RoadMargin;
		constexpr int      CenterRange = Width - RoadWidth - 2 * RoadMargin + 1;
		if (worldY < StraightLength)
		{
			return Width / 2;
		}
		const uint32_t segment = (worldY - StraightLength) / 96;
		const uint32_t offset  = (worldY - StraightLength) % 96;
		const int      from =
          segment == 0 ? Width / 2
		                    : MinCenter + static_cast<int>(mix(roadSeed + segment) %
                                                      CenterRange);
		const int to = MinCenter + static_cast<int>(
		                             mix(roadSeed + segment + 1) % CenterRange);
		const uint32_t t     = offset * 256 / 96;
		const int      blend = static_cast<int>((t * t * (768 - 2 * t)) >> 16);
		return from + (to - from) * blend / 256;
	}

	Bounds Game::road_bounds(int y) const
	{
		const int center =
		  road_center(distance + static_cast<uint32_t>(Height - 1 - y));
		return {center - RoadWidth / 2, center + RoadWidth / 2};
	}

	void Game::spawn_obstacle()
	{
		static_assert(ObstacleCount * SpawnSpacing > Height + TrailerHeight);
		constexpr ObjectKind Kinds[] = {
		  ObjectKind::Car,      ObjectKind::Car,      ObjectKind::Car,
		  ObjectKind::Truck,    ObjectKind::Truck,    ObjectKind::Bus,
		  ObjectKind::Trailer,  ObjectKind::Cone,     ObjectKind::Barrier,
		  ObjectKind::MoneyBag, ObjectKind::MoneyBag, ObjectKind::MoneyBag};
		constexpr size_t KindCount = sizeof(Kinds) / sizeof(Kinds[0]);
		randomState        = mix(randomState + 0x9e3779b9u);
		Obstacle &obstacle = obstacles[nextObstacle];
		nextObstacle       = (nextObstacle + 1) % ObstacleCount;
		obstacle.kind      = (randomState >> 8) % 16 == 0
		                       ? ObjectKind::GoldPile
		                       : Kinds[randomState % KindCount];
		if (obstacle.kind == ObjectKind::MoneyBag)
		{
			constexpr int64_t Values[] = {500, 1'000, 2'500, 5'000};
			const uint32_t    loot     = mix(randomState ^ 0x51ed270bu);
			obstacle.value =
			  loot % 32 == 0 ? JackpotValue : Values[(loot >> 5) % 4];
		}
		else if (obstacle.kind == ObjectKind::GoldPile)
		{
			obstacle.value =
			  GoldValue * (((randomState >> 16) % 4 == 0) ? 2 : 1);
		}
		obstacle.y        = static_cast<int16_t>(-obstacle.height());
		const Bounds road = road_bounds(obstacle.y + obstacle.height() / 2);
		obstacle.x        = static_cast<int16_t>(
          road.left + 8 +
          static_cast<int>((randomState >> 8) %
                           (RoadWidth - 16 - obstacle.width())));
		obstacle.active = true;
	}

	void Game::update_scenery()
	{
		if (scenery.active)
		{
			scenery.y += 2;
			if (scenery.y >= Height)
			{
				scenery.active = false;
			}
		}
		if (static_cast<int32_t>(distance - nextScenery) < 0)
		{
			return;
		}
		sceneryState   = mix(sceneryState + 0x9e3779b9u);
		scenery.kind   = static_cast<SceneryKind>(sceneryState % 4);
		scenery.y      = -SceneryHeight;
		scenery.active = true;
		int left       = Width;
		int right      = 0;
		for (int y = scenery.y; y < scenery.y + SceneryHeight; ++y)
		{
			const Bounds road = road_bounds(y);
			left              = road.left < left ? road.left : left;
			right             = road.right > right ? road.right : right;
		}
		scenery.x = static_cast<int16_t>(
		  left >= Width - right ? left - SceneryWidth - 2 : right + 2);
		nextScenery = distance + 1'000 + (sceneryState >> 8) % 1'001;
	}

	void Game::prepare_demo_tick(Bounds road)
	{
		carX = (road.left + road.right - CarWidth) / 2;
		for (size_t i = 1; i < obstacles.size(); ++i)
		{
			obstacles[i].active = false;
		}
		for (auto &cop : police)
		{
			if (cop.active)
			{
				cop.x = road_bounds(cop.y + CarHeight / 2).right - CarWidth - 4;
				cop.targetX = cop.x;
				cop.courseCorrected = true;
			}
		}
		if (stars == 5)
		{
			if (cleanMs >= DemoFiveStarMs)
			{
				auto &cop = police[0];
				if (cop.active)
				{
					cop.x = carX;
					cop.targetX = carX;
					cop.y = carY + 2;
				}
				else
				{
					cop.enabled = true;
					cop.waitMs = 0;
				}
			}
			return;
		}
		if (activeMs == TickMs)
		{
			obstacles[0] = {static_cast<int16_t>(carX),
			                 static_cast<int16_t>(carY - 22),
			                 ObjectKind::MoneyBag, true};
		}
		else if (!obstacles[0].active)
		{
			if (money > 0 && protection.roadDenials == 0)
			{
				carX = road.left - 1;
			}
			else if (graceMs == 0)
			{
				obstacles[0] = {static_cast<int16_t>(carX),
				                 static_cast<int16_t>(carY - CarHeight - 10),
				                 ObjectKind::Car, true};
			}
		}
	}

	void Game::tick(uint8_t input)
	{
		++revision;
		activeMs += TickMs;
		distance += 2;
		graceMs     = graceMs > TickMs ? graceMs - TickMs : 0;
		wallGraceMs = wallGraceMs > TickMs ? wallGraceMs - TickMs : 0;
		pickupMs    = pickupMs > TickMs ? pickupMs - TickMs : 0;
		goldFlashMs = goldFlashMs > TickMs ? goldFlashMs - TickMs : 0;
		pickupFlashMs = pickupFlashMs > TickMs ? pickupFlashMs - TickMs : 0;
		damageFlashMs = damageFlashMs > TickMs ? damageFlashMs - TickMs : 0;
		const uint8_t previousDamage = damage;
		advance_explosions(TickMs);
		progressMs += TickMs;
		update_scenery();

		const int direction = ((input & Right) != 0) - ((input & Left) != 0);
		const int vertical  = ((input & Down) != 0) - ((input & Up) != 0);
		carX                = clamp(carX + direction * 4, 0, Width - CarWidth);
		carY                = clamp(carY + vertical * 2, MinCarY, MaxCarY);
		int roadLeft        = 0;
		int roadRight       = Width;
		for (int y = carY; y < carY + CarHeight; ++y)
		{
			const Bounds road = road_bounds(y);
			roadLeft          = road.left > roadLeft ? road.left : roadLeft;
			roadRight         = road.right < roadRight ? road.right : roadRight;
		}
		if (demoActive)
		{
			prepare_demo_tick({roadLeft, roadRight});
		}

		const bool arithmeticContact =
		  carX < roadLeft || carX + CarWidth > roadRight;
		const auto roadAccess = protection.check_road(
		  roadRight - 1 - carX, carX + CarWidth - 1 - roadLeft,
		  static_cast<size_t>(roadRight - roadLeft));
		if (roadAccess == RoadAccess::Error)
		{
			carX = clamp(carX, roadLeft + 8, roadRight - CarWidth - 8);
			return;
		}
		const bool wallContact = roadAccess == RoadAccess::Blocked;
		if (wallContact != arithmeticContact)
		{
			protection.fail(ProtectionError::BoundaryMismatch);
			carX = clamp(carX, roadLeft + 8, roadRight - CarWidth - 8);
			return;
		}
		bool       contact     = false;
		bool       carContact  = false;
		int        impactDamage = 0;
		int64_t    collected    = 0;
		bool       gold         = false;
		bool       highGold     = false;
		for (auto &obstacle : obstacles)
		{
			if (!obstacle.active)
			{
				continue;
			}
			obstacle.y += 2;
			if (obstacle.y >= Height)
			{
				obstacle.active = false;
				continue;
			}
			if (overlaps_car(carX,
			                 carY,
			                 obstacle.x,
			                 obstacle.y,
			                 obstacle.width(),
			                 obstacle.height()))
			{
				if (obstacle.kind == ObjectKind::MoneyBag ||
				    obstacle.kind == ObjectKind::GoldPile)
				{
					collected += obstacle.value;
					gold |= obstacle.kind == ObjectKind::GoldPile;
					highGold |= obstacle.kind == ObjectKind::GoldPile &&
					            obstacle.value > GoldValue;
				}
				else
				{
					contact = true;
					carContact |= obstacle.is_vehicle();
					impactDamage +=
					  obstacle.kind == ObjectKind::Cone
					    ? ConeDamage
					    : (obstacle.kind == ObjectKind::Barrier ? BarrierDamage
					                                            : CarDamage);
				}
				obstacle.active = false;
			}
		}
		if (collected != 0)
		{
			if (!protection.credit(collected, money))
			{
				return;
			}
			lastPickup = collected;
			pickupMs   = 1'000;
			pickedGold = gold;
			if (highGold)
			{
				goldFlashMs = GoldFlashMs;
			}
			else
			{
				pickupFlashMs = PickupFlashMs;
			}
		}
		if (wallContact)
		{
			carX = clamp(carX, roadLeft + 8, roadRight - CarWidth - 8);
			if (wallGraceMs == 0)
			{
				damage =
				  static_cast<uint8_t>(clamp(damage + WallDamage, 0, 100));
				wallGraceMs = WallGraceMs;
			}
		}
		const uint8_t policeHits  = update_police();
		if (protection.failed())
		{
			return;
		}
		const bool    fatalPolice = policeHits != 0 && stars == 5;
		contact |= policeHits != 0;
		carContact |= policeHits != 0;
		impactDamage += policeHits * PoliceDamage;
		if (contact)
		{
			cleanMs = 0;
			if (graceMs == 0)
			{
				damage =
				  static_cast<uint8_t>(clamp(damage + impactDamage, 0, 100));
				if (carContact)
				{
					record_car_hit();
				}
				graceMs = GraceMs;
			}
		}
		else if (!wallContact && (stars != 0 || carHits != 0))
		{
			cleanMs += TickMs;
			if (cleanMs >= RecoveryMs)
			{
				stars      = 0;
				carHits    = 0;
				progressMs = 0;
				cleanMs    = 0;
			}
		}
		if (wallContact)
		{
			cleanMs = 0;
		}
		if (damage > previousDamage)
		{
			if (!protection.damage_fault())
			{
				return;
			}
			damageFlashMs = DamageFlashMs;
		}
		if (fatalPolice || damage == 100)
		{
			if (fatalPolice && damage == previousDamage &&
			    !protection.damage_fault())
			{
				return;
			}
			enter_wasted(input);
			return;
		}
		for (size_t i = police_limit(); i < PoliceCount; ++i)
		{
			police[i] = Police{};
		}

		// Subtraction remains valid across the unsigned distance wrap.
		if (!demoActive && static_cast<int32_t>(distance - nextSpawn) >= 0)
		{
			spawn_obstacle();
			nextSpawn = distance + SpawnSpacing;
		}
	}

	uint32_t Game::brightness() const
	{
		if (fadingOut)
		{
			const uint32_t linear = (PostFadeMs - fadeOutMs) * 256 / PostFadeMs;
			return (linear * linear) >> 8;
		}
		if (phase == Phase::Billing)
		{
			uint32_t linear = 256;
			if (phaseMs < BillFadeMs)
			{
				linear = phaseMs * 256 / BillFadeMs;
			}
			return (linear * linear) >> 8;
		}
		if (phase == Phase::Playing && phaseMs < playFadeMs)
		{
			const uint32_t linear = phaseMs * 256 / playFadeMs;
			return (linear * linear) >> 8;
		}
		if (phase != Phase::FadeIn)
		{
			return 256;
		}
		const uint32_t linear = phaseMs * 256 / splashFadeMs;
		return (linear * linear) >> 8;
	}

	int64_t Game::bill_total() const
	{
		int64_t total = 0;
		for (const auto &item : bill)
		{
			total += item.amount;
		}
		return total;
	}

	int64_t Game::bill_item_shown(size_t index) const
	{
		const uint32_t start =
		  BillFadeMs + static_cast<uint32_t>(index) * BillLineMs;
		if (phaseMs <= start)
		{
			return 0;
		}
		const uint32_t elapsed = phaseMs - start;
		if (elapsed >= BillLineMs)
		{
			return bill[index].amount;
		}
		const uint32_t eased = elapsed * (2 * BillLineMs - elapsed);
		return bill[index].amount * eased / (BillLineMs * BillLineMs);
	}

	int64_t Game::bill_shown_total() const
	{
		int64_t total = 0;
		for (size_t i = 0; i < BillItemCount; ++i)
		{
			total += bill_item_shown(i);
		}
		return total;
	}

	bool Game::stars_visible() const
	{
		return !wanted_expiring() || ((cleanMs - EscapeFlashMs) / 500) % 2 != 0;
	}

	bool Game::wanted_expiring() const
	{
		return phase == Phase::Playing && (stars != 0 || carHits != 0) &&
		       cleanMs >= EscapeFlashMs;
	}

	StarAppearance Game::star_appearance(size_t index) const
	{
		if ((phase != Phase::Playing && phase != Phase::Wasted) || index >= 5 ||
		    (index >= stars && (index != stars || carHits == 0)))
		{
			return StarAppearance::Off;
		}
		if (wanted_expiring())
		{
			return stars_visible() ? StarAppearance::Expiring
			                       : StarAppearance::Off;
		}
		if (index < stars)
		{
			return StarAppearance::Wanted;
		}
		return phase == Phase::Wasted || (progressMs / 250) % 2 == 0
		         ? StarAppearance::Pending
		         : StarAppearance::Off;
	}

	bool Game::car_visible() const
	{
		return phase == Phase::Wasted || graceMs == 0 ||
		       ((GraceMs - graceMs) / 125) % 2 == 0;
	}

	uint8_t Game::led_mask() const
	{
		uint8_t mask = 0;
		for (unsigned i = 0; i < 5; ++i)
		{
			if (star_appearance(i) != StarAppearance::Off)
			{
				mask |= static_cast<uint8_t>(1u << (7 - i));
			}
		}
		return mask;
	}

	void Game::record_car_hit()
	{
		if (stars == 5)
		{
			return;
		}
		++carHits;
		progressMs = 0;
		if (carHits >= hitsNeeded)
		{
			++stars;
			carHits     = 0;
			randomState = mix(randomState + 0x9e3779b9u);
			hitsNeeded  = static_cast<uint8_t>(2 + (randomState & 1));
		}
	}

	size_t Game::police_limit() const
	{
		return stars < 3 ? 0 : (stars > 5 ? PoliceCount : stars - 2);
	}

	bool Game::advance_explosions(uint32_t elapsedMs)
	{
		bool changed = false;
		for (auto &cop : police)
		{
			if (cop.explosionMs != 0 && elapsedMs != 0)
			{
				cop.explosionMs =
				  cop.explosionMs > elapsedMs ? cop.explosionMs - elapsedMs : 0;
				changed = true;
			}
		}
		return changed;
	}

	uint8_t Game::update_police()
	{
		std::array<bool, PoliceCount> contacts{};
		std::array<bool, PoliceCount> playerHits{};
		for (size_t i = 0; i < PoliceCount; ++i)
		{
			Police &cop = police[i];
			if (i >= police_limit())
			{
				cop = Police{};
				continue;
			}
			if (!cop.enabled)
			{
				cop.enabled = true;
				randomState = mix(randomState + 0x9e3779b9u);
				cop.waitMs  = PoliceDelayMs + randomState % 1'701 +
				             static_cast<uint32_t>(i) * 200;
			}
			if (!cop.active)
			{
				if (cop.waitMs > TickMs)
				{
					cop.waitMs -= TickMs;
					continue;
				}
				if (cop.explosionMs != 0)
				{
					continue;
				}
				cop.active          = true;
				cop.y               = i == 2 ? -CarHeight - 12 : Height + 12;
				const Bounds road   = road_bounds(cop.y + CarHeight / 2);
				cop.x               = (road.left + road.right - CarWidth) / 2;
				cop.targetX         = cop.x;
				cop.courseCorrected = false;
				cop.inContact       = false;
				if (!protection.police_attack() ||
				    !protection.read_wallet(money))
				{
					return 0;
				}
			}
			cop.y += i == 2 ? 2 : -2;
			if ((i != 2 && cop.y + CarHeight < 0) ||
			    (i == 2 && cop.y >= Height))
			{
				cop.active  = false;
				randomState = mix(randomState + 0x9e3779b9u);
				cop.waitMs  = PoliceReturnMs + randomState % 2'001;
				continue;
			}
			const int separation = i == 2 ? carY - cop.y : cop.y - carY;
			if (!cop.courseCorrected && separation >= 0 && separation <= 60)
			{
				cop.targetX         = carX;
				cop.courseCorrected = true;
			}
			if (cop.courseCorrected)
			{
				cop.x += (cop.targetX > cop.x) - (cop.targetX < cop.x);
			}
			int left  = 0;
			int right = Width;
			for (int y = cop.y; y < cop.y + CarHeight; ++y)
			{
				const Bounds road = road_bounds(y);
				left              = road.left > left ? road.left : left;
				right             = road.right < right ? road.right : right;
			}
			contacts[i] = cop.y + CarHeight > 0 && cop.y < Height &&
			              (cop.x <= left + 1 || cop.x >= right - CarWidth - 1);
			cop.x = clamp(cop.x, left + 1, right - CarWidth - 1);
		}
		for (size_t i = 0; i < PoliceCount; ++i)
		{
			const Police &cop = police[i];
			if (!cop.active || cop.y + CarHeight <= 0 || cop.y >= Height)
			{
				continue;
			}
			playerHits[i] =
			  overlaps_car(carX, carY, cop.x, cop.y, CarWidth, CarHeight);
			contacts[i] = contacts[i] || playerHits[i];
			for (const auto &obstacle : obstacles)
			{
				if (obstacle.active && obstacle.kind != ObjectKind::MoneyBag &&
				    obstacle.kind != ObjectKind::GoldPile &&
				    overlaps_car(cop.x,
				                 cop.y,
				                 obstacle.x,
				                 obstacle.y,
				                 obstacle.width(),
				                 obstacle.height()))
				{
					contacts[i] = true;
				}
			}
			for (size_t j = i + 1; j < PoliceCount; ++j)
			{
				if (police[j].active &&
				    overlaps_car(cop.x,
				                 cop.y,
				                 police[j].x,
				                 police[j].y,
				                 CarWidth,
				                 CarHeight))
				{
					contacts[i] = contacts[j] = true;
				}
			}
		}
		uint8_t hits = 0;
		for (size_t i = 0; i < PoliceCount; ++i)
		{
			Police &cop = police[i];
			if (!cop.active)
			{
				continue;
			}
			if (contacts[i] && !cop.inContact)
			{
				randomState = mix(randomState + 0x9e3779b9u);
				if (randomState % 3 == 0)
				{
					cop.explosionMs = PoliceExplosionMs;
				}
			}
			cop.inContact = contacts[i];
			hits += playerHits[i];
			if (cop.explosionMs != 0 || playerHits[i])
			{
				cop.active  = false;
				randomState = mix(randomState + 0x9e3779b9u);
				cop.waitMs  = PoliceReturnMs + randomState % 2'001;
			}
		}
		return hits;
	}

	bool Game::police_nearby() const
	{
		if (phase != Phase::Playing)
		{
			return false;
		}
		for (size_t i = 0; i < police_limit(); ++i)
		{
			const int dy = police[i].y - carY;
			const int dx = police[i].x - carX;
			if (police[i].active && dy >= -84 && dy <= 84 && dx >= -RoadWidth &&
			    dx <= RoadWidth)
			{
				return true;
			}
		}
		return false;
	}

	uint32_t Game::wasted_color_level() const
	{
		if (phase != Phase::Wasted || phaseMs <= WastedHoldMs)
		{
			return 256;
		}
		const uint32_t elapsed = phaseMs - WastedHoldMs;
		const uint32_t linear =
		  elapsed < WastedFadeMs
		    ? (WastedFadeMs - elapsed) * 256 / WastedFadeMs
		    : 0;
		return (linear * linear) >> 8;
	}

	std::array<uint32_t, 2> Game::rgb_colors() const
	{
		if (protection.failed())
		{
			return {RgbWastedRed, RgbWastedRed};
		}
		if (phase == Phase::Wasted)
		{
			const uint32_t level = wasted_color_level();
			const uint32_t purple = 256 - level;
			const uint32_t red =
			  (((RgbWastedRed >> 16) * level +
			    (RgbWastedPurple >> 16) * purple) * level) >> 16;
			const uint32_t blue =
			  ((RgbWastedPurple & 0xff) * purple * level) >> 16;
			const uint32_t color = (red << 16) | blue;
			return {color, color};
		}
		if (phase != Phase::Playing)
		{
			return {};
		}
		std::array<uint32_t, 2> colors{
		  police_nearby() && (activeMs / 125) % 2 == 0 ? RgbPoliceBlue : 0,
		  0};
		if (damageFlashMs != 0)
		{
			colors[1] = RgbDamageOrange;
		}
		else if (pickupFlashMs != 0)
		{
			colors[1] = RgbPickupGreen;
		}
		else if (goldFlashMs != 0 &&
		         ((GoldFlashMs - goldFlashMs) / 250) % 2 == 0)
		{
			colors[1] = RgbPickupGold;
		}
		return colors;
	}

	int64_t Game::repair_cost() const
	{
		return damage * RepairPerPoint;
	}
} // namespace driving
