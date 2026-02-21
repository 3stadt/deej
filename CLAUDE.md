# Claude Code context for deej (nd fork)

This file is read automatically by [Claude Code](https://claude.ai/claude-code) at the start of every session. It serves as project memory and documents the AI-assisted development history of this fork.

## What this project is

[deej](https://github.com/omriharel/deej) is an open-source hardware volume mixer: an Arduino sends potentiometer (slider) values over serial, and a Go program on the PC maps those to application/device volumes via the Windows Core Audio API (or PulseAudio on Linux).

This is a personal fork. The original upstream is [omriharel/deej](https://github.com/omriharel/deej). A maintained community fork is [nicholasgasior/deej](https://github.com/nicholasgasior/deej) (the basis for the releases linked in this repo's README).

## Changes made in this fork vs upstream

### 1. Mute buttons (added 2025-02)

Adds support for up to 5 digital mute-toggle buttons wired to the Arduino alongside the existing sliders.

**Arduino side** (`arduino/deej-5-sliders-vanilla/deej-5-sliders-5-buttons.ino`):
- Reads 5 analog sliders + 5 digital buttons
- Sends a combined serial line: `s0|s1|s2|s3|s4|b0|b1|b2|b3|b4\r\n`
- Button values are 1 on rising edge (press), 0 otherwise — reset each serial cycle

**Go side**:
- `num_buttons` config key tells the parser how many trailing values are buttons vs sliders
- `button_mapping` config key maps button indices to the same targets as `slider_mapping`
- Each button press reads the current mute state from the OS and toggles it (dynamic, not tracked internally)
- Uses `ISimpleAudioVolume::GetMute/SetMute` for app sessions and `IAudioEndpointVolume::GetMute/SetMute` for master/device sessions
- Linux mute is stubbed out (no-op) — PulseAudio implementation not yet done

### 2. Dependency and reliability updates (pre-existing in this fork's base)

- Go updated to 1.23.7
- Various dependency updates
- Improved serial connection reliability on Windows
- Better error handling

## Codebase architecture (Go)

All Go source lives in `pkg/deej/`. Key files:

| File | Role |
|------|------|
| `serial.go` | Reads serial port, parses slider + button values, emits events |
| `session.go` | `Session` interface (`GetVolume`, `SetVolume`, `GetMute`, `SetMute`, `Key`, `Release`) |
| `session_windows.go` | Windows implementations: `wcaSession` (apps, via `ISimpleAudioVolume`) and `masterSession` (devices, via `IAudioEndpointVolume`) |
| `session_linux.go` | Linux implementations via PulseAudio (`jfreymuth/pulse`) |
| `session_finder_windows.go` | Enumerates all active audio sessions via Windows Core Audio COM API |
| `session_map.go` | Maps sessions to config targets; handles slider move and button press events |
| `config.go` | Loads and watches `config.yaml`; exposes `CanonicalConfig` |
| `slider_map.go` | `sliderMap` type (used for both `slider_mapping` and `button_mapping`) |
| `deej.go` | Top-level orchestrator |

### Data flow: sliders

```
Arduino → serial line → SerialIO.handleLine()
  → processSliderValues() → SliderMoveEvent
  → sessionMap.handleSliderMoveEvent()
  → Session.SetVolume()
```

### Data flow: buttons

```
Arduino → serial line → SerialIO.handleLine()
  → processButtonValues() → ButtonPressEvent (only when value == 1)
  → sessionMap.handleButtonPressEvent()
  → Session.GetMute() [reads OS state]
  → Session.SetMute(!currentMute)
```

### Serial line format

`<slider0>|<slider1>|...|<sliderN>|<button0>|...|<buttonM>\r\n`

The split point is determined by `num_buttons` in config. If `num_buttons` is 0 (default), the entire line is treated as sliders (backward compatible).

### Config keys

| Key | Type | Description |
|-----|------|-------------|
| `slider_mapping` | map[int][]string | Slider index → app/device targets |
| `button_mapping` | map[int][]string | Button index → app/device targets (mute toggle) |
| `num_buttons` | int | How many trailing serial values are buttons (default 0) |
| `com_port` | string | Serial port (default COM4) |
| `baud_rate` | int | Baud rate (default 9600) |
| `invert_sliders` | bool | Invert slider direction |
| `noise_reduction` | string | `low` / `default` / `high` |

### Special mapping targets (same for sliders and buttons)

- `master` — default output device master volume
- `mic` — default input device level
- `system` — system sounds (Windows only)
- `deej.current` — currently focused app (Windows only)
- `deej.unmapped` — all apps not mapped to any slider
- `"Speakers (Realtek ...)"` — specific audio device by friendly name

## Coding conventions

- Match the existing style in each file (tabs, zap structured logging, etc.)
- Use `s.logger.Warnw(...)` for recoverable errors, `Debugw` for normal operation
- Error sentinel `errRefreshSessions` signals the session map to force-refresh
- Always run `go build ./...` after changes to verify compilation
- Platform-specific code goes in `_windows.go` / `_linux.go` files
- The `Session` interface must be satisfied by all platform implementations — add stubs to `session_linux.go` whenever the interface grows

## AI development notes

Changes in this fork were developed with assistance from **Claude** (Anthropic) via Claude Code. The AI was used for:
- Architecture design and planning
- Writing Go code for the mute button feature
- Explaining existing code and dependencies

All AI-generated code was reviewed before commit. The human developer retains full understanding and ownership of the changes.
