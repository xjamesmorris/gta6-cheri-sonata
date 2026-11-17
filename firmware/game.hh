// SPDX-License-Identifier: MIT
#pragma once

#include "protection.hh"

#include <array>
#include <stddef.h>
#include <stdint.h>

namespace driving
{
	constexpr int      Width          = 128;
	constexpr int      Height         = 160;
	constexpr int      PanelWidth     = Width;
	constexpr int      PanelHeight    = Height;
	constexpr size_t   FrameBytes     = Width * Height * 2;
	constexpr int      RoadWidth      = 72;
	constexpr int      RoadMargin     = 10;
	constexpr int      CarWidth       = 10;
	constexpr int      CarHeight      = 16;
	constexpr int      TrailerHeight  = 38;
	constexpr int      SceneryWidth   = 26;
	constexpr int      SceneryHeight  = 28;
	constexpr size_t   BillboardMessageCount = 6;
	constexpr int      FrogWidth      = 8;
	constexpr int      FrogHeight     = 7;
	constexpr uint32_t FrogHopMs      = 120;
	constexpr int      CarY           = Height * 2 / 3 - CarHeight / 2;
	constexpr int      MinCarY        = CarY - CarHeight * 2;
	constexpr int      MaxCarY        = CarY + CarHeight * 2;
	constexpr uint32_t TickMs         = 40;
	constexpr uint32_t FadeMs         = 6'000;
	constexpr uint32_t PromptDelayMs  = 3'000;
	constexpr uint32_t GraceMs        = 1'000;
	constexpr uint32_t WallGraceMs    = 500;
	constexpr uint8_t  WallDamage     = 1;
	constexpr uint8_t  ConeDamage     = 1;
	constexpr uint8_t  BarrierDamage  = 3;
	constexpr uint8_t  CarDamage      = 3;
	constexpr uint8_t  PoliceDamage   = 6;
	constexpr int64_t  RepairPerPoint = 500;
	constexpr uint32_t EscapeFlashMs  = 5'000;
	constexpr uint32_t RecoveryMs     = 10'000;
	constexpr int64_t  MoneyBagValue  = 1'000;
	constexpr int64_t  JackpotValue   = 100'000;
	constexpr int64_t  GoldValue      = 250'000;
	constexpr uint32_t GoldFlashMs    = 1'000;
	constexpr uint32_t PickupFlashMs  = 200;
	constexpr uint32_t DamageFlashMs  = 240;
	constexpr uint32_t WastedHoldMs   = 3'000;
	constexpr uint32_t WastedFadeMs   = 6'000;
	constexpr uint32_t RgbPoliceBlue  = 0x0000ff;
	constexpr uint32_t RgbWastedRed   = 0xff0000;
	constexpr uint32_t RgbWastedPurple = 0x8000ff;
	constexpr uint32_t RgbPickupGold  = 0xffad00;
	constexpr uint32_t RgbPickupGreen = 0x00ff00;
	constexpr uint32_t RgbDamageOrange = 0xff8000;
	constexpr size_t   PoliceCount    = 3;
	constexpr uint32_t PoliceDelayMs  = 1'200;
	constexpr uint32_t PoliceReturnMs = 3'000;
	constexpr uint32_t PoliceExplosionMs = 640;
	constexpr uint32_t PostFadeMs     = 1'500;
	constexpr int64_t  HospitalBill   = 25'000;
	constexpr size_t   BillItemCount  = 5;
	constexpr uint32_t BillFadeMs     = PostFadeMs;
	constexpr uint32_t BillLineMs     = 600;
	constexpr uint32_t BillReadyMs    = BillFadeMs + BillLineMs * BillItemCount;
	constexpr uint32_t SpawnSpacing   = 64;
	constexpr size_t   ObstacleCount  = 4;

	enum Input : uint8_t
	{
		Left  = 1,
		Right = 2,
		Up    = 4,
		Down  = 8,
		Press = 16
	};

	enum class Phase : uint8_t
	{
		FadeIn,
		Wallpaper,
		Prompt,
		Playing,
		Wasted,
		Billing,
		SourceQr
	};

	enum class ObjectKind : uint8_t
	{
		Cone,
		Barrier,
		Car,
		MoneyBag,
		GoldPile,
		Truck,
		Bus,
		Trailer
	};

	enum class SceneryKind : uint8_t
	{
		ZebraCrossing,
		Billboard,
		PoliceStation,
		GasStation
	};

	enum class StarAppearance : uint8_t
	{
		Off,
		Pending,
		Wanted,
		Expiring
	};

	struct Bounds
	{
		int left;
		int right;
	};

