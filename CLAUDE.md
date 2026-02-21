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
- Uses `ISimpleAudioVolume::GetMute/SetMute` for app sessions and `IAudioEndpointVolume::GetMute/SetMute` for master/device sessions on Windows
- Linux mute uses PulseAudio via `jfreymuth/pulse`: `SetSinkInputMute`/`GetSinkInputInfo` for app sessions, `SetSinkMute`/`SetSourceMute` and `GetSinkInfo`/`GetSourceInfo` for master/device sessions

### 2. LED mute indicators (added 2026-02)

Extends the mute button feature with per-button LED indicators that reflect the mute state of the configured target in real time — including changes made externally via the Windows volume mixer or by an application muting itself.

**Arduino side** (`arduino/deej-5-sliders-vanilla/deej-5-sliders-5-buttons.ino`):
- Receives LED commands from the Go program over the same serial connection used for slider/button data
- Command format: `L<pin>,<state>\n` where state is `0` (off), `1` (on), or `2` (blink)
- Non-blocking character-by-character parsing accumulates commands while the main loop continues normally
- Blink state is driven locally by the Arduino using `millis()` at 500 ms intervals — Go only sends the state once when it changes, not on every blink

**Go side**:
- `led_mapping` config key maps button index → Arduino digital pin number
- `LEDState` type defined in `serial.go`: `LEDOff` (0), `LEDOn` (1), `LEDBlink` (2)
- `SerialIO.SendLEDCommand(pin, state)` writes `L<pin>,<state>\n` to the serial port under a write mutex (reads and writes share the same `conn` but are thread-safe via `writeMu`)
- `SerialIO` emits a connect event via `SubscribeToConnectEvents()` after each successful `connect()` call; `sessionMap` subscribes and re-syncs all LEDs so a freshly reset Arduino gets the correct state
- A 500 ms poller in `sessionMap` calls `computeLEDState(buttonIdx)` for each mapped button and sends a command only when the state has changed, keeping the serial port quiet
- LED state semantics: **solid on** = all targets muted; **solid off** = all targets unmuted or no active sessions; **blinking** = mixed state (some targets muted, some not — indicates pressing the button will produce an asymmetric result)
- `lastKnownLEDState map[int]LEDState` is protected by `ledStateMu sync.Mutex` against concurrent access from the poller goroutine, the connect handler, and `handleButtonPressEvent`

### 3. Dependency and reliability updates (pre-existing in this fork's base)

- Go updated to 1.23.7
- Various dependency updates
- Improved serial connection reliability on Windows
- Better error handling

## Codebase architecture (Go)

All Go source lives in `pkg/deej/`. Key files:

| File | Role |
|------|------|
| `serial.go` | Reads serial port, parses slider + button values, emits events; writes LED commands; defines `LEDState` type |
| `session.go` | `Session` interface (`GetVolume`, `SetVolume`, `GetMute`, `SetMute`, `Key`, `Release`) |
| `session_windows.go` | Windows implementations: `wcaSession` (apps, via `ISimpleAudioVolume`) and `masterSession` (devices, via `IAudioEndpointVolume`) |
| `session_linux.go` | Linux implementations via PulseAudio (`jfreymuth/pulse`) |
| `session_finder_windows.go` | Enumerates all active audio sessions via Windows Core Audio COM API |
| `session_map.go` | Maps sessions to config targets; handles slider move and button press events; drives LED sync poller; defines shared error sentinels (`errNoSuchProcess`, `errRefreshSessions`) |
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
  → sessionMap.syncAllLEDs()
  → SerialIO.SendLEDCommand(pin, state)
  → Arduino LED pin
```

### Data flow: LED sync (external mute changes)

```
sessionMap.setupMuteStatePoller() [500 ms ticker]
  → syncAllLEDs()
  → computeLEDState(buttonIdx) → Session.GetMute() for each target
  → if state changed: SerialIO.SendLEDCommand(pin, state)
  → Arduino LED pin

sessionMap.setupOnConnect() [on serial reconnect]
  → reset lastKnownLEDState
  → syncAllLEDs() [forces re-send to freshly reset Arduino]
```

### Serial line format

`<slider0>|<slider1>|...|<sliderN>|<button0>|...|<buttonM>\r\n`

The split point is determined by `num_buttons` in config. If `num_buttons` is 0 (default), the entire line is treated as sliders (backward compatible).

### Config keys

| Key | Type | Description |
|-----|------|-------------|
| `slider_mapping` | map[int][]string | Slider index → app/device targets |
| `button_mapping` | map[int][]string | Button index → app/device targets (mute toggle) |
| `led_mapping` | map[int]int | Button index → Arduino digital pin for the indicator LED |
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
- Error sentinels `errNoSuchProcess` and `errRefreshSessions` are defined in `session_map.go` and shared across platform implementations; `errRefreshSessions` signals the session map to force-refresh
- Always run `go build ./...` after changes to verify compilation
- Platform-specific code goes in `_windows.go` / `_linux.go` files
- The `Session` interface must be satisfied by all platform implementations — add stubs to `session_linux.go` whenever the interface grows

## AI development notes

Changes in this fork were developed with assistance from **Claude** (Anthropic) via Claude Code. The AI was used for:
- Architecture design and planning
- Writing Go code for the mute button and LED indicator features
- Explaining existing code and dependencies

All AI-generated code was reviewed before commit. The human developer retains full understanding and ownership of the changes.
