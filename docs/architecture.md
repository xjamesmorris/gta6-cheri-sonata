# Architecture and verification boundaries

## Shared game, separate protection backends

`firmware/game.cc` owns deterministic gameplay, traffic, police, scoring,
receipts, transitions and LED state. `firmware/game_renderer.cc` renders
portrait RGB565 rows. Firmware, native tests and WebAssembly use this same
game and renderer. Splash data in `firmware/wallpaper.hh` is generated from
the two authorized CC BY 4.0 splash PNGs. `primary` / original is the default;
nearest-neighbor sampling recovers its native 128×160 grid from the supplied
512×640 enlargement. `alternate` uses a centered 1024×1280 crop of the
1024×1536 derivative and Lanczos resizing. Primary/alternate selection supplies
the initial artwork; both images remain in read-only program memory. Runtime
selection changes a state flag and requires no extra framebuffer. It survives
run resets; the browser's `preview_init` models a fresh power reset.
In firmware, the images belong to compartment code and use read-only
capabilities; ELF section flags alone are not the capability permission model.
The existing writable renderer buffer is shared between screens.

Only Right for story is displayed in the LCD title prompt. Hidden Down changes
artwork during FadeIn, Wallpaper and Prompt, using the existing fresh-input
arm flag. A held press cannot retrigger across phases; switching preserves the
phase and fade progress. Down remains a driving input during gameplay.
The browser artwork button also works during gameplay in v1.0.0 without
resetting progress.

Hidden Left at the title opens the source QR. Its reproducible 41-module,
246-byte packed constant is generated locally and encodes
`https://github.com/xjamesmorris/gta6-cheri-sonata`. Fresh input or a browser
QR-canvas click returns to the title without starting a run.

WASTED shares a quadratic remaining-color level between RGB565 desaturation
and red-to-purple LED dimming after a three-second hold. Level changes advance
the render revision so the physical LCD redraws throughout the six-second
fade. Billing and respawn use their normal colors.

The firmware adapter drives the LCD and joystick; the browser adapter maps
keyboard events, elapsed time and RGB565 pixels to a canvas. Browser/native
builds link `protection_emulated.cc`, not the capability implementation.
The browser explicitly labels protection **EMULATED** and rejects a backend
claiming otherwise.

Browser board indicators are emulated, not telemetry. All nine CHERI fault
classes are displayed; bounds (0), tag (1) and permit-store (5) track gameplay
denials, while unused classes remain dark. FPGA Config, Ibex Boot and CHERI
model a running target; Legacy and Halted are off. The last emulated gameplay
fault remains lit; the previous cause fades when it changes. This reflects
the observed FPGA revision's CSR-driven input, rather than assuming every
violation is only a one-cycle pulse. The browser does not model every CPU
trap/interrupt or replay startup self-tests.

