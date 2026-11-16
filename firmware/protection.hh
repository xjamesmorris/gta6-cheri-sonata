// SPDX-License-Identifier: MIT
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace driving
{
	enum class ProtectionOrigin : uint8_t
	{
		Emulated,
		Cheriot
	};

	enum class ProtectionError : uint8_t
	{
		None,
		Wallet,
		InvalidGrant,
		Infrastructure,
		UnexpectedSuccess,
		WrongFault,
		DataChanged,
		BoundaryMismatch
	};

	enum class ProtectedEvent : uint8_t
	{
		None,
		DamageTag,
		WalletStore,
		RoadBounds
	};

	enum class RoadAccess : uint8_t
	{
		Allowed,
		Blocked,
		Error
	};

	struct FaultRecord
	{
		bool     valid;
		uint32_t count;
		uint32_t mcause;
		uint32_t mtval;
	};

	struct FaultTotals
	{
		uint32_t tag;
		uint32_t bounds;
		uint32_t store;
		uint32_t local;
	};

	FaultTotals protection_fault_totals();

	constexpr uint32_t CheriException = 28;
	constexpr uint32_t BoundsCause    = 0x01;
	constexpr uint32_t TagCause       = 0x02;
	constexpr uint32_t StoreCause     = 0x13;
	constexpr uint32_t StoreLocalCause = 0x16;
	constexpr int      CompartmentFailure = -1;
	constexpr int64_t  MinimumBalance = -INT64_MAX - int64_t{1};

	constexpr FaultRecord decode_fault(uint64_t word)
	{
		return {(word >> 63) != 0,
		        static_cast<uint32_t>((word >> 48) & 0x7fff),
		        static_cast<uint32_t>((word >> 32) & 0xffff),
		        static_cast<uint32_t>(word)};
	}

	constexpr ProtectionError classify_denial(int status,
	                                          uint64_t word,
	                                          uint32_t cause,
	                                          bool unchanged)
	{
		if (status == 0)
		{
			return ProtectionError::UnexpectedSuccess;
		}
		const auto record = decode_fault(word);
		if (status != CompartmentFailure || !record.valid || record.count != 1)
		{
			return ProtectionError::Infrastructure;
		}
		if (record.mcause != CheriException || (record.mtval & 0x1f) != cause ||
		    (record.mtval >> 5) == 0 || (record.mtval >> 5) > 15)
		{
			return ProtectionError::WrongFault;
		}
		return unchanged ? ProtectionError::None : ProtectionError::DataChanged;
	}

	enum class MoneyResult : uint8_t
	{
		Ok,
		Invalid,
		Overflow
	};

	constexpr MoneyResult changed_balance(int64_t current,
	                                      int64_t amount,
	                                      bool debit,
	                                      int64_t &next)
	{
		if (amount < 0)
		{
			return MoneyResult::Invalid;
		}
		if (debit ? current < MinimumBalance + amount : current > INT64_MAX - amount)
		{
			return MoneyResult::Overflow;
		}
		next = debit ? current - amount : current + amount;
		return MoneyResult::Ok;
	}

	struct Protection
	{
		uint32_t tagDenials    = 0;
		uint32_t walletDenials = 0;
		uint32_t roadDenials   = 0;
		ProtectedEvent lastEvent = ProtectedEvent::None;
		ProtectionError error   = ProtectionError::None;
		int32_t  errorDetail   = 0;
		uint64_t lastFault     = 0;
		uint64_t maxProbeCycles = 0;
		int32_t lastStatus = 0;
		int64_t lastBefore = 0;
		int64_t lastAfter = 0;

		static ProtectionOrigin origin();
		bool reset_wallet(int64_t &display);
		bool read_wallet(int64_t &display);
		bool credit(int64_t amount, int64_t &display);
		bool debit(int64_t amount, int64_t &display);
		bool police_attack();
		bool damage_fault();
		RoadAccess check_road(int leftOffset, int rightOffset, size_t width);
		bool failed() const { return error != ProtectionError::None; }

		bool fail(ProtectionError reason, int32_t detail = 0)
		{
			if (!failed())
			{
				error       = reason;
				errorDetail = detail;
			}
			return false;
		}

		private:
#ifndef __CHERI_PURE_CAPABILITY__
		int64_t emulatedBalance = 0;
#endif
	};

#ifdef __CHERI_PURE_CAPABILITY__
	bool run_protection_selftest(bool exhaustive);
	bool count_protection_fault(uint64_t record);
	void log_protection_fault_totals();
#endif
} // namespace driving
