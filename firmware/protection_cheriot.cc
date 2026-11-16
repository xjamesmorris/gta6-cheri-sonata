// SPDX-License-Identifier: MIT
#ifndef __CHERI_PURE_CAPABILITY__
#	error "The real protection backend requires CHERIoT"
#endif

#include "game.hh"
#include "protection_api.hh"

#include <cheri.hh>
#include <errno.h>
#include <debug.hh>
#include <riscvreg.h>

namespace
{
	using namespace driving;
	using namespace CHERI;

	static_assert(static_cast<uint32_t>(CauseCode::BoundsViolation) == BoundsCause);
	static_assert(static_cast<uint32_t>(CauseCode::TagViolation) == TagCause);
	static_assert(static_cast<uint32_t>(CauseCode::PermitStoreViolation) == StoreCause);
	static_assert(static_cast<uint32_t>(CauseCode::PermitStoreLocalCapabilityViolation) ==
	              StoreLocalCause);
	static_assert(-ECOMPARTMENTFAIL == CompartmentFailure);

	constexpr PermissionSet ReadWrite{Permission::Load, Permission::Store};
	uint32_t verifiedTag = 0;
	uint32_t verifiedBounds = 0;
	uint32_t verifiedStore = 0;
	uint32_t verifiedLocal = 0;

	bool accept(Protection &state, int status, uint64_t record,
	            uint32_t cause, bool unchanged, ProtectedEvent event)
	{
		state.lastFault = record;
		const auto error = classify_denial(status, record, cause, unchanged);
		if (error != ProtectionError::None)
		{
			return state.fail(error, status);
		}
		if (!count_protection_fault(record))
		{
			return state.fail(ProtectionError::WrongFault, status);
		}
		state.lastEvent = event;
		return true;
	}

	int timed_store(Protection &state, void *pointer, size_t offset, uint8_t value)
	{
		const uint64_t start = rdcycle64();
		const int status = probe_store(pointer, offset, value);
		const uint64_t elapsed = rdcycle64() - start;
		if (elapsed > state.maxProbeCycles)
		{
			state.maxProbeCycles = elapsed;
		}
		return status;
	}
}

namespace driving
{
	FaultTotals protection_fault_totals()
	{
		return {verifiedTag, verifiedBounds, verifiedStore, verifiedLocal};
	}

	bool count_protection_fault(uint64_t word)
	{
		const auto record = decode_fault(word);
		if (!record.valid || record.count != 1 || record.mcause != CheriException)
		{
			return false;
		}
		switch (record.mtval & 0x1f)
		{
			case TagCause: ++verifiedTag; break;
			case BoundsCause: ++verifiedBounds; break;
			case StoreCause: ++verifiedStore; break;
			case StoreLocalCause: ++verifiedLocal; break;
			default: return false;
		}
		return true;
	}

	void log_protection_fault_totals()
	{
		using Debug = ConditionalDebug<true, "captest">;
		Debug::log("TOTAL tag={} bounds={} store={} local={}",
		           verifiedTag, verifiedBounds, verifiedStore, verifiedLocal);
	}

	ProtectionOrigin Protection::origin()
	{
		return ProtectionOrigin::Cheriot;
	}

	bool Protection::reset_wallet(int64_t &display)
	{
		if (failed())
		{
			return false;
		}
		const int status = wallet_reset();
		return status == 0 ? read_wallet(display)
		                   : fail(ProtectionError::Wallet, status);
	}

	bool Protection::read_wallet(int64_t &display)
	{
		if (failed())
		{
			return false;
		}
		const int64_t *view = wallet_view();
		if (!wallet_view_valid(view))
		{
			return fail(ProtectionError::InvalidGrant);
		}
		display = *static_cast<const volatile int64_t *>(view);
		return true;
	}

	bool Protection::credit(int64_t amount, int64_t &display)
	{
		if (failed())
		{
			return false;
		}
		const int status = wallet_credit(amount);
		return status == 0 ? read_wallet(display)
		                   : fail(ProtectionError::Wallet, status);
	}

