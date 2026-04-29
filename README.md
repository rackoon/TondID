# TondID

![TondID wordmark](./assets/tondid-wordmark.svg)

TondID is a private tooling stack for identity, presence, and protection testing on ESP32-class hardware. It packages a modern local configurator, persistent firmware profiles, mock swarm behavior, and the supporting material needed to run repeatable QA sessions without depending on cloud services.

## What TondID is for

TondID is meant to sit behind systems that need to observe, verify, or harden against moving assets with a transmitted identity layer. The protected object does not have to be a single vehicle type. The stack is intentionally broad enough to support testing around drones, rail, aviation, heavy equipment, and other protected platforms where presence and identity matter.

## Current repository contents

- `configurator/` - Chrome/WebSerial control surface for local configuration over USB Type-C
- `firmware/RemoteIDModule/` - ESP32-C3 oriented firmware with persistent profile and runtime state
- `firmware/libraries/` - bundled firmware dependencies
- `docs/` - screenshots, notes, and operating references
- `scripts/qa-flash.ps1` - Windows helper for repeatable flashing
- `assets/` - brand marks, monograms, and presentation assets
- `site/` - static public landing package for domain deployment

## Product principles

- local-first operation
- persistent on-device configuration
- hardware-friendly setup over existing USB Type-C
- configurable mock identity, movement, and swarm behavior
- clean operator workflow through a single browser-based control surface

## Quick start

1. Open the configurator from `configurator/`.
2. Connect to the board in Chrome over WebSerial.
3. Adjust profile, identity, ghost count, movement mode, and altitude mode.
4. Apply settings so they persist to on-device storage.
5. Run and validate behavior against the target detector.

## Branding

Primary name:
- `TondID`

Compact form:
- `TID`

Brand usage rule:
- use `TondID` wherever there is enough room to show the full product name
- use `TID` in constrained placements such as badges, app icons, print labels, and tight UI surfaces

Primary brand assets:
- `assets/tondid-wordmark.svg`
- `assets/tondid-mark.svg`
- `assets/tid-monogram.svg`

## Domains

Reserved domains:
- `tondid.com`
- `tondid.eu`

Recommended production mapping:
- `tondid.com` - primary public product site
- `www.tondid.com` - redirect to `tondid.com`
- `tondid.eu` - EU-facing redirect to `tondid.com` until separate regional content exists
- `app.tondid.com` - future public-facing app or customer portal if needed
- `docs.tondid.com` - future documentation surface if the docs split from the marketing site

See [docs/DOMAINS.md](./docs/DOMAINS.md) for the domain binding plan.

## Notes

This repository is the assembled private TondID worktree and contains the configurator, firmware, supporting scripts, screenshots, and brand material needed for ongoing product development and QA.
