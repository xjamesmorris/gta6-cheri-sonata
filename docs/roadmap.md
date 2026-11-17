# Roadmap and TODO

This is a working backlog, not a promise of release dates. GitHub issues track
implementation; release notes describe what has actually shipped.

## Tagged release and current main

Version 1.0.0 includes CHERIoT fault recovery, protected wallet/road/damage
demonstrations, both splash choices, the source QR, browser indicators and
zebra crossings. Zebra crossings are already implemented, not an outstanding
feature.

The following changes are implemented on `main` after 1.0.0. They are not
part of the immutable 1.0.0 tag or its firmware assets; Pages follows the
latest separately approved deployment.

| Work | Tracking | Release status |
| --- | --- | --- |
| Enable the web artwork button only while the splash is visible | [xjamesmorris/gta6-cheri-sonata#1](https://github.com/xjamesmorris/gta6-cheri-sonata/issues/1) | Implemented on main |
| Add several billboard messages | [xjamesmorris/gta6-cheri-sonata#2](https://github.com/xjamesmorris/gta6-cheri-sonata/issues/2) | Implemented on main |
| Occasional green frog road crossings | [xjamesmorris/gta6-cheri-sonata#3](https://github.com/xjamesmorris/gta6-cheri-sonata/issues/3) | Implemented on main |

The current frog implementation is decorative and non-colliding: it does not
change score, damage, wanted stars or wallet authority.

- [x] Integrate the three tracked post-release gameplay changes.
- [x] Record native/browser/RTL results and image-specific hardware startup.
- [ ] Complete extended physical playtesting and stress checks.
- [ ] Select and publish the next tagged firmware release when approved; keep
  the 1.0.0 tag and firmware assets unchanged.

## Audio

No audio backend is shipped in 1.0.0. The existing
[audio-options research](audio-options.md) covers browser, host-relay, USB
and external-module possibilities.

- [ ] Establish a small sound palette for pickups, damage, police, WASTED
  and billing; use original or appropriately licensed assets.
- [ ] Prototype browser audio with user-initiated enablement, mute/volume
  controls and appropriate pause/focus behavior.
- [ ] Prototype board-to-host audio events by following the operator's
  existing console log, not by opening a competing serial reader.
- [ ] Measure event latency, logging overhead and event-rate limits.
- [ ] Select any standalone buzzer/sample-player hardware only after checking
  wiring, power, logic levels, peripheral ownership and audit changes.
- [ ] Treat native USB MIDI/audio as a separate feasibility investigation,
  not an assumed feature of the current firmware.

## Earned capabilities and level unlocks

**Suggested by Jacob in the CHERIoT public chat.**

Explore earning capabilities through gameplay and using that authority to
unlock additional levels or challenges. This is a proposed design direction,
not an implemented feature or a claim that the current score already grants
hardware-enforced level access.

- [ ] Define what a level capability authorizes and how it is earned.
- [ ] Design the trusted issuer and level-entry boundary; investigate an
  opaque/sealed grant rather than relying only on a writable unlock flag.
- [ ] Specify grant lifetime across runs, resets and respawns before deciding
  whether saved progression is needed.
- [ ] Add positive and negative tests for legitimate entry, missing/invalid
  authority and attempts to obtain broader access.
- [ ] Keep the browser backend explicitly emulated, and verify any firmware
  claim with real CHERIoT evidence.
- [ ] Review compartment permissions, call depth, stack and memory budgets;
  do not broaden authority simply to make the feature work.

## Quality, artwork and usability

- [ ] Prepare an exact 128x160 derivative splash to avoid reliance on cropping
  and downsampling; retain attribution and reproducible RGB565 conversion.
- [ ] Run longer on-board play/stress sessions, including repeated fault
  recovery, joystick behavior, LCD timing and frame pacing.
- [ ] Record the exact image digest and measured results for each hardware
  check; do not treat simulator success as physical acceptance.
- [ ] Playtest traffic, wanted-star and billing pacing while preserving the
  documented CHERI demonstrations.
- [ ] Consider touch controls and accessibility refinements for smaller
  browser screens without changing the board's control scheme.
- [ ] Keep the [screenshots and video](screenshots.md), controls, build
  instructions and verification records aligned with each release.

The removed attract/demo mode is not on the current roadmap. New hardware
drivers, persistent state and stronger security claims require their own
design and validation rather than being implied by a UI change.

[Back to the README](../README.md)