	bool Protection::debit(int64_t amount, int64_t &display)
	{
		if (failed())
		{
			return false;
		}
		const int status = wallet_debit(amount);
		return status == 0 ? read_wallet(display)
		                   : fail(ProtectionError::Wallet, status);
	}

	bool Protection::police_attack()
	{
		if (failed())
		{
			return false;
		}
		const int64_t *view = wallet_view();
		if (!wallet_view_valid(view))
		{
			return fail(ProtectionError::InvalidGrant);
		}
		const auto observed = static_cast<const volatile int64_t *>(view);
		const int64_t before = *observed;
		lastBefore = before;
		const uint8_t value =
		  static_cast<uint8_t>(static_cast<uint64_t>(before)) ^ uint8_t{1};
		const int status = timed_store(*this, const_cast<int64_t *>(view), 0, value);
		lastStatus = status;
		const uint64_t record = probe_fault_take();
		if (!wallet_view_valid(view))
		{
			return fail(ProtectionError::InvalidGrant);
		}
		lastAfter = *observed;
		if (!accept(*this, status, record, StoreCause, lastAfter == before,
		            ProtectedEvent::WalletStore))
		{
			return false;
		}
		++walletDenials;
		return true;
	}

	bool Protection::damage_fault()
	{
		if (failed())
		{
			return false;
		}
		volatile uint8_t part = 0x5a;
		Capability<uint8_t> owner{const_cast<uint8_t *>(&part)};
		owner.bounds() = sizeof(part);
		owner.permissions() &= ReadWrite;
		if (!owner.is_valid())
		{
			return fail(ProtectionError::InvalidGrant);
		}
		void *broken = __builtin_cheri_tag_clear(owner.get());
		lastBefore = part;
		const int status = timed_store(*this, broken, 0, 0xa5);
		lastStatus = status;
		const uint64_t record = probe_fault_take();
		lastAfter = part;
		if (!accept(*this, status, record, TagCause, lastAfter == lastBefore,
		            ProtectedEvent::DamageTag))
		{
			return false;
		}
		++tagDenials;
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
		alignas(16) volatile uint8_t storage[Width + 16];
		Capability<uint8_t> lane{const_cast<uint8_t *>(storage + 8)};
		lane.bounds() = width;
		lane.permissions() &= ReadWrite;
		if (!lane.is_valid() || lane.address() != lane.base() ||
		    __builtin_cheri_length_get(lane.get()) != width ||
		    lane.permissions().as_raw() != ReadWrite.as_raw())
		{
			fail(ProtectionError::InvalidGrant);
			return RoadAccess::Error;
		}
		bool blocked = false;
		for (int offset : {rightOffset, leftOffset})
		{
			if (offset < 0)
			{
				continue;
			}
			for (auto &byte : storage)
			{
				byte = 0x5a;
			}
			const bool outside = static_cast<size_t>(offset) >= width;
			const uint8_t before = storage[8 + static_cast<size_t>(offset)];
			const int status = timed_store(*this, lane.get(),
			                               static_cast<size_t>(offset), 0xa5);
			const uint64_t record = probe_fault_take();
			const uint8_t after = storage[8 + static_cast<size_t>(offset)];
			bool intact = true;
			for (size_t i = 0; i < sizeof(storage); ++i)
			{
				const uint8_t expected =
				  !outside && i == 8 + static_cast<size_t>(offset) ? 0xa5 : 0x5a;
				intact &= storage[i] == expected;
			}
			const bool allowed = status == 0 && record == 0 && intact;
			// Later allowed probes must not erase the last denial's evidence.
			if (outside || !allowed)
			{
				lastStatus = status;
				lastFault = record;
				lastBefore = before;
				lastAfter = after;
			}
			if (outside)
			{
				if (!accept(*this, status, record, BoundsCause, intact,
				            ProtectedEvent::RoadBounds))
				{
					return RoadAccess::Error;
				}
				++roadDenials;
				blocked = true;
			}
			else if (!allowed)
			{
				fail(intact ? ProtectionError::Infrastructure
				            : ProtectionError::DataChanged, status);
				return RoadAccess::Error;
			}
		}
		return blocked ? RoadAccess::Blocked : RoadAccess::Allowed;
	}
} // namespace driving
