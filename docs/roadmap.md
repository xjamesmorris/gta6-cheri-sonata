# Roadmap and TODO

This is a working backlog, not a promise of release dates. GitHub issues track
implementation; release notes describe what has actually shipped.

## Tagged releases

Version 1.0.0 includes CHERIoT fault recovery, protected wallet/road/damage
demonstrations, both splash choices, the source QR, browser indicators and
zebra crossings. Zebra crossings are already implemented, not an outstanding
feature.

Version 1.1.0 adds the following changes. They are not part of the 1.0.0 tag
or its firmware assets, which project policy leaves unchanged; Pages follows
the latest separately approved deployment.

| Work | Tracking | Release status |
| --- | --- | --- |
| Enable the web artwork button only while the splash is visible | [xjamesmorris/gta6-cheri-sonata#1](https://github.com/xjamesmorris/gta6-cheri-sonata/issues/1) | Released in 1.1.0 |
| Add several billboard messages | [xjamesmorris/gta6-cheri-sonata#2](https://github.com/xjamesmorris/gta6-cheri-sonata/issues/2) | Released in 1.1.0 |
| Occasional green frog road crossings | [xjamesmorris/gta6-cheri-sonata#3](https://github.com/xjamesmorris/gta6-cheri-sonata/issues/3) | Released in 1.1.0 |
| Phone touch joystick and portrait/landscape layouts | Owner-requested usability work | Released in 1.1.0 |
| Gentler collision damage and survivable five-star police hits | Owner-requested gameplay tuning | Released in 1.1.0 |
| Deny-by-default audit policy for components, device grants and application imports | Static review follow-up | Released in 1.1.0 |
| Loader, converter and CI hardening; third-party notices and Apache-2.0 headers for files adapted from lowRISC | Static review follow-up | Released in 1.1.0 |

The current frog implementation is decorative and non-colliding: it does not
change score, damage, wanted stars or wallet authority.

- [x] Integrate the three tracked post-release gameplay changes.
- [x] Record native/browser/RTL results and image-specific hardware startup.
- [x] Load the later damage-tuning image with separate authorization and
  record its image-specific startup.
- [x] Publish version 1.1.0 as the next tagged firmware release; the 1.0.0
  tag and firmware assets are unchanged.
- [ ] Complete extended physical playtesting and stress checks, including the
  retuned collision damage.

## Engineering and release hygiene

- [x] Run the firmware build, audit and capability self-test in CI and keep
  their outputs as workflow artifacts.
- [x] Track third-party notices and ship them with firmware and browser
  builds.
- [ ] Refresh the nixpkgs revision used by the preview and CI shells, which
  dates from March 2025, independently of the firmware toolchain pin.
- [ ] Sign release tags and attach build provenance to released UF2 images.
- [ ] Decide whether to enable GitHub's immutable-release setting for future
  releases.
- [ ] Publish redacted hardware transcripts if that can be done without
  device identifiers.

## Audio

No audio backend is shipped in 1.0.0 or 1.1.0. The existing
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
- [x] Add touch controls, labelled buttons and compact portrait/landscape layouts
  without changing the board's control scheme.
- [ ] Broaden phone, tablet and accessibility playtesting, including real
  devices and assistive technologies.
- [ ] Keep the [screenshots and video](screenshots.md), controls, build
  instructions and verification records aligned with each release.

The removed attract/demo mode is not on the current roadmap. New hardware
drivers, persistent state and stronger security claims require their own
design and validation rather than being implied by a UI change.

[Back to the README](../README.md)
