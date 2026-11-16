// SPDX-License-Identifier: MIT
#pragma once

#ifndef __CHERI_PURE_CAPABILITY__
#	error "This interface requires the CHERIoT capability ABI"
#endif

#include <compartment.h>
#include <stddef.h>
#include <stdint.h>

static_assert(__has_attribute(cheriot_minimum_stack),
              "Probe recovery requires compiler minimum-stack support");

extern "C"
{
	[[cheriot::interrupt_state(enabled)]]
	int __cheri_compartment("game_wallet") wallet_reset();
	[[cheriot::interrupt_state(enabled)]]
	int __cheri_compartment("game_wallet") wallet_credit(int64_t amount);
	[[cheriot::interrupt_state(enabled)]]
	int __cheri_compartment("game_wallet") wallet_debit(int64_t amount);
	[[cheriot::interrupt_state(enabled)]]
	const int64_t *__cheri_compartment("game_wallet") wallet_view();

	[[cheriot::interrupt_state(enabled)]]
	int __cheri_compartment("cap_probe") __cheriot_minimum_stack(512)
	  probe_store(void *object, size_t offset, uint8_t value);
	[[cheriot::interrupt_state(enabled)]]
	uint64_t __cheri_compartment("cap_probe") probe_fault_take();
	[[cheriot::interrupt_state(enabled)]]
	int __cheri_compartment("cap_probe") __cheriot_minimum_stack(512)
	  probe_retain(void *object);
}

namespace driving
{
	inline bool wallet_view_valid(const int64_t *pointer)
	{
		CHERI::Capability<const int64_t> view{pointer};
		constexpr CHERI::PermissionSet permissions{CHERI::Permission::Load};
		return view.is_valid() && !view.is_sealed() &&
		       view.address() == view.base() &&
		       __builtin_cheri_length_get(pointer) == sizeof(int64_t) &&
		       view.address() % alignof(int64_t) == 0 &&
		       view.permissions().as_raw() == permissions.as_raw();
	}
}
