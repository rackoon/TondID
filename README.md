# TondID

![TondID logo](./assets/TondID_logo.svg)

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
- configurable identity, movement, and per-slot behavior
- clean operator workflow through a single browser-based control surface

## Quick start

1. Start the local configurator and open `http://localhost:3000`.
2. Connect to the board in Chrome over WebSerial.
3. Adjust primary identity, swarm defaults, tuning values, and slot-level settings.
4. Enable exactly the slots you want active.
5. Apply settings so they persist to on-device storage.
6. Use `Fly` / `Pause` and validate behavior against the target detector.

## Current operating model

- maximum of **5 total slots**
- each slot has its own `ON/OFF` state
- each slot can carry its own:
  - `UAS ID`
  - `Operator ID`
  - `Self ID`
  - `Lab Manufacturer`
  - `Lab Model`
  - `MAC`
  - `UA Type`
  - `ID Type`
- profile changes persist to board storage
- the board can resume from the last applied profile after power loss

## Detector-development note

TondID now distinguishes between:

- **detector-visible standard fields**
  - `UAS ID`
  - `ID Type`
  - `UA Type`
  - `Operator ID`
  - `Self ID`
- **lab metadata**
  - `Lab Manufacturer`
  - `Lab Model`

For detector development workflows, the configurator can export a per-slot lab mapping keyed by `UAS ID` and `MAC` so your own detector can display the exact labels you assign in the UI.

## Branding

Primary name:
- `TondID`

Compact form:
- `TID`

Brand usage rule:
- use `TondID` wherever there is enough room to show the full product name
- use `TID` in constrained placements such as badges, app icons, print labels, and tight UI surfaces

Primary brand assets:
- `assets/TondID_logo.svg`

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

## Documentation

- [BUILDING.md](./BUILDING.md) - current Windows build and run workflow
- [docs/serial.md](./docs/serial.md) - active serial protocol and slot round-trip behavior
- [docs/DOMAINS.md](./docs/DOMAINS.md) - domain binding plan

## Notes

This repository is the assembled private TondID worktree and contains the configurator, firmware, supporting scripts, screenshots, and brand material needed for ongoing product development and QA.

