# CHERI-backed gameplay plan

This pre-release design plan is retained as a development record. The
implementation has been verified on native/Wasm and the Sonata RTL simulator;
the separately approved physical startup results are recorded in the
[design review](cheri-gameplay-review.md). Current build and loading
instructions are in the [README](../README.md) and
[board-loading guide](board-loading.md).

The original plan kept experimental feature builds separate from the stable
board image. Later loads, merges and publication required separate owner
approval. This clean repository does not include that private development
history or operator-specific records.

## Goal and limits

Use genuine CHERIoT capability exceptions as part of the game, recover from the
expected faults, and deny the attempted operation without terminating the game.
This is a capability-security demonstration, not a real cryptocurrency wallet,
encryption scheme, remote attestation service, or general-purpose sandbox.

The browser/native builds must remain usable but explicitly identify their
protection behavior as **emulated**. Only the CHERIoT firmware and Sonata RTL
simulator may claim architectural capability faults. No silent fallback from
hardware protection to emulation is permitted.

Existing traffic, money values, billing, steering, LEDs, artwork, controls and
slot-2 safety remain intact. Audio remains an options document
([audio-options.md](audio-options.md)), not a new peripheral or driver in this
feature.

## Threat model

The game/controller, wallet implementation, compiler, loader and CHERIoT RTOS
are trusted. An attacker/probe compartment is deliberately given less authority
and attempts forbidden accesses. It must not receive a wallet write capability,
a wallet mutation entry point, the game object, peripheral authority, or a
broader parent capability from which it could derive one.

The intended claims are spatial/permission isolation and recovery from the
specific scripted attacks. A malicious trusted game could misuse its legitimate
wallet authority. Arbitrary nonterminating code in a synchronous callee is also
outside the availability claim; the probe routines are short, audited and
bounded. Diagnostic fault records are not cryptographically unforgeable
attestations from an arbitrary compromised component.

No fault recovery is described as transaction rollback. A probe must not make
sensitive changes, acquire a lock, allocate, perform I/O, or call another
compartment before its forbidden access.

## Components and authority

| Component | State and authority |
| --- | --- |
| `wallpaper` | Trusted game, input, renderer and RGB adapter; existing onboard MMIO only. Holds the legitimate wallet-management entry points. |
| `game_wallet` | Sole authoritative signed 64-bit game-currency balance. No MMIO, debug UART, allocator authority or access to the game object. |
| `cap_probe` | Short forbidden-access routines and a minimal compartment error handler. Receives only a narrowly bounded borrowed data capability for each call. No wallet-management entry points or MMIO. |
| Existing LCD/debug components | Keep their current ownership and interfaces. |

`cap_probe` must be a real **compartment**, not a shared library executing in
the game's protection domain.

Keep one application thread, the existing 4096-byte stack and three trusted
stack frames. Call depth stays two; the handler runs on the faulting frame and
does not consume another trusted-stack frame. It needs 128 bytes for
`ErrorState` plus its own stack use. Put an explicit minimum-stack requirement
on faulting probe entries and measure headroom. Do not broaden peripheral
permissions or change the SDK/bitstream to make a failing test pass.

## Protected wallet

The real balance lives in private `game_wallet` storage, not in the attacker or
the renderer. Proposed C interfaces, callable only by `wallpaper`, are:

```c
int wallet_reset(void);
int wallet_credit(int64_t amount);
int wallet_debit(int64_t amount);
const int64_t *wallet_view(void);
```

Mutation amounts are nonnegative; arithmetic is checked before changing the
balance. Legitimate bills may make the balance negative. Mutators return zero
or a negative errno; there are no caller-supplied output pointers. Do not return
an `int64_t` balance across the compartment boundary: a forced unwind sets
`a0=-1, a1=0`, which can look like the valid balance 4294967295.

The view points to the **actual balance**, with bounds exactly covering that
object and hardware permissions exactly `{Load}`, without Global. Before every
load through it, the caller checks its tag, unsealed state, base/cursor,
alignment, length eight and exact permissions. C++ `const` alone is not a
security boundary. Do not export a writable parent pointer or a
capability-bearing aggregate.

A small wallet facade preserves native tests' per-game value semantics and
forwards to these entry points on CHERIoT. `Game.money` may remain a display
snapshot for compatibility, but must never be authoritative: every credit,
debit and run-transition balance comes from the wallet. Test that corrupting
the display snapshot cannot mint money or alter a resumed balance.

Separate run-state reset from wallet reset. Boot/new game/Left reset clears the
wallet; starting a run and Right respawn preserve the authoritative balance.
Constructors and assignment have no wallet side effects. Do not restore the
wallet from an untrusted or stale display snapshot. Advance the bill-line index
even when a debit fails, then latch the error; never retry that charge.

