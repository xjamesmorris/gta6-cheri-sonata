# Architecture and verification boundaries

## Shared game, separate protection backends

`firmware/game.cc` owns deterministic gameplay, traffic, police, scoring,
receipts, transitions and LED state. `firmware/game_renderer.cc` renders
portrait RGB565 rows. Firmware, native tests and WebAssembly use this same
game and renderer. Splash data in `firmware/wallpaper.hh` is generated from
the two authorized, licensed PNGs. Primary/alternate selection supplies the
initial artwork; both images remain in read-only program memory. Runtime
selection changes a state flag and requires no extra framebuffer. It survives
run resets; the browser's `preview_init` models a fresh power reset.
In firmware, the images belong to compartment code and use read-only
capabilities; ELF section flags alone are not the capability permission model.
The existing writable renderer buffer is shared between screens.

The title accepts fresh Right for story, Down for artwork and hidden Left for
the source QR. The QR matrix is a reproducible 41-module, 246-byte packed
constant generated locally. Fresh input or a browser QR-canvas click returns
to the title without starting a run.

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
model a running target; Legacy and Halted are off. Fault brightness follows
127 PWM steps at 524161 clocks per step on a 40 MHz clock, about 1.66 seconds.
The mapping and envelope come from the pinned Sonata
[fault mapping](https://github.com/lowRISC/sonata-system/blob/e32eb27f638ccc3dbd86ff9b80ce21cac3828028/dv/verilator/top_verilator.sv)
and [PWM module](https://github.com/lowRISC/sonata-system/blob/e32eb27f638ccc3dbd86ff9b80ce21cac3828028/rtl/system/pwm_fade.sv).

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

`policy/gta6_wallpaper.rego` is enforced by `tools/check`. It requires the RTOS
policy plus exact device owners and bounds/permissions, a single 4096-byte
application stack with the configured trusted stack, and exact wallet/probe
exports and callers. Both protected compartments must have no imports.
External GPIO, pinmux and secondary UART access are forbidden.

`tools/check-protection-elf` additionally verifies probe-only handler placement
and minimum-stack metadata. Negative tests mutate grants, callers, exports,
stack sizes and ELF metadata to ensure rejected configurations remain rejected.

Every firmware boot runs real fault/recovery checks. The exhaustive simulator
scene tests all road widths, 1100 consecutive recovered tag faults, retention
of non-Global grants, successful-access and wrong-cause controls, stack/timing
budgets, and continued gameplay. `tools/check-protection-transcript` reconciles
exact totals with native scene expectations; it does not count repeated
simulator class announcements as a substitute for handler evidence.

Native tests and the browser exercise **emulation**. The RTL simulator executes
the capability firmware, but it does not verify physical joystick orientation,
electrical behavior or real-board LCD timing. Physical-board verification of
this CHERI gameplay remains a prerequisite for release acceptance.
