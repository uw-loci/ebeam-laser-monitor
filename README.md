# EBEAM Laser Monitor - Radiation and Beams Indicator

Arduino Uno firmware for the EBEAM dashboard Laser Monitor indicator.

The Arduino receives simple USB serial commands from the dashboard, drives two digital output pins, and replies to each valid command so the dashboard can detect whether communication is alive.

## Firmware Workflow

1. On startup, configure the indicator pins as outputs and drive both LOW.
2. Start USB serial communication at `9600` baud.
3. Enable the AVR hardware watchdog with an approximately 8 second reset window.
4. Continuously read newline-delimited serial commands without blocking.
5. Reply to dashboard `PING` commands with `PONG`.
6. Apply valid `STATE` commands atomically to both indicator outputs.
7. If no valid dashboard message is received for 4 seconds, force only the beams-on output LOW.
8. Leave the last radiation-indicator state unchanged during dashboard serial timeout, malformed commands, or serial silence.

## Pinout

| Signal | Arduino Uno Pin | False State | True State |
| --- | ---: | --- | --- |
| Radiation indicator | `D2` | `LOW` / 0 V | `HIGH` / 5 V |
| Beams on | `D8` | `LOW` / 0 V | `HIGH` / 5 V |

## Serial Settings

The dashboard should open the Arduino USB serial port with:

| Setting | Value |
| --- | --- |
| Baud rate | `9600` |
| Data bits | `8` |
| Parity | `None` |
| Stop bits | `1` |
| Line ending | `\n` |
| Encoding | ASCII |

The dashboard driver is expected to poll the Arduino regularly. The current dashboard driver sends `STATE` every 500 ms and whenever either indicator boolean changes.

## Communication Protocol

All messages are ASCII and line-delimited with `\n`. The firmware accepts an optional `\r` before `\n`.

### Ping

Dashboard sends:

```text
PING
```

Arduino replies:

```text
PONG
```

### State Update

Dashboard sends:

```text
STATE beams=<0|1> radiation=<0|1>
```

Examples:

```text
STATE beams=0 radiation=0
STATE beams=1 radiation=0
STATE beams=0 radiation=1
STATE beams=1 radiation=1
```

Arduino behavior:

- Valid command: update both output pins, then reply `OK`.
- Invalid or malformed command: leave outputs unchanged, then reply `ERR`.
- Oversized command line: leave outputs unchanged, then reply `ERR` when the line terminates.

## Watchdogs

Two watchdog behaviors are implemented:

- Hardware watchdog: if firmware execution stalls for about 8 seconds, the AVR watchdog resets the Arduino.
- Dashboard communication watchdog: if no valid `PING` or `STATE` command is received for 4 seconds, the firmware sets `beams` LOW while preserving the last known `radiation` output.

## Build

This project is configured for PlatformIO:

```powershell
pio run
```