At each actual police appearance, borrow the read-only view and attempt to
change a byte of the actual balance in `cap_probe`, representing attempted
theft/tampering. Choose a value different from the current byte, including when
the balance is zero. The store must generate a
`PermitStoreViolation`, return a denied result, and leave the wallet unchanged.
An unexpected write must cause a visible protection failure, not an automatic
restoration followed by a false "denied" message.

## Road and damage objects

### Road boundary

Represent the legal horizontal span under the entire car footprint with a real
bounded byte object. The trusted game computes the intersection of the road
bounds across the car's height, as it does today.

Prefer a caller-owned local buffer no larger than `RoadWidth`, narrowed with
`CHERI::Capability::bounds()` to the exact legal span. The SDK setter uses
`__builtin_cheri_bounds_set_exact`; check the resulting tag, base and length.
Never silently round the grant outward.

CHERIoT clears the tag for below-base addresses. Therefore represent the two
edges with non-negative distances from opposite kerbs:

```text
rightOffset = carX + CarWidth - 1 - roadLeft
leftOffset  = roadRight - 1 - carX
```

For each non-negative offset, perform a real byte-store probe; if one offset
is negative, the other necessarily breaches the width. Each edge breaches
exactly when its offset is at least the granted width. Let the actual probe
outcomes determine `wallContact`, retaining the old arithmetic predicate only
as a cross-check. A disagreement is an error and still blocks movement.

Use CHERI cursor intrinsics, not undefined C++ pointer arithmetic. Positive
out-of-bounds cursors must remain tagged and produce `BoundsViolation`, not the
damage demonstration's tag fault. Allocate enough backing/canary bytes for all
candidate offsets, lend only the legal span, and use a differing store value.
Compare bytes before each probe: an earlier allowed edge write is not a
transactional road-position commit. Normal spans are around 63-72 bytes, but
the primitive should validate and test every positive width up to `RoadWidth`
rather than assume one sampled terrain range.

The grant is borrowed for one synchronous invocation. Remove Global and
capability-load/store authority where appropriate, so the probe cannot retain
an old, wider grant in global storage. Keep canaries outside the granted region.
Validate both sides of the boundary and exact representability for all
supported spans; a wrong fault type is an error, not a reason to relabel it.

### Damage/tag demonstration

For a valid damage event, make a tag-cleared copy of a narrowly bounded
car-part/guard capability and attempt a byte access through that copy in
`cap_probe`. Require a genuine `TagViolation` before reporting the demonstration
as successful. The owner retains the valid capability; do not corrupt an
unrelated pointer or the real game's control state.

The damage meter is still game policy. Hardware tags are binary validity
metadata, not percentages of health. Collision grace must not generate repeated
damage/tag events for the same suppressed impact.

## Recovery protocol

The pinned SDK already supplies:

- `compartment_error_handler(ErrorState *, size_t mcause, size_t mtval)`;
- `ErrorRecoveryBehaviour::ForceUnwind`;
- `CHERI::extract_cheri_mtval`, including tag, bounds and store-permission causes;
- `-ECOMPARTMENTFAIL` on a failed compartment call.

The vendored crash-recovery tests demonstrate that a caller can continue
normally after the callee force-unwinds; it need not install a handler merely
to receive the failure.

The probe's handler will record raw `mcause`, `mtval` and an invocation-local
count, then return `ForceUnwind`. It will not
print, allocate, lock, retry the access, invoke application callbacks, or modify
saved registers/PCC. In particular, it will not blindly skip instructions.
It will not dereference `ErrorState` or decode enums in the handler.
`ErrorState.pcc` is untagged metadata, not authority to execute or read code.

Clear the record at probe entry and when it is consumed. The proposed probe
interfaces are:

```c
int probe_store(void *object, size_t offset, uint8_t value);
uint64_t probe_fault_take(void);
int probe_retain(void *object); /* only used by the isolation self-test */
```

The compact fault word carries valid/count/mcause/mtval. Reserve a high valid
bit so the forced-unwind 64-bit return (`0x00000000ffffffff`) cannot be mistaken
for evidence. Reject any value that cannot be encoded without truncating a
valid record. Decode and classify outside the handler.

`probe_retain` tests the borrowed-capability lifetime claim by attempting to
store a non-Global capability in probe globals, then clearing any stored value.
Record whether the target traps or strips its tag; either must prevent a valid
retained capability. This narrowly scoped self-test entry gets no additional
authority and must be included in the exact export allowlist.

After the call, trusted code checks:

1. The call returned `-ECOMPARTMENTFAIL`, not ordinary success.
2. A fresh, valid handler record exists with exactly one fault.
3. The architectural cause is CHERI and the decoded cause is the expected kind.
4. The caller validated the lent capability's metadata and intended restriction.
5. Wallet contents or surrounding canaries remain unchanged.

Allowed road-access controls must succeed with no fault record. Missing,
unexpected or unrecoverable faults must enter an explicit protection-error
state and report diagnostics outside the handler; they must not become a
success-shaped denial. The error path must not retry a side effect.

