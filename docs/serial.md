# TondID Serial Protocol

TondID uses a simple line-based serial protocol over the board's USB serial path.

The current configurator uses:

- WebSerial in Chrome / Chromium
- `115200` baud
- line-delimited commands terminated with `CRLF`

## Transport notes

- commands are sent as ASCII lines
- events are returned as ASCII lines
- each message starts with `$`
- fields are delimited with `|`

Example:

```text
$SS|1|1|AA:BB:CC:DD:EE:01|1596ABCDEF0123456789|EST123|TRACK 1|LAB|MODEL|2|1
```

## Command overview

| Command | Meaning |
| --- | --- |
| `$V` | request firmware version |
| `$D` | request current persisted profile/default data |
| `$C` | request current runtime position/state |
| `$GS|<slot>` | request one slot payload |
| `$SD|...` | store profile/default data |
| `$SS|...` | store one slot payload |
| `$SM|...` | store runtime mode |
| `$R` | reboot |

## Response overview

| Response | Meaning |
| --- | --- |
| `$V|...` | firmware version |
| `$D|...` | persisted default/profile payload |
| `$C|...` | current runtime state |
| `$G|...` | one slot payload |
| `$T|...` | live mock telemetry line |
| `$%` | success / acknowledgement |
| `$-` | invalid request / out-of-range slot |

## `$D` profile payload

`$D` returns the board's current persisted profile/default state. The configurator uses this to repopulate the profile screen.

Relevant values include:

- primary `UAS ID`
- `Operator ID`
- `Self ID`
- `UA Type`
- `ID Type`
- origin/operator coordinates
- speed / satellites / MAC
- app mode
- pest origin and radius
- total enabled slot count
- lab manufacturer / lab model defaults
- ghost ID / movement / altitude defaults

## `$G` slot payload

Per-slot round-trip payload:

```text
$G|slot|enabled|mac|uas_id|operator_id|self_id|lab_manufacturer|lab_model|ua_type|id_type
```

This is the payload the configurator uses to confirm that each slot actually persisted to the board.

## `$SS` slot write payload

Per-slot write command:

```text
$SS|slot|enabled|mac|uas_id|operator_id|self_id|lab_manufacturer|lab_model|ua_type|id_type
```

The current configurator writes slots sequentially and then reads them back to verify the board stored them correctly.

## `$T` telemetry payload

The board emits live mock telemetry while active:

```text
$T|lat|lng|alt|spd|hdg|mac|uas_id|operator_id|self_id|ua_type|id_type|fly_mode|lab_manufacturer|lab_model
```

The configurator uses this for live plotting and slot display.

## Current configurator expectations

The active TondID configurator assumes:

- maximum of **5 total slots**
- slot enablement is per slot
- profile `Apply` writes defaults first, then each slot, then reads back board state
- slot mismatch after apply is treated as a real sync problem, not ignored

## Practical debugging note

If the UI shows:

- `NO SERIAL RX`

then the browser is connected to a serial port but is not receiving valid serial lines back. In that state, `Fly` behavior in the UI cannot be trusted until `$D`, `$C`, `$G`, or `$T` traffic resumes.
