// SPDX-License-Identifier: MIT
#ifdef __CHERI_PURE_CAPABILITY__
#	error "The emulated protection backend must not be linked into CHERIoT firmware"
#endif

#include "game.hh"

namespace
{
	driving::FaultTotals totals{};
}

namespace driving
{
	FaultTotals protection_fault_totals()
	{
		return totals;
	}

	ProtectionOrigin Protection::origin()
	{
		return ProtectionOrigin::Emulated;
	}

	bool Protection::reset_wallet(int64_t &display)
	{
		if (failed())
		{
			return false;
		}
		emulatedBalance = 0;
		display = 0;
		return true;
	}

	bool Protection::read_wallet(int64_t &display)
	{
		if (failed())
		{
			return false;
		}
		display = emulatedBalance;
		return true;
	}

	bool Protection::credit(int64_t amount, int64_t &display)
	{
		if (failed())
		{
			return false;
		}
		int64_t next;
		const auto result = changed_balance(emulatedBalance, amount, false, next);
		if (result != MoneyResult::Ok)
		{
			return fail(ProtectionError::Wallet, static_cast<int32_t>(result));
		}
		emulatedBalance = next;
		return read_wallet(display);
	}

	bool Protection::debit(int64_t amount, int64_t &display)
	{
		if (failed())
		{
			return false;
		}
		int64_t next;
		const auto result = changed_balance(emulatedBalance, amount, true, next);
		if (result != MoneyResult::Ok)
		{
			return fail(ProtectionError::Wallet, static_cast<int32_t>(result));
		}
		emulatedBalance = next;
		return read_wallet(display);
	}

	bool Protection::police_attack()
	{
		if (failed())
		{
			return false;
		}
		++walletDenials;
		++totals.store;
		lastEvent = ProtectedEvent::WalletStore;
		return true;
	}

	bool Protection::damage_fault()
	{
		if (failed())
		{
			return false;
		}
		++tagDenials;
		++totals.tag;
		lastEvent = ProtectedEvent::DamageTag;
		return true;
	}

	RoadAccess Protection::check_road(int leftOffset, int rightOffset, size_t width)
	{
		if (failed())
		{
			return RoadAccess::Error;
		}
		if (width == 0 || width > RoadWidth ||
		    (leftOffset < 0 && rightOffset < 0) ||
		    leftOffset >= Width || rightOffset >= Width)
		{
			fail(ProtectionError::InvalidGrant);
			return RoadAccess::Error;
		}
		bool blocked = false;
		for (int offset : {rightOffset, leftOffset})
		{
			if (offset >= 0 && static_cast<size_t>(offset) >= width)
			{
				++roadDenials;
				++totals.bounds;
				lastEvent = ProtectedEvent::RoadBounds;
				blocked = true;
			}
		}
		return blocked ? RoadAccess::Blocked : RoadAccess::Allowed;
	}
} // namespace driving