Keep the unsafe routines stateless and free of RAII resources. Forced unwind
does not promise to run arbitrary C++ destructors in the failed compartment.
`wallpaper` and `game_wallet` install no handler and do not use
`CHERIOT_DURING`. The switcher counter is per trusted-stack frame, zeroed on
entry and discarded by ForceUnwind; no faults may accumulate in the permanent
game-entry frame. Stack-shortage returns (-140/-141), a missing record or an
unexpected cause are infrastructure/protection errors, not denials.

## Presentation and emulation

Keep bounded per-kind denial counters and a small, nonintrusive "CAP BLOCKED"
indication or equivalent diagnostic. Do not replace the existing HUD or change
pickup/damage LED meanings. Police theft denial does not reduce the score or
itself damage the player.

The native/Wasm backend emulates permission/bounds/tag outcomes with ordinary
safe code and per-game wallet state. It must never execute invalid host
pointers to imitate CHERI. Export the backend origin to the browser and display
an explicit emulation label. Hardware-only source files must fail compilation
without the CHERIoT capability ABI, and hardware builds must not link the
emulated backend. Both backends define the same strong symbols with opposing
compiler-ABI guards. Add the emulated implementation to every native/Wasm build
command, never to `xmake.lua`. The backend-origin label belongs in browser DOM
and UART, not in the framebuffer, so RTL/native pixel comparisons stay exact.

## Audit and verification

Extend the current policy rather than weakening it:

- Only `wallpaper` may call wallet-management or probe entry points.
- Both new compartments must exist and have no imports of any kind.
- Pin their exact exported symbols, all with interrupts explicitly enabled.
- Pin the exact [importer, export-symbol] pairs for those entry points.
- Existing LCD/UART/onboard GPIO/RGB ownership remains exact.
- External GPIO, pinmux and secondary UARTs remain forbidden.
- Pin thread count, stack size and trusted-stack length.
- Negative policy tests inject a probe-to-wallet call, debug/LCD calls to either
  new compartment, an unlisted MMIO device, an extra export and a second thread.
- Check ELF symbols/sections: the handler exists in `cap_probe` only. Do not
  rely on the audit report to list it or on a missing-handler warning.

| Test | Required evidence |
| --- | --- |
| Wallet attack with positive, zero and negative balances | Real store-permission fault, failed call, fresh cause record, exact unchanged balance |
| Valid/invalid road positions, both edges and varied spans | Allowed access inside; real bounds fault outside; untouched canaries |
| Tag-cleared car-part access | Real tag fault; guard data unchanged |
| Writable/valid control access | Success, expected write, no manufactured denial |
| Missing or wrong fault | Explicit failure, not an accepted block |
| Repeated recovery | More than 1024 separate faulting calls, followed by a normal matching game scene |
| Wallet arithmetic and reset/respawn | Checked overflow, legitimate billing, retained Right balance and zeroed Left balance |
| Native/Wasm | Safe emulation, explicit origin label, matching game state/rendering |
| Firmware/RTL | Actual CHERIoT handler evidence plus existing build, audit, UF2 and representative LCD checks |

Use a simulator-only isolation scene that prints a structured transcript outside
the handler: raw return, valid/count, raw cause, before/after data and result.
Include positive controls, wrong-cause and unexpected-success checker controls,
the retention experiment, repeated recovery, and cycle/stack measurements.
A missing record is also tested natively on the classifier.

The normal simulator error checks must continue to reject unexpected faults.
Do not globally ignore capability-error output or count a printed "denied"
string as proof. The pinned RTL monitor logs first occurrences per class, not
each trap. Require exact class presence (no unknown/duplicate classes), plus
exact handler-validated event totals matching the self-test contract and native
gameplay expectations. Do not mislabel class-presence output as event counts.

Keep the production slot-2 image separate from simulator fixtures. Do not
automatically replace the working board image with an unverified feature
build, merge the branch, change a public visibility setting, or create a PR.

## References and review questions

- [Pinned Sonata software](https://github.com/lowRISC/sonata-software/tree/722cdea0207920997b0895ea1f430619b343e10c)
- [CHERIoT compartment error handling](https://cheriot.org/book/compartments.html)
- [Improved error handling in CHERIoT RTOS](https://cheriot.org/rtos/errors/2024/09/20/error-handling.html)
- Verified pinned source: `sdk/include/compartment.h`, `cheri.hh`, `unwind.h`,
  and `tests/crash_recovery_inner.cc` / `crash_recovery_outer.cc`.

The pre-implementation review focused on the wallet facade/display-snapshot
boundary, borrowed road grants and lifetime, error-record trust assumptions,
entry-point allowlists, frame/stack budget, and the distinction between
architectural evidence and emulation. The accepted steering and measured
results are preserved in the [review record](cheri-gameplay-review.md).
