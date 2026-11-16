// SPDX-License-Identifier: MIT
#include "protection.hh"
#include "protection_api.hh"

#include <cheri.hh>
#include <errno.h>

namespace
{
	alignas(8) int64_t balance = 0;

	int change(int64_t amount, bool debit)
	{
		int64_t next;
		switch (driving::changed_balance(balance, amount, debit, next))
		{
			case driving::MoneyResult::Invalid:
				return -EINVAL;
			case driving::MoneyResult::Overflow:
				return -EOVERFLOW;
			case driving::MoneyResult::Ok:
				balance = next;
				return 0;
		}
		__builtin_unreachable();
	}
}

int wallet_reset()
{
	balance = 0;
	return 0;
}

int wallet_credit(int64_t amount)
{
	return change(amount, false);
}

int wallet_debit(int64_t amount)
{
	return change(amount, true);
}

const int64_t *wallet_view()
{
	CHERI::Capability<const int64_t> view{&balance};
	view.bounds() = sizeof(balance);
	view.permissions() &= CHERI::PermissionSet{CHERI::Permission::Load};
	return view.get();
}
