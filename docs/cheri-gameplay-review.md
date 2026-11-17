# CHERI gameplay design review and verification

This is the retained pre-release design/review record. Earlier checkpoint
results are historical, not claims about every later build. The current
release's exact image and physical evidence appear at the end and in
[architecture](architecture.md). Private development history, operator
identifiers and raw console archives are not included here.

Automated design feedback: **unlisted**. The original model identifier is
omitted from this publication-bound record. This is not a human sign-off or
independent human review; see the [LLM policy](../LLM-POLICY.md).

Verdict: **go with steering**. These were design corrections, not findings of
vulnerabilities in the then-current stable baseline.

| Concern | Accepted decision |
| --- | --- |
| Below-base road cursors lose their tag | Use mirrored, non-negative distances from the two kerbs; each edge violates the same exact upper bound. Cross-check arithmetic against actual fault outcomes. |
| Handler/frame limits | Keep one thread, 4096-byte stack and three trusted frames. Only the probe has a handler; use ForceUnwind, explicit minimum-stack attributes, runtime stack/cycle measurements and more than 1024 recovery calls. |
| Ambiguous 64-bit wallet failure returns and output pointers | Status-only mutators plus a validated, exactly bounded `{Load}` view. No wallet output pointers or in-band `int64_t` failure sentinel. |
| Reset/display-cache authority | Run-state reset never resets the wallet. Public reset does. Display snapshots are refreshed from the wallet, never used to restore it. Failed bill debits are not retried. |
| Fault evidence can be forged by the actor | Treat it as diagnostic. Enforce the real authority boundary with audit/capability checks and compare actual protected state. Probe writes must differ from existing bytes. Clear evidence on entry/take; keep the handler minimal. |
| Existing policy is insufficient for added components | Require both components, zero imports, exact enabled-interrupt export sets and exact caller/export pairs; pin stacks/threads and add negative mutations. Check handler linkage separately. |
| Origin labels would break pixel parity | Keep framebuffer wording origin-neutral. Expose emulation in browser DOM and architectural mode on UART. Hardware and emulated backend files have opposing compiler guards and no weak fallback. |

Implementation choices made while applying this steering:

- Test the road primitive for every width from one through `RoadWidth`; the
  observed normal terrain span is not treated as a universal lower bound.
- Add a minimal retention-test entry in the probe, with no extra authority, to
  measure non-Global capability escape behavior. It clears any stored value and
  is part of the exact export allowlist.
- A compact fault word must reject encoding overflow and the switcher's
  64-bit failure value, rather than truncate them into acceptable evidence.
- Preserve stable rollback artifacts before experimental builds. Main merges,
  publication and hardware loads require separate authorization.

Implementation acceptance required experimental verification of fault causes,
exact view permissions, zero-import linkage, minimum-stack/handler linkage,
recovery beyond 1024 calls, retention behavior, and timing/stack headroom. These
checks passed in the pinned RTL simulator. They did not substitute for a
separately approved physical-board load.

The first RTL experiment caught an unsupported `cgetsealed` instruction emitted
by the generic `__builtin_cheri_sealed_get`. The fix used the pinned SDK's
`Capability::is_sealed()` (`cgettype`) instead. The failed experiment stopped
before gameplay; no physical board was loaded with that build.

The pinned simulator (`sonata-system` revision
`e32eb27f638ccc3dbd86ff9b80ce21cac3828028`) intentionally prints only the first
occurrence of each CHERI error class per reset (`dv/verilator/top_verilator.sv`,
lines 168-192). Those lines are not per-event counts. Verification therefore
requires the exact expected class presence with no duplicates/unknown classes,
and separately reconciles fresh handler-validated totals against the fixed
self-test contract plus native gameplay expectations. It does not claim the
RTL diagnostic counts every trap.

## Historical implementation checkpoint

These results describe the original feature checkpoint, before the later
artwork and menu revisions. Rebuilding at the same output path does not make
an image equivalent to a recorded checkpoint; its digest is authoritative.

The implementation preserves the most recent denial's evidence across allowed
road probes and subsequent normal ticks. A protection failure ends the current
catch-up loop immediately, and a failed bill debit cannot be retried. The live
game and WASTED screen both display bounded `T/B/W` counters; an initial visual
check caught and corrected a missing live-game overlay.

