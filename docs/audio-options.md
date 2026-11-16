# Music and sound-effect options

Status: options for review, not implemented. Research refreshed on 2026-11-16
against the lowRISC Sonata v1.3 documentation, the pinned `sonata-software`
v1.3 tree and the `sonata-system` revision locked by its flake (`e32eb27f`).
No module has been purchased, wired or enabled. This project has no audio
driver, USB stack or host sound relay.

## Recommendation

1. **Design the sound palette in the browser preview** with Web Audio. This
   needs no board.
2. **Easiest useful board-connected route: a host sound relay.** The firmware
   prints compact game-event lines on the console it already uses. A separate
   host program follows the operator's existing console *log file* and mixes
   sounds on the computer's speakers, built-in or USB. This needs no new Sonata
   hardware, peripheral authority or USB stack. Expect a perceptible delay that
   must be measured; it is not frame- or sample-locked audio.
3. **Easiest standalone option: the SparkFun Qwiic Buzzer.** The module
   generates tones itself over I2C, which suits beeps, pickups, sirens and
   single-voice jingles.
4. **Richer standalone audio, later:** a UART-controlled sample player
   (Adafruit Audio FX Sound Board) or a UART MIDI synthesizer (M5Stack Unit
   Synth) for polyphonic music. Both need a UART routed to a header, which the
   current audit forbids.
5. **Sonata is not a USB audio device today.** Its FPGA design does contain a
   USB *device* controller on the separate User USB port, so class-compliant
   USB MIDI is a plausible future project. USB Audio Class streaming is a poor
   fit, and Sonata has no USB host port for speakers.

These are engineering recommendations, not claims that an audio driver exists.

## Routes at a glance

| Route | Sound | Added hardware | New firmware authority | Main caveat |
| --- | --- | --- | --- | --- |
| Browser Web Audio (preview) | Rich and mixed | None | None | Preview only; no board events |
| Console event relay to a host mixer | Rich and mixed, on host speakers | None | None: existing debug UART | Needs the host and picocom logging; delay must be measured |
| Qwiic buzzer | Single-voice tones | Qwiic module | One I2C instance | Not a mixer |
| UART sample player or MIDI synth | Sampled clips, or polyphonic MIDI | Module, speaker, wiring, power | One UART plus pinmux setup | Logic levels, power and asset preparation |
| Native USB MIDI on User USB | Rich, synthesized on the host | Second USB cable | `usbdev` and its interrupt | No USB stack today; substantial driver work |
| PWM or I2S amplifier | CPU-generated waveforms or PCM | Amplifier, speaker, wiring | PWM and pinmux, or a new FPGA I2S block | Audio-rate CPU work; no stock I2S |

## What Sonata's USB connections provide

| Connection | Hardware path | What the host sees today | Role for game audio |
| --- | --- | --- | --- |
| Main USB | On-board USB hub to an FTDI FT4232H [1][10][11] | JTAG and two virtual UARTs. CHERIoT's debug console is the fixed `uart`, 921,600 baud by default [1][3][17]. | Route 1 text event channel. The FT4232H is a fixed-function USB-to-UART/MPSSE bridge [18], not a programmable audio or MIDI device. |
| Main USB | Same hub to the RP2040 [1][11] | The `SONATA` mass-storage drive used to load bitstreams and firmware [12] | None. Its firmware enables only TinyUSB mass storage [12]. Do not reprogram it for audio. |
| User USB | Full-speed transceiver to the FPGA's OpenTitan `usbdev` block at `0x8040_0000` [2][4][5][11] | Nothing until firmware enumerates. A v1.3 bring-up check presents a vendor-specific bulk interface that the release procedure opens as a host serial port [8][9]. | Possible future USB MIDI device (route 2). sonata-software v1.3 ships no USB stack [3]. |

Sonata has no USB host port: `usbdev` is a device controller [6]. A USB speaker
cannot be plugged into Sonata; it belongs on the host (route 3).

## Four ways to get sound "via USB to the host"

### 1. Console event relay with a host mixer (recommended first)

```text
game loop -> bounded event queue -> short console line -> debug UART
  -> FT4232H -> host tty -> operator's picocom --logfile -> log file
  -> read-only host relay -> mixer -> built-in or USB speakers
```

