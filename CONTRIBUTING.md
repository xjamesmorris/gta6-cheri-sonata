# Contributing

This is a small personal project. Issues and pull requests are welcome, with
no promise of review time or that a change will be accepted.

## Before you start

- Read the [LLM contribution policy](LLM-POLICY.md). It applies to code,
  tests, documentation, reviews and commit messages.
- Report security problems privately, as described in
  [SECURITY.md](SECURITY.md).
- New hardware drivers, device authority, persistent state or stronger
  security claims need their own design and validation first; see the
  [roadmap](docs/roadmap.md).

## Checks

The pinned Nix flake supports x86_64 Linux. Run these before proposing a
change:

```sh
./tools/check
./tools/check-browser
./tools/sim-test cheri-selftest
```

`tools/check` builds and audits the firmware and runs the tool, policy and
native tests. `tools/check-browser` builds the preview and runs the browser
tests. Run the `cheri-selftest` simulator fixture for any change to the
firmware protection layer.

The audit policy is deny-by-default. A change that adds a compartment, a
device grant, an export, or any import to an application component fails the
audit until `policy/gta6_wallpaper.rego` changes too. That includes a new
library call, such as a compiler helper. `tools/check` names the rejecting
rules. Explain the policy change and add a negative test for it.

Do not weaken a guard, the audit policy or a test to make a change pass, and
do not load hardware as part of checking a contribution.

## Licensing and provenance

- A contribution is licensed under the license of the file it changes: MIT
  unless the file header says otherwise. Files adapted from lowRISC's Sonata
  software are Apache-2.0, and the artwork and its generated data are
  CC BY 4.0.
- Give every new source file an `SPDX-License-Identifier` line.
- Contribute only code, artwork, fonts and text that you have the right to
  contribute.
- If you adapt third-party material, keep its copyright and license notices,
  keep the file under a license compatible with its source, say in the header
  what you changed, and add the work to
  [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).
- New artwork needs an explicit license and attribution in
  [artwork/README.md](artwork/README.md).

## Commits

Describe what changed and how it was checked. Add `Assisted-by: LLM` for
substantive LLM assistance. Only a human may add their own `Signed-off-by`.
