# Building TondID

This repository carries the TondID configurator, firmware, assets, and supporting material used in the current Windows-based development workflow.

## Current local workflow

The active day-to-day setup is:

- Windows
- Chrome / Chromium with WebSerial
- ESP32-C3 connected over the existing USB Type-C port
- local configurator served on `http://localhost:3000`

The user-facing control path is:

1. start the local configurator
2. connect to the board over WebSerial
3. configure slots and runtime behavior
4. apply settings so they persist to board storage
5. use `Fly` / `Pause` and the board `BOOT` button for runtime control

## Repository structure

- `configurator/` - source for the local React/WebSerial configurator
- `firmware/RemoteIDModule/` - ESP32 firmware
- `firmware/libraries/` - bundled dependencies used by the firmware build
- `docs/` - protocol notes, screenshots, and operating references
- `scripts/` - helper scripts such as flashing
- `site/` - static site package

## Configurator build

From the configurator workspace:

```powershell
cd C:\QA_Tools\TondID\configurator
npm install
npm run build
```

For the active localhost development copy used in the current rig:

```powershell
cd C:\QA_Tools\MockBeaconRig\web
npm install
npm run build
```

## Local configurator dev server

The localhost app is typically served from the active web workspace:

```powershell
cd C:\QA_Tools\MockBeaconRig\web
$env:BROWSER='none'
$env:PORT='3000'
npm start
```

Then open:

- `http://localhost:3000`

If the page appears stale, do a hard refresh:

- `Ctrl+F5`

## Firmware build

The current working firmware tree is:

- `C:\QA_Tools\ArduRemoteID_HEAD\RemoteIDModule`

Typical compile command:

```powershell
& 'C:\QA_Tools\arduino-cli\arduino-cli.exe' compile `
  --fqbn 'esp32:esp32:esp32c3:PartitionScheme=min_spiffs,FlashMode=dio,FlashFreq=40,UploadSpeed=115200,CDCOnBoot=cdc' `
  --build-property 'compiler.cpp.extra_flags=-DBOARD_ESP32C3_DEV -DESP32' `
  --libraries 'C:\QA_Tools\ArduRemoteID_HEAD\libraries' `
  --build-path 'C:\QA_Tools\ArduRemoteID_HEAD\build-c3' `
  'C:\QA_Tools\ArduRemoteID_HEAD\RemoteIDModule'
```

Typical upload command:

```powershell
& 'C:\QA_Tools\arduino-cli\arduino-cli.exe' upload `
  -p COM6 `
  --fqbn 'esp32:esp32:esp32c3:PartitionScheme=min_spiffs,FlashMode=dio,FlashFreq=40,UploadSpeed=115200,CDCOnBoot=cdc' `
  --build-path 'C:\QA_Tools\ArduRemoteID_HEAD\build-c3' `
  'C:\QA_Tools\ArduRemoteID_HEAD\RemoteIDModule'
```

## Persistence model

Profile changes written through the configurator are persisted to board storage.

Persisted items include:

- primary identity values
- per-slot values
- slot on/off state
- movement defaults
- tuning values
- runtime mode state used by the current firmware

This allows the board to boot with the last applied profile without needing cloud services.

## Known practical notes

- the project currently assumes a maximum of **5 total slots**
- slot enablement is controlled per slot, not through a separate count field
- `Lab Manufacturer` / `Lab Model` are local metadata for lab and detector-development workflows
- third-party detectors may infer manufacturer/model from `UAS ID` rather than from local metadata fields

## Related docs

- [README.md](./README.md)
- [docs/serial.md](./docs/serial.md)
- [docs/DOMAINS.md](./docs/DOMAINS.md)
