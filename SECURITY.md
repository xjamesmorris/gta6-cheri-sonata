# Security policy

Grand Time Auto is a personal demonstration project maintained in spare time.
It is not a product, and its "wallet" is a game score. Security reports are
still welcome.

## Supported versions

Fixes are made on `main` and ship in the next tagged release. Earlier releases
are not patched, and their tags and assets are left unchanged.

## Scope

In scope:

- the firmware protection layer: the wallet and probe compartments, fault
  classification and the audit policy;
- the host tools, especially `tools/load-slot2`, the UF2 validator and the
  image converter;
- the browser preview and the GitHub Actions workflows.

Out of scope here, and better reported to their own projects: CHERIoT RTOS,
the Sonata system and its RP2040 firmware, the display drivers, Emscripten,
Nix packages and GitHub Actions. The browser build deliberately **emulates**
protection and makes no security claim.

[Architecture and verification boundaries](docs/architecture.md) describe the
threat model: trusted game code keeps legitimate wallet authority, and the
audit policy can be changed by anyone who can change the sources.

## Reporting a vulnerability

Open a normal
[issue](https://github.com/xjamesmorris/gta6-cheri-sonata/issues), or email
the maintainer at [jmorris@namei.org](mailto:jmorris@namei.org). Issues are
public; email reaches only the maintainer.

A useful report names the commit or release, the affected file, what an
attacker must control, and the impact you observed or expect. Say whether
anything was run on hardware.

Replies may take a couple of weeks. There is no bug bounty.

## Fixes

Fixes pass the same build, audit and test gates as any other change. Reports
and fixes prepared with LLM assistance are welcome under the
[LLM contribution policy](LLM-POLICY.md): check the claim yourself first, and
say what was and was not tested.