| Check | Historical result |
| --- | --- |
| `tools/check` | Hardware build with no repository compiler warnings; exact compartment/MMIO policy; probe-only handler and 512-byte minimum-stack metadata; slot-2 UF2 verification; 15 Python tests and native game/rendering checks passed |
| Negative policy cases | All 32 mutations rejected, including 15 new wallet/probe authority, export, interrupt, component and stack mutations |
| `tools/check-browser` | Sanitized native checks and all 10 browser tests passed, including explicit emulation origin and native/Wasm state/frame agreement |
| `tools/sim-test cheri-selftest` | Both self-test suites passed, followed by real pickup, road/tag collision and police theft-denial gameplay; another normal tick preserved the denial record and wallet balance; all 20,480 LCD pixels matched |
| `tools/sim-test hospital` | Fatal collision, legitimate bill debits and retained negative balance after respawn passed; all 20,480 LCD pixels matched |
| `tools/sim-test wallpaper` | Startup and pristine splash matched all 20,480 pixels; rerun was unnecessary for the subsequent live-game-only overlay fix |
| Changed-shell-script lint | No new ShellCheck diagnostics; the historical baseline retained two SC2001 diagnostics in `tools/check` and one SC2016 in `tools/sim-test` |

The exhaustive run recovered from **1100 consecutive tag faults** before
continuing gameplay. Final handler-verified totals, including both startup
suites and gameplay, were **tag 1105, bounds 147, store 7, local-store 0**.
The native gameplay expectation was one tag, one bounds and one wallet denial.
RTL reported exactly the expected three first-occurrence classes.

Observed raw evidence was `mcause=0x1c`, with `mtval=0x142` for tag faults,
`0x141` for bounds faults, and `0x153` for wallet store-permission faults.
The retained-capability test returned no usable tagged capability and no trap;
it is not evidence of a local-capability-store exception. The exhaustive
self-test measured 12,078 cycles for the slowest allowed road check and
1776 stack bytes used within the configured 4096-byte stack. These are observed
test measurements, not a universal worst-case execution-time guarantee.

The local verification tools retain generated transcripts, expectations and
LCD captures under ignored `build/sim` and `build/sim-firmware` directories.
Raw operator archives are not part of this public repository.

The unflashed feature UF2 at that checkpoint was 297472 bytes:

```text
SHA-256 46ce8a130114d6ad2add79c236ee027eb954d7fd87a2eafcb2b99ea6653a5fe9
```

The stable rollback UF2 at that time had SHA-256
`f82574956698c6f69495f1cc7311ebe20412eb192d4158c493a1ea8d42e687a0`.
No feature firmware had been flashed, and no feature branch had been pushed
or merged at that checkpoint.

## Physical startup check: November 16, 2026

The guarded slot-2 load of the 383488-byte image with SHA-256
`a1eae22da914d9a89e19ac05950543cba11a6c960d2ce8d94b293f6045c26f2d`
completed cleanly and reached the title screen. The actual startup suite
reported `PASS hardware`: tag 2, bounds 2, store 3, local-store 0.
The tested wallet/guard values stayed unchanged; successful-access,
wrong-cause and retention controls passed. Normal road checks measured 11813
cycles and 1408 stack bytes. No unexpected failure or reset loop appeared.

This was normal startup recovery, not an exhaustive physical stress run or
full interactive acceptance. The subsequent native-artwork image is separate.

## Physical release startup: November 17, 2026

The native-`orig` slot-2 image is 381952 bytes with SHA-256
`f3663309d5d06189e0e56485ef80e453a224165d80773ad3c9ed781c860e68a7`.
The clean public-tree build reproduced this UF2 byte-for-byte.
The guarded loader validated an immutable snapshot, wrote only application
slot 2, and unmounted the programming drive cleanly. The FPGA bitstream
storage and other application slots were not written.

A fresh boot transcript reported selected software slot 2, `PASS hardware`,
the CHERIoT hardware backend, `Wallpaper ready` and title `Screen 2`.
Startup totals were tag 2, bounds 2, store 3, local-store 0.
The normal road check measured 11911 cycles and 1408 stack bytes. No unexpected
failure or reset loop appeared, and no unfinished-load latch remained.
No serial commands were sent.

This is image-specific physical startup evidence, not an exhaustive physical
stress run or a claim that every joystick interaction was observed on the
board. Native/browser tests and exact RTL LCD comparisons cover the `orig`
default and hidden Down artwork navigation. The owner authorized the 1.0.0
release; post-release gameplay enhancements remain separate.
