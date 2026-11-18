# Third-party notices and acknowledgements

Grand Time Auto builds on work by other people. This file names each parent
work, says how this project uses it and records its license. It complements
the [project license](LICENSE) and the separate
[artwork notice](artwork/README.md).

Project code and tools are MIT licensed unless a file header says otherwise.
Upstream work keeps its own license. Nothing here relicenses upstream work or
implies that an upstream project endorses this one.

The revisions below are the ones selected by `flake.lock` and by the
submodules of that Sonata software revision.

## Source files adapted from upstream

Three tracked files contain material adapted from
[lowRISC Sonata software](https://github.com/lowRISC/sonata-software) v1.3,
revision `722cdea0207920997b0895ea1f430619b343e10c`. Upstream licenses that
material under Apache-2.0 with the notice `Copyright lowRISC Contributors.`
The adapted files stay under Apache-2.0, keep that notice and describe their
changes in their headers. The license text is in
[LICENSES/Apache-2.0.txt](LICENSES/Apache-2.0.txt). The upstream repository
has no `NOTICE` file.

| File here | Upstream source | What was kept and what changed |
| --- | --- | --- |
| `firmware/lcd_panel.cc` | `libraries/lcd.cc` | Keeps the SPI, backlight and chip-select helpers and the ST7735 bring-up order. The backlight now stays off until controller RAM is black, and the drawing API is replaced by brightness-scaled RGB565 row streaming. |
| `firmware/lcd_panel.hh` | `libraries/lcd.hh` | Keeps the shape of the LCD wrapper class and its `Size` type. It is reduced to a fixed portrait `Panel`; the geometry, colour and font types are removed. |
| `xmake.lua` | `examples/xmake.lua`, `common.lua`, `libraries/xmake.lua` | Keeps the project preamble, UF2 conversion, LCD library and firmware target layout. Only a slot-2 UF2 is emitted; the compartments, build options and notice handling are new. |

Two smaller reuses do not change a file's license:

- `flake.nix` repeats the lowRISC binary-cache URL and public key published
  in upstream's `flake.nix`. They are configuration values needed to use that
  cache.
- `tools/sim-test` starts the simulator with the command form used by
  upstream's `scripts/run_sim.sh`.

## Compiled into firmware images

Firmware images link the code below from the pinned checkout. None of it is
copied into this repository. Each hardware build writes
`gta6_wallpaper.third-party-notices.txt` beside the image. That file holds
this document, the Apache-2.0 text, and the CHERIoT RTOS and display-driver
license and notice files taken verbatim from the pinned checkout.

| Component | Revision | License | Copyright notice |
| --- | --- | --- | --- |
| [lowRISC Sonata software](https://github.com/lowRISC/sonata-software): the LCD panel driver adapted above | `722cdea0207920997b0895ea1f430619b343e10c` (v1.3) | Apache-2.0 | Copyright lowRISC Contributors. |
| [CHERIoT RTOS](https://github.com/CHERIoT-Platform/cheriot-rtos): loader, switcher, scheduler, allocator, libraries and headers | `21fe9d4775108325e4efe930c427564731d22f00` | MIT | Copyright (c) Microsoft Corporation. Source headers read "Copyright Microsoft and CHERIoT Contributors." |
| [display_drivers](https://github.com/engdoreis/display_drivers): `lcd_base.c`, `lcd_st7735.c` and their headers | `e07390d26c4bf0eccdb736a4f30148881b01830d` (v0.3.1) | Apache-2.0 | Copyright (c) 2022 Douglas Reis. |

CHERIoT RTOS includes third-party material that these builds compile:

- LLVM libc++ headers, under Apache-2.0 WITH LLVM-exception.
- `memcmp`, `memcpy` and `memset` from FreeBSD, under BSD-3-Clause, Copyright
  (c) 1990, 1993 The Regents of the University of California.
- [magic_enum](https://github.com/Neargye/magic_enum) 0.8.1, under MIT,
  Copyright (c) 2019 - 2022 Daniil Goncharov.
- [arith64](https://github.com/glitchub/arith64) integer helpers, released
  into the public domain under the Unlicense.

The full terms for the first three are in the RTOS's own
`THIRD_PARTY_NOTICES.txt`, which is part of the notices file written beside
each image.

The display driver's ST7735 command and initialisation tables come from the
Adafruit ST7735 library. They carry this notice, reproduced here as it
requires:

```text
This is a library for several Adafruit displays based on ST77* drivers.

  Works with the Adafruit 1.8" TFT Breakout w/SD card
    ----> http://www.adafruit.com/products/358
  The 1.8" TFT shield
    ----> https://www.adafruit.com/product/802
  The 1.44" TFT breakout
    ----> https://www.adafruit.com/product/2088
  as well as Adafruit raw 1.8" TFT display
    ----> http://www.adafruit.com/products/618

Check out the links above for our tutorials and wiring diagrams.
These displays use SPI to communicate, 4 or 5 pins are required to
interface (RST is optional).

Adafruit invests time and resources providing this open source code,
please support Adafruit and open-source hardware by purchasing
products from Adafruit!

Written by Limor Fried/Ladyada for Adafruit Industries.
MIT license, all text above must be included in any redistribution.
```

## Compiled into the browser build

| Component | Version | License | Copyright notice |
| --- | --- | --- | --- |
| [Emscripten](https://emscripten.org/): the JavaScript runtime in `game.js` and the memory and stack helpers in `game.wasm` | 3.1.64, from the pinned nixpkgs | MIT or University of Illinois/NCSA | Copyright (c) 2010-2014 Emscripten authors, see AUTHORS file. |
| LLVM libc++ headers shipped with that Emscripten | 3.1.64 | Apache-2.0 WITH LLVM-exception | See the [LLVM license](https://llvm.org/LICENSE.txt). |

`game.js` starts with Emscripten's own license comment. The browser build
writes `third-party-notices.txt`, which holds this document and the `LICENSE`
file from the pinned Emscripten. The RTOS, the display driver and the adapted
LCD panel driver are not part of the browser build.

## Ideas and prior work

- lowRISC's
  [CHERI Snake](https://github.com/lowRISC/sonata-software/blob/v1.3/examples/snake/snake.cc)
  first used capability faults as gameplay events on Sonata. This project
  takes that idea further; it does not use Snake's code.
- The compartment, error-handler and audit model is that of CHERIoT RTOS and
  [cheriot-audit](https://github.com/CHERIoT-Platform/cheriot-audit).
  `policy/gta6_wallpaper.rego` extends the RTOS's base policy.
- The [Sonata system](https://github.com/lowRISC/sonata-system) is by lowRISC
  and the [Sonata PCB](https://github.com/newaetech/sonata-pcb) by NewAE
  Technology. The emulated board indicators follow the system RTL cited in
  [architecture](docs/architecture.md). The loader's slot rules follow the
  [Sonata RP2040 firmware](https://github.com/newaetech/sonata-rp2040)
  documentation.
- Jacob suggested earned-capability level unlocks in the CHERIoT public chat;
  see the [roadmap](docs/roadmap.md).

## Build, test and development tools

These are used but not redistributed: Nix and nixpkgs, lowRISC's Nix
packaging, xmake, the CHERIoT LLVM toolchain, cheriot-audit, Microsoft's
uf2conv, the Verilator-based Sonata simulator, Emscripten, Pillow,
python-qrcode, zbar, Playwright and Chromium. The GitHub Actions workflows use
the pinned actions listed in [Pages configuration](docs/pages.md).

QR Code is a registered trademark of DENSO WAVE INCORPORATED.

## Artwork

The splash images and their generated data are covered separately by the
[artwork notice](artwork/README.md).
