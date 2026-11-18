// SPDX-License-Identifier: MIT
//
// Deliberately under-privileged probe compartment. Treating capability faults
// as gameplay events follows lowRISC's CHERI Snake demo (sonata-software
// examples/snake/snake.cc). This implementation is independent of that code
// and uses the CHERIoT RTOS compartment error-handler interface.
#include "protection_api.hh"

namespace
{
	volatile uint32_t faultCount  = 0;
	volatile uint32_t faultCause  = 0;
	volatile uint32_t faultValue  = 0;
	void *volatile retained      = nullptr;

	void clear_fault()
	{
		faultCount = 0;
		faultCause = 0;
		faultValue = 0;
	}
}

extern "C" ErrorRecoveryBehaviour
compartment_error_handler(ErrorState *, size_t mcause, size_t mtval)
{
	faultCause = static_cast<uint32_t>(mcause);
	faultValue = static_cast<uint32_t>(mtval);
	faultCount = faultCount + 1;
	return ErrorRecoveryBehaviour::ForceUnwind;
}

int probe_store(void *object, size_t offset, uint8_t value)
{
	clear_fault();
	auto target = static_cast<volatile uint8_t *>(
	  __builtin_cheri_offset_increment(object, static_cast<ptrdiff_t>(offset)));
	*target = value;
	return 0;
}

int probe_retain(void *object)
{
	clear_fault();
	retained = nullptr;
	retained = object;
	const bool escaped = __builtin_cheri_tag_get(retained);
	retained = nullptr;
	return escaped ? 1 : 0;
}

uint64_t probe_fault_take()
{
	const uint32_t count = faultCount;
	const uint32_t cause = faultCause;
	const uint32_t value = faultValue;
	uint64_t result = 0;
	if (count != 0)
	{
		result = (static_cast<uint64_t>(count & 0x7fff) << 48) |
		         (static_cast<uint64_t>(cause & 0xffff) << 32) | value;
		if (count <= 0x7fff && cause <= 0xffff)
		{
			result |= uint64_t{1} << 63;
		}
	}
	clear_fault();
	return result;
}
