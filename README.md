# Grand Time Auto: CHERI on Sonata

A small portrait driving game for the lowRISC Sonata v1.3 FPGA board, with a
board-free WebAssembly preview. The repository name is `gta6-cheri-sonata`.
The shared C++ game and RGB565 renderer target a **128×160** ST7735 LCD.

**Development status:** this is not a 1.0 release. Real CHERIoT protection,
fault recovery and simulator checks are implemented. The recorded November 16,
2026 firmware image passed its physical-board startup fault tests and reached
the title screen; see [verification boundaries](docs/architecture.md).
Interactive acceptance and later artwork revisions still require appropriate
board checks. Version 1.0 and public deployment require a later owner decision.
A browser result is **not** evidence of hardware capability enforcement.
Both supplied CC BY 4.0 images are authorized for this edition. Runtime artwork
switching, title navigation, a source-repository QR screen and emulated CHERI
indicator LEDs are implemented. Final release acceptance and deployment
remain pending; this is a development edition.

## Play

The LCD starts black, fades in the artwork over six seconds, then displays a
title menu after another three seconds. A fresh **Right** starts story mode;
**Down** changes the artwork. The hidden **Left** shortcut opens a source
repository QR screen; any fresh direction/center press returns to the title,
as does a click on the QR canvas in the browser.

Use the joystick on the board, or arrow keys and Space/Enter in the browser.
The browser's artwork button changes the selected title image without
resetting progress, including during play. Artwork selection survives runs
and soft resets; a power reset (or browser reload) restores the build's
initial selection. Focus loss pauses the browser and clears held keys.

- Steer through cars, trucks, buses and trailers; collect cash and gold.
- Vehicle collisions build wanted stars. Police appear from three stars,
  aim once per pass, and can explode on collisions. Police hits are fatal
  at five stars.
- Damage, escape timers, decorative roadside scenes, user LEDs and two RGB
  status LEDs share the same game state in firmware and the preview.
- WASTED holds the colored frame and red RGB LEDs for three seconds, then
  desaturates the entire screen over six seconds while both LEDs shift toward
  purple and dim to off using the same eased fade. The monochrome frame
  remains until dismissed; billing and respawn restore normal colors.
- A fresh center press after WASTED leads to an animated **ITEMIZED REGRET**
  receipt. Left resets the
  game and balance; Right resumes with the balance after charges and a
  repaired car. Held inputs cannot skip the receipt.
- The wallet is an in-memory game score, not a financial or persistent wallet.
  There is no sound, backend service or saved progress.

Future browser audio, read-only host sound relays, native USB options and
vendor module comparisons are documented in [audio options](docs/audio-options.md).
These are researched options, **not implemented audio support**.

The HUD's `T/B/W` counters show verified tag, bounds and wallet denials.
In firmware, a separate wallet compartment owns the balance; a restricted
probe compartment performs actual forbidden accesses and recovers from
capability faults. Unexpected results stop play with **PROTECTION ERROR**,
rather than claiming a successful denial. Browser and native host tests use
an explicitly **EMULATED** backend.

See [architecture](docs/architecture.md) for compartment boundaries and the
exact audit contract.

## Browser and host development

The pinned Nix flake currently supports **x86_64 Linux**. Install Nix with flakes
enabled, then run:

```sh
./tools/preview
```

Open the Ethernet address printed by the tool. `--port 8001` changes the port;
`--build-only` creates static files in `build/game-preview` without a server.
The default is a unique private IPv4 address on a wired interface, excluding
Wi-Fi, bridges and loopback; `--host IPV4` is an explicit override. A missing
or ambiguous Ethernet address fails rather than falling back to loopback or
all interfaces. Allow the chosen port from your LAN in the host firewall.
Only the built preview directory is served. `--build-only` never inspects
network interfaces. Loopback is opt-in with `--host 127.0.0.1`.
The newer Claude-generated `alternate` image is the default.
`--artwork primary` selects the older image initially; both remain available
through the compact **switch artwork** button and title-menu Down input.

```sh
./tools/check-browser
```

This builds the browser preview using the locked Emscripten toolchain and runs
ASan/UBSan native tests, browser input/focus/restart/billing checks, native/Wasm
trace comparisons, LED checks and renderer/pixel checks. Playwright uses the
pinned Chromium with its sandbox enabled. Test screenshots remain under
`build/game-preview/screenshots`.

The flake supplies Emscripten, C++, Python, Pillow, QR generation, zbar decoding,
Playwright and Chromium.
It pins Sonata software v1.3 and its dependency graph in `flake.lock`.
The scripts enter the appropriate Nix shell automatically.
Firmware and simulator shells also provide Pillow, so image conversion and
its tests do not depend on a host-system Pillow installation.

## Firmware build and audit

```sh
./tools/check
```

This performs a reproducible artwork check, warning-free application rebuild,
component ELF checks, the CHERIoT audit, slot-2 UF2 validation, negative policy
mutation tests, loader tests with synthetic devices, and native game tests.
It does not access hardware.

The firmware target remains `gta6_wallpaper`; output is under
`build/cheriot/cheriot/release`. Only application **slot 2**
(`0x10000000`, UF2 family `0x6CE29E60`) is emitted. Other application slots
and FPGA bitstreams are not modified.

For direct builds:

```sh
nix develop --accept-flake-config
xmake f --board=sonata-1.1 --game-scene=none
xmake build gta6_wallpaper
```

Outside Nix, explicitly set `SONATA_SOFTWARE` to a compatible Sonata software
checkout containing its CHERIoT SDK and display drivers. There is no assumed
operator-specific directory layout.

