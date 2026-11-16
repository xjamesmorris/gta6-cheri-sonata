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
electrical behavior or real-board LCD timing. Interactive physical acceptance
remains a prerequisite for release.

## Physical startup check: November 16, 2026

The 383488-byte slot-2 image with SHA-256
`a1eae22da914d9a89e19ac05950543cba11a6c960d2ce8d94b293f6045c26f2d`
was loaded through the guarded loader and reached the title screen.
Its actual startup checks recovered seven expected faults: two tag, two
bounds and three store-permission faults. Wallet balances zero, positive and
negative, and the protected guard values, remained unchanged. Positive and
wrong-cause controls passed; the retention probe returned no usable tagged
capability without reporting a local-store exception.

The console reported `PASS hardware`, the CHERIoT hardware backend and title
readiness with no unexpected failure. The normal road check measured 11813
cycles and 1408 stack bytes. This is image-specific startup evidence, not an
exhaustive physical stress run or full interactive acceptance. Subsequent
smooth-artwork/demo-removal builds are separate artifacts until explicitly
loaded and checked.