At FPGA revision `1df06cdbe3954fb4`, the
[core decodes retained mcause/mtval](https://github.com/lowRISC/sonata-system/blob/1df06cdbe3954fb4/vendor/lowrisc_ibex/rtl/ibex_cs_registers.sv)
into the CHERI LED inputs; normal trap return does not clear them.
The [PWM filter](https://github.com/lowRISC/sonata-system/blob/1df06cdbe3954fb4/rtl/system/pwm_fade.sv)
is continually refreshed while that input stays high. A steady Tag LED can
therefore describe a recovered last fault, not a stream of new failures.
When the input drops, the fade is 127 steps of 524161 clocks (about 1.66 seconds
at the software's 40 MHz target). No privileged diagnostic state is cleared
merely to extinguish an indicator.

## Actual CHERIoT compartments

- **`game_wallet`** holds the authoritative balance. Its four entry points
  reset, credit, debit and return a strictly bounded, load-only view. Only
  the trusted game can call them.
- **`cap_probe`** has no imports or ambient allocation authority. Its three
  exports attempt a store, return the recorded fault, and test grant retention.
  It cannot import wallet mutation or device access.
- **`wallpaper`** drives the game and validates each grant and result. Police
  appearances attempt a forbidden write to the real wallet balance. Road
  movement probes an exactly bounded road object. Damage probes a tag-cleared
  car-part capability.
- **`lcd_panel`** owns LCD SPI and backlight access. **`debug`** owns the debug
  UART. Only `wallpaper` imports the onboard GPIO and RGB LED device regions.

Only `cap_probe` installs a minimal error handler. It records architectural
cause information and force-unwinds the failed compartment call. Trusted code
requires the expected failure, exactly one recorded fault and unchanged
protected bytes before accepting a denial. Wrong causes, missing faults,
unexpected success and infrastructure failures stop play with
**PROTECTION ERROR** and red RGB LEDs.

This demonstrates constrained authority and recoverable faults, not immunity
to arbitrary compromise of trusted game code. The game intentionally retains
legitimate wallet credit/debit authority. Balances are volatile game state,
not money, cryptographic attestation or persistence.

## Exact audit contract

`policy/gta6_wallpaper.rego` is enforced by `tools/check`. It is
deny-by-default. On top of the RTOS policy it requires:

- exactly the fifteen listed compartments and libraries;
- exactly the nine listed component-device MMIO grants anywhere in the image,
  with memory outside the board devices held only by the allocator;
- exact bounds and load/store-only permissions for the five application
  grants: LCD SPI, backlight, debug UART, onboard GPIO and RGB LEDs;
- in the application components `wallpaper`, `lcd_panel` and `debug`, only
  device, library and compartment-call imports, with every library function
  and compartment call listed by symbol;
- no imports of any kind in `game_wallet` and `cap_probe`;
- exact exports for `wallpaper`, `game_wallet`, `cap_probe` and `lcd_panel`,
  and exactly the listed calls into them from anywhere in the image;
- a single 4096-byte application stack with the configured trusted stack.

Adding a compartment or a board-device grant anywhere in the image, or any
import to an application component, therefore fails the audit until the
policy itself is changed. `tools/check` then names the rejecting rules.
External GPIO, pinmux and secondary UARTs also remain named as forbidden
devices.

The policy has limits. It does not pin the imports that RTOS components make
among themselves or the bounds of the allocator's heap; those come from the
pinned SDK, and the RTOS base policy checks only allocator quota objects and
which components hold the revoker and interrupt controllers. It does not
protect against bugs in trusted game code. Anyone able to change the sources
can also change the policy, so policy changes need the same review as code.

`tools/check-protection-elf` additionally verifies probe-only handler placement
and minimum-stack metadata. Negative tests mutate components, grants, import
kinds, callers, exports, stack sizes and ELF metadata to ensure rejected
configurations remain rejected. They include an extra compartment and every
board device granted to a component that is not listed for it.

Every firmware boot runs real fault/recovery checks. The exhaustive simulator
scene tests all road widths, 1100 consecutive recovered tag faults, retention
of non-Global grants, successful-access and wrong-cause controls, stack/timing
budgets, and continued gameplay. `tools/check-protection-transcript` reconciles
exact totals with native scene expectations; it does not count repeated
simulator class announcements as a substitute for handler evidence.

Native tests and the browser exercise **emulation**. The RTL simulator executes
the capability firmware, but it does not verify physical joystick orientation,
electrical behavior or real-board LCD timing. The owner authorized v1.0.0 with
these verification boundaries; release authorization does not claim exhaustive
interactive physical acceptance.

The hardware records below are **owner-attested summaries**. The console
transcripts behind them are private and are not published, so nobody else can
check them independently. What can be checked from public material is
narrower. The build, audit and RTL results reproduce from this tree with the
pinned toolchain. The v1.0.0 UF2 digest matches the published release asset.
From this change onward, the
[firmware workflow](../.github/workflows/firmware.yml) records build, audit
and RTL results for pushes to `main` and for pull requests, and keeps its
artifacts for 30 days. It has no results for earlier commits.

## Version 1.0.0 physical startup: November 17, 2026

The 381952-byte slot-2 UF2 image with SHA-256
`f3663309d5d06189e0e56485ef80e453a224165d80773ad3c9ed781c860e68a7`
was loaded through the guarded loader, selected in slot 2 and reached the
title screen. The version 1.0.0 public-tree build reproduced this UF2
byte-for-byte.
Its actual startup checks recovered seven expected faults: two tag, two
bounds and three store-permission faults. Wallet balances zero, positive and
negative, and the protected guard values, remained unchanged. Positive and
wrong-cause controls passed; the retention probe returned no usable tagged
capability without reporting a local-store exception.

The console reported `captest PASS hardware`, the CHERIoT hardware backend,
`Wallpaper ready` and title `Screen 2`. The fault totals were tag 2, bounds 2,
store 3 and local-store 0. The normal road check measured 11911 cycles and
1408 stack bytes. There was no unexpected failure or reset loop; the
programmer was left unmounted with no unfinished-write latch. No serial
commands were sent.

This is image-specific startup evidence, not an exhaustive physical stress run
or full interactive joystick acceptance. A rebuilt image must not inherit that
physical verification solely because its sources or rendered pixels match;
compare its exact UF2 digest with the recorded image.

## Post-release main startup: November 17, 2026

The post-release image with splash-only web artwork controls, varied billboard
messages and decorative frog crossings is 384000 bytes, SHA-256
`37abe6fb02a002eb225e02bbc161763f06a4a3ea9316eeddd7e5cc043b164124`.
The public-tree build at commit `4be894c` reproduced this UF2 byte-for-byte.
It was separately approved and loaded into application slot 2 through the
guarded loader. Fresh output reported selected slot 2, `PASS hardware`, the
CHERIoT hardware backend, `Wallpaper ready` and title `Screen 2`.

Startup totals were tag 2, bounds 2, store 3 and local-store 0. The normal road
check measured 11777 cycles and 1424 stack bytes. No unexpected failure or
reset loop appeared; the programmer was unmounted and the unfinished-load
latch absent. No serial commands were sent. This is startup evidence, not
exhaustive physical gameplay acceptance.

The frog and updated-billboard RTL fixtures each matched all 20,480 pixels
and 61,440 RGB channels against the native renderer. These code/data changes
do not grant additional MMIO or wallet authority. The frog is decorative and
non-colliding, and its independent random stream preserves traffic generation.
The archived version 1.0.0 image and release assets remain unchanged.

## Later phone controls and damage tuning

The retuned 384000-byte slot-2 build has SHA-256
`11969b65ae57d58b45ea629408912f5f718343698294900d8acc31544457723b`.
The firmware build/audit, native/sanitizer suites and 22 public browser tests
passed. Browser coverage uses Chromium touch emulation, not physical phones.

The `police-survival` RTL fixture checks a survivable five-star hit and a
second contact during collision grace: total recovered faults were tag 3,
bounds 2, store 3 and local-store 0, including startup. The `hospital` fixture
checks a full-damage collision, billing and restart; its totals were tag 15,
bounds 2, store 3 and local-store 0. Both matched all 20,480 LCD pixels and
61,440 RGB channels against the native renderer.

No additional hardware load was performed for this update. The board still
runs the recorded pre-tuning image; the new build requires its own approved
load and physical verification. Version 1.0.0 remains unchanged.

## Static review follow-up: November 18, 2026 (UTC)

Automated static review: **unlisted**. As with the design review, the
reviewer is not identified in this publication-bound record, and this is not
a human sign-off. The review read commit `be0163d` without building or
running it.

Its findings on notices for adapted code, the audit policy, the host tools
and CI led to Apache-2.0 headers and tracked third-party notices for the
files adapted from lowRISC, the deny-by-default audit policy, loader and
converter hardening, and the firmware workflow. Firmware behavior is
unchanged: the hardware build is still the 384000-byte image with SHA-256
`11969b65ae57d58b45ea629408912f5f718343698294900d8acc31544457723b`, and
`game.wasm` is byte-identical to the previous browser build.

Results on the development host with the pinned toolchain:

- `tools/check` passed for both artwork selections: warning-free build, audit
  result `true`, slot-2 UF2 validation, 27 Python tests and the native game
  tests. The audit rejected all 82 policy mutations; the previous policy
  accepted 30 of them, including an extra compartment holding an I2C device.
- `tools/check-browser` passed its sanitizer run and 22 browser tests.
- The exhaustive `cheri-selftest` RTL fixture passed with handler-verified
  totals of tag 1105, bounds 147, store 7 and local-store 0. It and the
  `wallpaper` fixture matched all 20,480 LCD pixels and 61,440 RGB channels.

Not verified:

- No hardware load was performed. The loader changes are covered only by
  tests with synthetic devices, so their first use on a board needs an
  attended load.
- The firmware workflow had not yet run on GitHub-hosted runners when this
  record was written.
