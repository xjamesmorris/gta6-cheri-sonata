// SPDX-License-Identifier: MIT
#ifndef __CHERI_PURE_CAPABILITY__
#	error "Architectural protection self-tests require CHERIoT"
#endif

#include "game.hh"
#include "protection_api.hh"

#include <cheri.hh>
#include <debug.hh>
#include <riscvreg.h>
#include <switcher.h>

namespace
{
	using namespace driving;
	using Debug = ConditionalDebug<true, "captest">;
	constexpr CHERI::PermissionSet ReadWrite{
	  CHERI::Permission::Load, CHERI::Permission::Store};

	bool require(bool condition, const char *message)
	{
		if (!condition)
		{
			Debug::log("FAIL: {}", message);
		}
		return condition;
	}

	void evidence(const char *name, const Protection &state)
	{
		const auto record = decode_fault(state.lastFault);
		Debug::log("{} ret={} valid={} count={} mcause={} mtval={} before={} after={}",
		           name, state.lastStatus, static_cast<unsigned>(record.valid), record.count,
		           record.mcause, record.mtval,
		           static_cast<int32_t>(state.lastBefore),
		           static_cast<int32_t>(state.lastAfter));
	}
}

namespace driving
{
	bool run_protection_selftest(bool exhaustive)
	{
		if (!require(Protection::origin() == ProtectionOrigin::Cheriot,
		             "not an architectural backend"))
		{
			return false;
		}
		Debug::log("BEGIN hardware exhaustive={}", static_cast<unsigned>(exhaustive));
		for (int64_t balance : {int64_t{0}, int64_t{1000}, int64_t{-1000}})
		{
			Protection test;
			int64_t display = 0;
			if (!require(test.reset_wallet(display), "wallet reset") ||
			    !require(balance >= 0 ? test.credit(balance, display)
			                          : test.debit(-balance, display),
			             "wallet seed") ||
			    !require(display == balance, "wallet seed mismatch") ||
			    !require(test.police_attack(), "wallet denial"))
			{
				return false;
			}
			evidence("wallet", test);
			if (!require(test.read_wallet(display) && display == balance,
			             "wallet changed"))
			{
				return false;
			}
		}

		Protection damage;
		if (!require(damage.damage_fault(), "tag denial"))
		{
			return false;
		}
		evidence("tag", damage);

		uint64_t maxRoadCycles = 0;
		const size_t firstWidth = exhaustive ? 1 : RoadWidth;
		for (size_t width = firstWidth; width <= RoadWidth; ++width)
		{
			Protection valid;
			const uint64_t start = rdcycle64();
			const auto access = valid.check_road(0, static_cast<int>(width - 1), width);
			const uint64_t cycles = rdcycle64() - start;
			maxRoadCycles = cycles > maxRoadCycles ? cycles : maxRoadCycles;
			if (!require(access == RoadAccess::Allowed && valid.lastStatus == 0 &&
			               valid.lastFault == 0,
			             "valid road access"))
			{
				return false;
			}
			for (int side = 0; side < 2; ++side)
			{
				Protection denied;
				const int outside = static_cast<int>(width);
				const auto result =
				  denied.check_road(side == 0 ? outside : 0,
				                    side == 0 ? 0 : outside, width);
				if (!require(result == RoadAccess::Blocked &&
				               denied.lastStatus == CompartmentFailure &&
				               denied.lastBefore == denied.lastAfter,
				             "road denial"))
				{
					return false;
				}
				if (width == RoadWidth)
				{
					evidence(side == 0 ? "road-left" : "road-right", denied);
				}
				const uint64_t fault = denied.lastFault;
				if (!require(denied.check_road(0, static_cast<int>(width - 1), width) ==
				               RoadAccess::Allowed &&
				               denied.lastFault == fault &&
				               denied.lastStatus == CompartmentFailure &&
				               denied.lastBefore == denied.lastAfter,
				             "allowed road access erased denial evidence"))
				{
					return false;
				}
			}
		}

		volatile uint8_t storage[16]{};
		storage[0] = 0x5a;
		CHERI::Capability<uint8_t> loan{const_cast<uint8_t *>(storage)};
		loan.bounds() = 8;
		loan.permissions() &= ReadWrite;
		if (!require(loan.is_valid(), "control grant"))
		{
			return false;
		}
		const uint8_t controlBefore = storage[0];
		int status = probe_store(loan.get(), 0, 0xa5);
		uint64_t record = probe_fault_take();
		if (!require(status == 0 && record == 0 && storage[0] == 0xa5 &&
		               classify_denial(status, record, TagCause, false) ==
		                 ProtectionError::UnexpectedSuccess,
		             "missing-fault checker control"))
		{
			return false;
		}
		Debug::log("control success-rejected ret={} record=0 before={} after={}",
		           status, static_cast<unsigned>(controlBefore),
		           static_cast<unsigned>(storage[0]));

		auto below = loan;
		below.address() = below.base() - 1;
		if (!require(!below.is_valid(), "below-base tag was not cleared"))
		{
			return false;
		}
		status = probe_store(below.get(), 0, 0x33);
		record = probe_fault_take();
		if (!require(classify_denial(status, record, BoundsCause, true) ==
		               ProtectionError::WrongFault &&
		               classify_denial(status, record, TagCause, storage[0] == 0xa5) ==
		                 ProtectionError::None &&
		               count_protection_fault(record),
		             "wrong-cause checker control"))
		{
			return false;
		}
		const auto wrong = decode_fault(record);
		Debug::log("control wrong-cause-rejected ret={} valid={} count={} mcause={} mtval={}",
		           status, static_cast<unsigned>(wrong.valid), wrong.count, wrong.mcause, wrong.mtval);

		status = probe_retain(loan.get());
		record = probe_fault_take();
		const auto retention = decode_fault(record);
		const bool trapped = classify_denial(
		  status, record,
		  static_cast<uint32_t>(CHERI::CauseCode::PermitStoreLocalCapabilityViolation),
		  storage[0] == 0xa5) == ProtectionError::None;
		const bool stripped = status == 0 && record == 0 && storage[0] == 0xa5;
		if (!require(trapped || stripped, "borrowed capability escaped"))
		{
			return false;
		}
		if (trapped && !require(count_protection_fault(record), "retention evidence"))
		{
			return false;
		}
		Debug::log("retention ret={} valid={} count={} mcause={} mtval={} trapped={}",
		           status, static_cast<unsigned>(retention.valid), retention.count, retention.mcause,
		           retention.mtval, static_cast<unsigned>(trapped));

		if (exhaustive)
		{
			Protection stress;
			for (unsigned i = 0; i < 1100; ++i)
			{
				if (!require(stress.damage_fault(), "repeated fault recovery"))
				{
					return false;
				}
			}
			if (!require(stress.tagDenials == 1100, "stress count"))
			{
				return false;
			}
			const auto last = decode_fault(stress.lastFault);
			Debug::log("stress faults={} valid={} count={} mcause={} mtval={}",
			           stress.tagDenials, static_cast<unsigned>(last.valid), last.count, last.mcause, last.mtval);
		}

		const auto top = CHERI::Capability{__builtin_cheri_stack_get()}.top();
		const auto lowest = stack_lowest_used_address();
		if (!require(lowest <= top && top - lowest < 4096,
		             "stack headroom exhausted") ||
		    !require(maxRoadCycles < uint64_t{CPU_TIMER_HZ} * TickMs / 1000 / 10,
		             "road probes exceed ten percent of a tick"))
		{
			return false;
		}
		Debug::log("budget road_cycles={} stack_bytes={}",
		           static_cast<uint32_t>(maxRoadCycles),
		           static_cast<uint32_t>(top - lowest));
		if (!require(wallet_reset() == 0, "final wallet reset"))
		{
			return false;
		}
		Debug::log("PASS hardware");
		log_protection_fault_totals();
		return true;
	}
} // namespace driving