- **No new device authority.** The `debug` library already owns the console
  UART, so an event line needs no new MMIO capability or audit exception. Its
  writes busy-wait whenever the transmit FIFO is full [17]. At 921,600 baud
  with 8N1 framing each byte takes about 11 µs on the wire: keep lines short,
  bound the rate and measure the cost under worst-case LCD rendering.
- **Emit from the game loop only.** Use one fixed-format line per accepted
  event, for example `SND <seq> <event>`. Never emit, block or log from the
  CHERIoT error handler; record the fault, unwind, and let the main loop report
  it after recovery.
- **Follow the log, not the port.** The relay reads the log file picocom
  already writes; picocom records every byte read from the port, before input
  mapping [22]. The relay never opens the tty, never writes to the board and
  never changes picocom, its options or its log. Start at the current end of
  the file so old events are not replayed, and tolerate truncation or rotation.
- **Treat log lines as untrusted input.** Strip ANSI colour escapes and
  carriage returns (CHERIoT debug output colours each line's prefix [17]).
  Accept only an allowlisted event vocabulary, map events to local,
  pre-approved sound files, and never interpret paths, commands or free text
  from the log. Use the sequence number to drop duplicates and notice board
  resets. Pick a prefix that cannot collide with existing markers and checks
  such as `FAIL:`, `CAP BLOCKED` or `Wallpaper ready`.
- **Prototype without firmware changes.** Existing diagnostic lines, such as
  screen-phase changes and `CAP BLOCKED` reports, can drive a first experiment,
  but they are diagnostics rather than a stable interface.
- **Latency.** The delay adds up game-loop scheduling, UART transmission, FTDI
  batching (Linux's `ftdi_sio` driver programs a per-port latency timer and
  uses 1 ms in low-latency mode [21]), the log write, file-change detection and
  the host audio buffer. That suits pickups, sirens and music cues; measure end
  to end before promising anything tighter. Changing host serial settings is an
  operator decision outside this design.
- **Limits.** Sound needs the host, the cable and picocom logging. Anyone who
  can read the log can see the events.

*Unverified variant:* the board documentation lists two FTDI virtual UARTs [1].
A dedicated event UART would avoid sharing the console log, but needs `uart1`
or `uart2` authority (forbidden by the current audit), a schematic check of the
UART-header jumpers and FTDI channel wiring, and a new host reader on a
different, confirmed-unused tty.

### 2. Native USB MIDI or USB Audio Class from Sonata (future project)

**Present in v1.3:** the FPGA design instantiates OpenTitan's USB 2.0
full-speed device controller and wires it to a dedicated User USB transceiver
set for full speed [2][4][5]. The IP supports up to 12 endpoints, packets of at
most 64 bytes, and control, bulk, interrupt and isochronous transfers;
isochronous transfers larger than 64 bytes are not supported [6]. CHERIoT RTOS
has a register-level `OpenTitanUsbdev` driver, and the Sonata board
description exposes the `usbdev` MMIO region and its interrupt [7].

**Missing:** sonata-software v1.3 states that there is no USB stack or full USB
example [3]. The only enumeration code is a bare-metal sonata-system check that
presents a vendor-specific bulk interface with test-oriented control handling
[8]. No CDC-ACM, USB Audio Class or USB MIDI implementation exists for Sonata.

- **USB MIDI 1.0 is the plausible native route.** MIDIStreaming uses bulk
  endpoints carrying 32-bit event packets [19], which fit 64-byte packets.
  Sonata would send note and event messages; the host's class driver exposes a
  MIDI port (on Linux, `snd-usb-audio` handles USB audio and MIDI devices
  [20]), and a host software synthesizer or Web MIDI page renders the sound.
  Work items: descriptors, enumeration, reset/suspend/disconnect handling, a
  second cable to the User USB port, and a new compartment that owns only
  `usbdev` and its interrupt. USB control requests come from the host, so parse
  them as untrusted input and contain any fault without stopping the game.
- **USB Audio Class streaming is not recommended.** UAC 1.0 carries PCM over
  isochronous endpoints [19]. At most 64 bytes fit in each 1 ms full-speed
  frame, so 16-bit mono at 32 kHz already fills every packet with no room for
  rate adjustment, while 48 kHz 16-bit stereo would need 192 bytes. Firmware
  would also have to synthesize and mix samples continuously, without DMA [2],
  while the game renders.
- **Not Sonata-native:** an external USB-capable microcontroller running a
  class-compliant stack (TinyUSB implements UAC1/UAC2 and MIDI device classes
  [23]) could act as a USB MIDI bridge fed from Sonata over UART or I2C. It is
  another programmable device whose firmware, power and wiring need the same
  review. Do not repurpose the on-board RP2040 or FT4232H.

### 3. USB speakers on the host

A USB speaker, such as Adafruit's Mini External USB Stereo Speaker [41], is
just the host's output device. Routes 1, 2 and 4 can all play through it via
the host's normal audio stack, adding that device's own buffering. It does not
connect to Sonata.

### 4. Browser options

| Option | Constraints |
| --- | --- |
| Web Audio in the existing preview | Best for designing sounds. An `AudioContext` created before a user gesture starts suspended and must be resumed after one [24]. Provide mute and volume controls. |
| Browser fed by the host relay | Keeps picocom as the only tty reader. A loopback-only relay forwards parsed events to a local page that starts Web Audio after a click. Do not expose it beyond the machine. |
| Web Serial reading the board | Chromium-based desktop browsers since Chrome 89 [26]; secure contexts only; `requestPort()` needs a user gesture and shows a port chooser, and previously granted ports can be reopened [25][26]. Chromium requests exclusive terminal access (`TIOCEXCL`) after opening a POSIX port [27]; picocom instead takes a lock-file or `flock(2)` lock [22], which binds only programs that also take the lock. Depending on which opened first, one open fails or both read and split the byte stream. That breaks the single-reader rule, so consider it only for a session in which the browser deliberately replaces picocom as the console reader. A granted page can also write to the console. |
| Web MIDI | Useful only with route 2 or a USB MIDI bridge. Secure contexts only; Chrome (rolled out from version 124) and Firefox require explicit permission for all MIDI access; not available in every major browser [28]. |

## Hardware and software constraints

- Sonata's documented user outputs are LEDs and the LCD, not a speaker [1].
  The v1.3 memory map provides GPIO, PWM, UART, I2C, SPI, ADC and the USB
  device; there is no I2S block, and the DMA slot is reserved [2].
- Header I/O is 3.3 V: the Arduino header supports only 3.3 V shields, and the
  v1.3 constraints use 3.3 V LVCMOS for the digital header pins [1][14]. Check
  every module output (TX, DOUT, MISO) before connecting it.
- The firmware audit grants only the LCD/backlight, debug UART and onboard
  GPIO/RGB LEDs, and forbids external GPIO, pinmux and secondary UARTs.
  External audio needs a separately reviewed compartment with only the chosen
  device authority, not a blanket relaxation of the audit policy.
- Respect existing peripheral connections and disconnect Pmod0 from external
  hardware before programming. Choose an unused connector, bus, address and
  power budget before connecting anything.
- **Routing.** Under the documented default pinmux selection, Qwiic connectors
  are driven by the I2C blocks (`i2c0`: qwiic0, shared with the Arduino I2C
  pins; `i2c1`: qwiic1), and the v1.3 I2C example reads a Qwiic sensor without
  pinmux writes [3][13][16]. Confirm on hardware. UART, SPI and PWM on other
  headers need pinmux configuration first.
- **Connector families do not identify the bus.** Qwiic is a 3.3 V I2C
  convention [38]. Adafruit's 4-pin STEMMA and STEMMA QT connectors carry I2C,
  but 3-pin STEMMA connectors carry analog, digital or PWM signals, and STEMMA
  power may be 3–5 V [37]. Seeed Grove connectors can be digital, analog, UART
  or I2C, at 3.3 V or 5 V depending on the module [39].
- Do not drive a loudspeaker or magnetic transducer directly from a GPIO. Use
  a module with its own driver or amplifier.

## Module comparison

Checked 2026-11-16 against vendor pages and datasheets. Effort ratings assume
the authority, wiring and power review above; none is a drop-in addition to the
current firmware.

| Module | Interface and electrical | Sound source | Output | Voices | Sonata effort |
| --- | --- | --- | --- | --- | --- |
| [SparkFun Qwiic Buzzer](https://www.sparkfun.com/sparkfun-qwiic-buzzer.html) (BOB-24474) [29] | I2C over Qwiic, default address `0x34`; 3.3 V supply and logic; about 95 mA at full volume | Onboard ATtiny84 generates tones on command (frequency, duration, 7 volume levels) | Onboard magnetic buzzer | One tone at a time | **Low to moderate.** Qwiic plug-in and an existing RTOS I2C driver; port the small command protocol into a new audited compartment. Check the 3.3 V budget and pull-ups. |
| [Adafruit Audio FX Sound Board + 2x2W Amp - WAV/OGG Trigger - 2MB](https://www.adafruit.com/product/2210) (#2210) [30] | 9600-baud UART when `UG` is grounded, or 11 trigger pins; 3–5.5 V supply; RX accepts 3.3 V or 5 V logic, TX is 3.3 V | Decodes WAV or Ogg Vorbis from 2 MB onboard flash | 2 × 2 W class-D amplifier for 4–8 Ω speakers | One file at a time ("no polyphonic ability") | **Moderate.** Needs `uart1` or `uart2` routed to a header. Its micro-USB port mounts the flash for copying files; it is not an audio interface. |
| [Seeed Grove - MP3 v4.0](https://www.seeedstudio.com/Grove-MP3-V4-p-5862.html) (107020129) [31] | Grove UART with AT commands (Seeed's examples use 115,200 baud); 5 V input; chip LDO output listed as 3.3 V | WT2605CX decodes MP3, WAV and WMA from microSD, SPI flash or a USB flash disk | JST speaker port (speaker included) and 3.5 mm jack | One track; a cut-in command pauses the current track, plays another, then resumes it | **Moderate.** As above, plus Grove-to-header wiring. Seeed does not state the UART logic level; measure or level-shift its TX before it reaches an FPGA pin. |
| [M5Stack Unit Synth](https://docs.m5stack.com/en/unit/Unit_Synth) (U178) [32] | Receive-only serial MIDI (`UART_RX`; library default 31,250 baud); 5 V supply | SAM2695 General MIDI synthesizer | NS4150B amplifier and an 8 Ω 1 W speaker | Up to 64 voices without effects, 38 with effects (SAM2695 datasheet) | **Moderate.** Sonata only transmits, so no module output reaches the FPGA; the SAM2695 is a 3.3 V device, but confirm the input level on the module schematic. Needs UART routing. Availability not verified. |
| [Adafruit VS1053 Codec + MicroSD Breakout - MP3/WAV/MIDI/OGG Play + Record - v4](https://www.adafruit.com/product/1381) (#1381) [33] | SPI with DREQ flow control, or real-time MIDI on a 31,250-baud UART pin selected at boot; 3–5 V power and level-shifted logic | VS1053b decodes MP3, AAC, Ogg Vorbis, WMA, WAV and MIDI and includes a General MIDI synthesizer | Headphone or line-level output; add an amplifier for loudspeakers | MIDI up to 64 notes, fewer sustained; one decoded file stream | **High** for file playback (SPI streaming with flow control); **moderate** in real-time MIDI mode. |
| [SparkFun I2S Audio Breakout - MAX98357A](https://www.sparkfun.com/sparkfun-i2s-audio-breakout-max98357a.html) (DEV-14809) [34] | I2S (BCLK, LRCLK, DIN), no MCLK; 2.5–5.5 V supply; digital inputs accept 3.3 V logic | None: DAC plus class-D amplifier for a continuous 8–96 kHz PCM stream | Drives a 4–8 Ω speaker directly | Whatever the sender mixes | **Very high.** Stock Sonata v1.3 has no I2S peripheral or driver: needs FPGA changes and a continuously fed sample stream. Adafruit #3006 is equivalent. |
| [Digilent Pmod AMP2](https://digilent.com/shop/pmod-amp2-audio-amplifier/) (410-233) [35] | 6-pin Pmod with `AIN` on pin 1, gain and shutdown pins; Digilent lists 3.3 V or 5 V power | None: SSM2377 class-D amplifier designed for an analog input; Digilent notes PWM can be used after reconstruction filtering | 1/8-inch mono jack | Whatever the sender generates | **High.** Needs audio-rate waveform generation (see the PWM caveat). Sonata cannot select PWM on Pmod pins 1 or 7, so a plugged-in AMP2 gets GPIO-only drive; hardware PWM needs adapter wiring. |

**Listed US prices and stock** (vendor pages on 2026-11-16; these change):
SparkFun Qwiic Buzzer $10.50, stock shown inconsistently (both "In stock" and
"Backorder" appear); Adafruit #2210 $24.95, 84 in stock (the 16 MB #2217 was
$29.95 and out of stock); Seeed Grove - MP3 v4.0 US$12.90, stock not shown;
M5Stack Unit Synth not verified; Adafruit #1381 $24.95, 64 in stock; SparkFun
DEV-14809 $7.50, in stock (Adafruit #3006 $5.95, out of stock); Digilent Pmod
AMP2 $12.00 list, sold through distributors with stock not shown.

Notes:

- Sound-player USB ports are storage paths: the Audio FX board's micro-USB
  port mounts its flash, and the Grove MP3 v4.0 lists USB flash disks as a
  media source. Neither is a USB audio interface or a Sonata-to-host path.
- SparkFun's Qwiic MP3 Trigger, the obvious I2C MP3 option, is retired
  (DEV-16892 and its DEV-19030 successor) [40].
- Analog alternatives: Adafruit's STEMMA Speaker (#3885, $5.95, in stock)
  combines a class-D amplifier and a 1 W 8 Ω speaker with an analog input on a
  3-pin STEMMA JST-PH connector, not STEMMA QT/I2C [37]. Digilent's Pmod I2S2
  is an I2S DAC/ADC with 3.5 mm line jacks and has the same I2S caveat as the
  MAX98357A [36].

### Why the I2C buzzer is a useful first board option

The module's own microcontroller generates tones from documented frequency,
duration and volume commands, so the renderer never services audio samples
[29]. It needs one I2C instance, which a Qwiic port appears to reach without
pinmux writes (confirm on hardware) [13][16]. Its default address is `0x34`;
check for conflicts rather than assuming it is free. It draws about 95 mA at
full volume, so confirm the Qwiic 3.3 V supply budget on the board schematic
[11] and start at a low volume.

For music plus an engine tone at the same time, a single buzzer needs
time-sharing or priority rules. That is a deliberate retro compromise, not a
mixer.

### PWM caveat

Sonata v1.3 PWM channels have 8-bit period and pulse-width counters clocked
directly by the 40 MHz system clock, so the longest period is 256 cycles,
about 156 kHz [15]. The PWM period alone therefore cannot make an audible note.
Audio needs software to update the duty value at the sample rate (an 8-bit PWM
DAC followed by a reconstruction filter) or software-timed GPIO square waves.
With no DMA [2], either costs CPU time and jitters while the LCD renders. Do
not add a busy-loop waveform generator to the game thread. PWM outputs exist
only on specific header pins, for example Pmod1 pins 2 and 8 [13].

## Proposed sound palette

| Event | Proposed sound |
| --- | --- |
| Cash or small gold | Short rising two-note pickup |
| Large gold | Longer bright arpeggio, played once per pickup |
| Minor damage | Short low click/thud |
| Police approaching | Quiet alternating siren, rate-limited |
| Police explosion | Brief noise burst with a descending tail |
| WASTED | Short descending phrase, then fade to silence |
| Receipt charges | Soft tick as each charge settles |
| CHERI-denied operation | A distinct short "blocked" chirp, not a crash alarm |
| Background | Optional original, low-volume loop; default music volume below SFX |

These sounds should be original synthesis or separately licensed assets. The
new splash artwork's CC BY license does not license unrelated music or samples.
Record title, creator, source, license and modifications for every imported
audio asset.

## Proposed implementation boundaries

- Keep gameplay emitting semantic events rather than calling an audio driver
  from collision or capability-fault handlers.
- Use a small bounded event queue, priority/coalescing rules and an observable
  overflow counter. WASTED and significant impacts outrank routine pickups.
- Never allocate, block on a device, emit sound or events, or log from the
  CHERIoT error handler. Record the fault first; schedule presentation after
  recovery.
- Put hardware audio ownership in a dedicated compartment with only the
  chosen bus/device authority. It must not receive wallet write authority.
- Console relay lines come only from the main loop, use an allowlisted
  vocabulary with sequence numbers, and are rate-limited. The host side only
  follows the existing log; it never opens the console tty or writes to the
  board.
- Any future USB compartment owns only `usbdev` and its interrupt, validates
  every host request, and stops audio on failure with bounded error reporting
  outside fault handlers, without affecting gameplay, score or fault outcomes.
- Browser blur/pause/mute should stop active notes; disabling audio must not
  change game state, score or fault outcomes.
- Validate timing under worst-case LCD rendering and intentional fault load
  before promising a sample rate or latency.

## Sources

1. [Sonata board reference, v1.3](https://github.com/lowRISC/sonata-system/blob/v1.3/doc/architecture/board.md)
2. [Sonata core reference and memory map, v1.3](https://github.com/lowRISC/sonata-system/blob/v1.3/doc/dev/ref-manual.md)
3. [Using Sonata IO, sonata-software v1.3](https://github.com/lowRISC/sonata-software/blob/v1.3/doc/guide/sonata-io.md)
4. [Sonata system RTL with the `usbdev` instance (flake-locked revision)](https://github.com/lowRISC/sonata-system/blob/e32eb27f638ccc3dbd86ff9b80ce21cac3828028/rtl/system/sonata_system.sv)
5. [Sonata FPGA top level with the User USB transceiver signals (flake-locked revision)](https://github.com/lowRISC/sonata-system/blob/e32eb27f638ccc3dbd86ff9b80ce21cac3828028/rtl/fpga/top_sonata.sv)
6. [OpenTitan USB device IP README as vendored by Sonata (flake-locked revision)](https://github.com/lowRISC/sonata-system/blob/e32eb27f638ccc3dbd86ff9b80ce21cac3828028/vendor/lowrisc_ip/ip/usbdev/README.md)
7. CHERIoT RTOS at the sonata-software v1.3 submodule revision: [`platform-usbdev.hh`](https://github.com/CHERIoT-Platform/cheriot-rtos/blob/21fe9d4775108325e4efe930c427564731d22f00/sdk/include/platform/sunburst/platform-usbdev.hh) and [`sonata-1.1.json`](https://github.com/CHERIoT-Platform/cheriot-rtos/blob/21fe9d4775108325e4efe930c427564731d22f00/sdk/boards/sonata-1.1.json)
8. [Sonata USB device check, v1.3](https://github.com/lowRISC/sonata-system/blob/v1.3/sw/cheri/checks/usbdev_check.cc)
9. [Sonata release procedure, v1.3 (USB checks)](https://github.com/lowRISC/sonata-system/blob/v1.3/doc/dev/release-procedure.md)
10. [Sonata FPGA programming, v1.3 (on-board FT4232H)](https://github.com/lowRISC/sonata-system/blob/v1.3/doc/dev/fpga-programming.md)
11. [Sonata PCB schematic r09 (USB hub, FTDI, RP2040 and User USB PHY sheets)](https://github.com/newaetech/sonata-pcb/blob/main/sonata-one/sonata-schematics-r09.pdf)
12. [Sonata RP2040 firmware](https://github.com/newaetech/sonata-rp2040) and its [TinyUSB class configuration, v1.0.0](https://github.com/newaetech/sonata-rp2040/blob/v1.0.0/usb_msc/tusb_config.h)
13. [Sonata pin multiplexer, v1.3](https://github.com/lowRISC/sonata-system/blob/v1.3/doc/ip/pinmux/README.md)
14. [Sonata FPGA pin constraints, v1.3](https://github.com/lowRISC/sonata-system/blob/v1.3/data/pins_sonata.xdc)
15. [Sonata PWM reference, v1.3](https://github.com/lowRISC/sonata-system/blob/v1.3/doc/ip/pwm.md); counter width and clock in [`sonata_system.sv`](https://github.com/lowRISC/sonata-system/blob/v1.3/rtl/system/sonata_system.sv) and [`top_sonata.sv`](https://github.com/lowRISC/sonata-system/blob/v1.3/rtl/fpga/top_sonata.sv)
16. [sonata-software v1.3 I2C example](https://github.com/lowRISC/sonata-software/blob/v1.3/examples/all/i2c_example.cc)
17. CHERIoT RTOS [Sonata UART driver](https://github.com/CHERIoT-Platform/cheriot-rtos/blob/21fe9d4775108325e4efe930c427564731d22f00/sdk/include/platform/sunburst/platform-uart.hh) and [debug output](https://github.com/CHERIoT-Platform/cheriot-rtos/blob/21fe9d4775108325e4efe930c427564731d22f00/sdk/lib/debug/debug.cc)
18. [FTDI FT4232H datasheet](https://ftdichip.com/wp-content/uploads/2025/11/DS_FT4232H.pdf)
19. USB-IF: [Defined Class Codes](https://www.usb.org/defined-class-codes), [Audio Device Document 1.0](https://www.usb.org/document-library/audio-device-document-10), [USB MIDI Devices 1.0](https://www.usb.org/document-library/usb-midi-devices-10)
20. [Linux ALSA module parameters (`snd-usb-audio`)](https://docs.kernel.org/sound/alsa-configuration.html)
21. [Linux `ftdi_sio` driver](https://github.com/torvalds/linux/blob/master/drivers/usb/serial/ftdi_sio.c)
22. [picocom manual](https://github.com/npat-efault/picocom/blob/master/picocom.1.md)
23. [TinyUSB](https://github.com/hathach/tinyusb)
24. [W3C Web Audio API](https://www.w3.org/TR/webaudio/) and [Chrome autoplay policy](https://developer.chrome.com/blog/autoplay)
25. [WebSerial Standard](https://serial.spec.whatwg.org/)
26. [Chrome: read from and write to a serial port](https://developer.chrome.com/docs/capabilities/serial)
27. [Chromium POSIX serial I/O handler](https://github.com/chromium/chromium/blob/main/services/device/serial/serial_io_handler_posix.cc)
28. [W3C Web MIDI API](https://www.w3.org/TR/webmidi/), [Chrome Web MIDI permission prompt](https://developer.chrome.com/blog/web-midi-permission-prompt), [MDN Web MIDI API](https://developer.mozilla.org/en-US/docs/Web/API/Web_MIDI_API)
29. SparkFun Qwiic Buzzer: [product page](https://www.sparkfun.com/sparkfun-qwiic-buzzer.html), [hardware guide](https://docs.sparkfun.com/SparkFun_Qwiic_Buzzer/single_page/), [library](https://docs.sparkfun.com/SparkFun_Qwiic_Buzzer_Arduino_Library/)
30. Adafruit Audio FX Sound Board: [#2210 product page](https://www.adafruit.com/product/2210), [serial audio control](https://learn.adafruit.com/adafruit-audio-fx-sound-board/serial-audio-control), [#2217 (16 MB)](https://www.adafruit.com/product/2217)
31. Seeed Grove - MP3 v4.0: [product page](https://www.seeedstudio.com/Grove-MP3-V4-p-5862.html), [wiki](https://wiki.seeedstudio.com/grove_mp3_v4/)
32. M5Stack Unit Synth: [documentation with schematic and SAM2695 datasheet links](https://docs.m5stack.com/en/unit/Unit_Synth), [Arduino library](https://github.com/m5stack/M5Unit-Synth)
33. Adafruit VS1053 breakout: [#1381 product page](https://www.adafruit.com/product/1381), [tutorial](https://learn.adafruit.com/adafruit-vs1053-mp3-aac-ogg-midi-wav-play-and-record-codec-tutorial); [VLSI VS1053b datasheet](https://www.vlsi.fi/fileadmin/datasheets/vs1053.pdf)
34. MAX98357A: [SparkFun DEV-14809](https://www.sparkfun.com/sparkfun-i2s-audio-breakout-max98357a.html), [Adafruit #3006](https://www.adafruit.com/product/3006), [datasheet](https://cdn.sparkfun.com/assets/3/5/4/d/5/MAX98357A-MAX98357B.pdf)
35. Digilent Pmod AMP2: [product page](https://digilent.com/shop/pmod-amp2-audio-amplifier/), [reference manual](https://digilent.com/reference/pmod/pmodamp2/reference-manual)
36. [Digilent Pmod I2S2](https://digilent.com/shop/pmod-i2s2-stereo-audio-input-and-output/)
37. [Adafruit STEMMA Speaker #3885](https://www.adafruit.com/product/3885) and [STEMMA technical specs](https://learn.adafruit.com/introducing-adafruit-stemma-qt/technical-specs)
38. [SparkFun Qwiic Connect System](https://www.sparkfun.com/qwiic)
39. [Seeed Grove System](https://wiki.seeedstudio.com/Grove_System/)
40. [SparkFun Qwiic MP3 Trigger DEV-19030 (retired)](https://www.sparkfun.com/sparkfun-qwiic-mp3-trigger-dev-19030.html)
41. [Adafruit Mini External USB Stereo Speaker #3369](https://www.adafruit.com/product/3369)