## RTL simulator

```sh
./tools/sim-test wallpaper
./tools/sim-test cheri-selftest
./tools/sim-test wasted-fade
./tools/sim-test wasted-monochrome
./tools/sim-test source-qr
./tools/sim-test --all
```

Each scene builds without a UF2, runs the
Sonata RTL simulator in a private network namespace, and compares all 20,480
LCD pixels with the native renderer. Linux must permit unprivileged user and
network namespaces. The newer default artwork uses
`build/sim-firmware/alternate/<scene>` and `build/sim/alternate/<scene>`;
explicit `primary` builds retain paths without `alternate/`.
The two WASTED fixtures freeze the shared color/LED fade at its midpoint and
completed monochrome frame.
`source-qr` additionally decodes the rendered QR to verify the source URL.

`cheri-selftest` exercises actual tag, bounds and store-permission faults,
all road widths, repeated recovery, grant-retention and wrong-cause controls,
stack/timing budgets and continued gameplay. Its transcript checker reconciles
exact handler-verified totals and the simulator's reported fault classes.
Fixture scenes cannot be built as hardware images; the tool restores the
normal hardware build configuration afterward.

The simulator uses scripted input, not a live joystick. Physical startup fault
recovery has been checked for the recorded image, but controls, complete
interactive gameplay, electrical behavior and LCD timing still require owner
acceptance on the board.

## Credits

Thanks to lowRISC contributors for the
[Sonata example/demo suite](https://github.com/lowRISC/sonata-software/tree/v1.3/examples)
and its [CHERI Snake game](https://github.com/lowRISC/sonata-software/blob/v1.3/examples/snake/snake.cc).
The demos provide board/UI references; Snake is an earlier demonstration of
capability faults used as gameplay events. This project also builds on
[CHERIoT RTOS](https://github.com/CHERIoT-Platform/cheriot-rtos) and Sonata's
display-driver support. Upstream work keeps its original licenses, including
Apache-2.0 for the Sonata examples; project notices do not relicense it or
imply endorsement.

## Artwork and licenses

Code and tools are [MIT licensed](LICENSE). Both supplied images and their
generated splash data are **CC BY 4.0**, supplied by the **Grand Time Auto
project**. `primary` is `artwork/gta6-8bit-01.png` (941×1672); `alternate` is
`artwork/gta6-alternate-01.png` (750×956). Only the alternate is described by
the owner as independent and **generated using Claude**. These are
owner-supplied provenance and licensing statements, not an independent rights
audit.

Both public PNGs have metadata removed and decoded pixels unchanged.
The full-resolution splash is center-cropped and smoothly resized directly to
128×160 with Lanczos, then encoded as the LCD's required RGB565. There is no
extra pixelation, restricted palette, dithering or replacement lettering by
default; the old coarse `--style retro` remains opt-in.
[Artwork attribution and transformation details](artwork/README.md)
accompany browser and firmware build outputs.

```sh
./tools/convert-wallpaper --preview-dir build/preview
./tools/convert-wallpaper --artwork alternate --preview-dir build/preview-alternate
./tools/convert-wallpaper --artwork primary --check
./tools/convert-wallpaper --artwork alternate --check
```

The converter generates `firmware/wallpaper_image.h` and
`firmware/wallpaper_image_alternate.h`. Build helpers accept
`--artwork primary|alternate`; direct xmake builds use `--artwork=...`.
The selection is the initial/default artwork, not a restriction against linking
both: both RGB565 bitmaps live in read-only program memory for runtime switching,
without adding a framebuffer. Custom conversion remains available:

```sh
./tools/convert-wallpaper --input path/to/your-image.png --output build/custom-image.h
```

Custom inputs are not automatically licensed by this project. Replacing a
bundled source or generated header requires updating attribution and tests;
the default checks intentionally verify both bundled images.

## Source QR and board indicators

The hidden title QR encodes the planned public source repository:
`https://github.com/xjamesmorris/gta6-cheri-sonata`. It is generated locally,
not fetched from a QR service, and uses 246 packed bytes for 41 modules
including the quiet zone.

```sh
./tools/generate-source-qr --check
./tools/generate-source-qr --preview build/source-qr.png
```

The browser includes all nine CHERI fault LEDs, CHERI/Legacy and
FPGA Config/Ibex Boot/Halted indicators, all eight user LEDs and both RGB LEDs.
Bounds (0), Tag (1) and Permit Store (5) follow emulated gameplay denials;
unused fault indicators remain off. FPGA Config, Ibex Boot and CHERI model a
running target; Legacy and Halted stay off. Fault indicators retrigger and
fade over approximately 1.66 seconds using the pinned Sonata PWM timing.
This models the running board; it is **not live telemetry** and does not
replay hardware startup self-tests. See [architecture](docs/architecture.md).

## Loading a board

Read [the operator guide](docs/board-loading.md) before using `tools/load-slot2`.
Board identities and device paths **must be configured explicitly**. The loader
retains identity/topology checks, mount and unfinished-write guards, private
approved-byte snapshots, isolated Python and optional digest-specific approval.
No board configuration is bundled.

## GitHub Pages

The [Pages workflow](.github/workflows/pages.yml) builds and checks the same
locked browser edition. Deployment is disabled by default: it requires a
manual dispatch from `main`, an approval variable, and the `github-pages`
environment. Nothing enables Pages or creates releases automatically.

See [Pages configuration and verified action pins](docs/pages.md). Initial
publication, deployment and a future 1.0 release remain separate owner decisions.