	struct Obstacle
	{
		int16_t    x      = 0;
		int16_t    y      = 0;
		ObjectKind kind   = ObjectKind::Cone;
		bool       active = false;
		int64_t    value  = MoneyBagValue;

		int  width() const;
		int  height() const;
		bool is_vehicle() const;
	};

	struct Scenery
	{
		int16_t     x      = 0;
		int16_t     y      = -SceneryHeight;
		SceneryKind kind   = SceneryKind::ZebraCrossing;
		bool        active = false;
		uint8_t     billboardMessage = 0;
	};

	struct Frog
	{
		int16_t  x         = 0;
		int16_t  y         = 0;
		int16_t  goalX     = 0;
		bool     active    = false;
		bool     rightward = true;
		uint32_t hopMs     = 0;
	};

	struct BillItem
	{
		const char *label  = nullptr;
		int64_t     amount = 0;
	};

	struct Police
	{
		int      x               = 0;
		int      y               = Height + 12;
		bool     active          = false;
		uint32_t waitMs          = PoliceDelayMs;
		bool     enabled         = false;
		int      targetX         = 0;
		bool     courseCorrected = false;
		bool     inContact       = false;
		uint32_t explosionMs     = 0;
	};

	struct Game
	{
		Phase                               phase       = Phase::FadeIn;
#ifdef DRIVING_ARTWORK_ALTERNATE
		bool                                alternateArtwork = true;
#else
		bool                                alternateArtwork = false;
#endif
		uint32_t                            phaseMs     = 0;
		uint32_t                            activeMs    = 0;
		uint32_t                            cleanMs     = 0;
		uint32_t                            graceMs     = 0;
		uint32_t                            wallGraceMs = 0;
		uint32_t                            distance    = 0;
		uint32_t                            revision    = 0;
		int                                 carX       = (Width - CarWidth) / 2;
		int                                 carY       = CarY;
		uint8_t                             stars      = 0;
		uint8_t                             carHits    = 0;
		uint8_t                             hitsNeeded = 2;
		uint32_t                            progressMs = 0;
		uint8_t                             damage     = 0;
		int64_t                             money      = 0;
		Protection                          protection{};
		int64_t                             lastPickup = 0;
		uint32_t                            pickupMs   = 0;
		bool                                pickedGold = false;
		uint32_t                            goldFlashMs = 0;
		uint32_t                            pickupFlashMs = 0;
		uint32_t                            damageFlashMs = 0;
		Scenery                             scenery{};
		Frog                                frog{};
		std::array<Police, PoliceCount>     police{};
		std::array<Obstacle, ObstacleCount> obstacles{};
		std::array<BillItem, BillItemCount> bill{};
		int64_t                             billOpeningMoney = 0;
		size_t                              billedItems      = 0;

		void           reset(uint32_t seed = 1);
		void           change_artwork();
		void           advance(uint32_t elapsedMs, uint8_t input);
		Bounds         road_bounds(int y) const;
		uint32_t       brightness() const;
		bool           stars_visible() const;
		bool           wanted_expiring() const;
		StarAppearance star_appearance(size_t index) const;
		size_t         police_limit() const;
		uint8_t        led_mask() const;
		uint32_t       wasted_color_level() const;
		std::array<uint32_t, 2> rgb_colors() const;
		bool           police_nearby() const;
		int64_t        repair_cost() const;
		bool           car_visible() const;
		int64_t        bill_total() const;
		int64_t        bill_item_shown(size_t index) const;
		int64_t        bill_shown_total() const;

		private:
		uint32_t baseSeed       = 1;
		uint32_t roadSeed       = 1;
		uint32_t randomState    = 1;
		uint32_t sceneryState   = 1;
		uint32_t frogState      = 1;
		uint32_t runs           = 0;
		uint32_t remainderMs    = 0;
		uint32_t nextSpawn      = 64;
		uint32_t nextScenery    = 800;
		uint32_t nextFrog       = 600;
		size_t   nextObstacle   = 0;
		bool     armed          = false;
		bool     fadingOut      = false;
		bool     returnToGame   = false;
		bool     waitForNeutral = false;
		uint32_t fadeOutMs      = 0;
		uint32_t splashFadeMs   = FadeMs;
		uint32_t playFadeMs     = 0;

		void    start_run(bool fade = false);
		void    reset_run_state(uint32_t seed);
		void    begin_bill();
		void    enter_wasted(uint8_t input);
		void    tick(uint8_t input);
		uint8_t update_police();
		bool    advance_explosions(uint32_t elapsedMs);
		void    record_car_hit();
		void    spawn_obstacle();
		void    update_scenery();
		void    update_frog();
		int     road_center(uint32_t worldY) const;
	};
} // namespace driving
