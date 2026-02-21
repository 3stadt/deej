package util

import (
	"errors"
)

// TODO: implement getCurrentWindowProcessNames for Linux.
//
// Goal: return the process name(s) of the currently focused window, so that
// the "deej.current" mapping target works on Linux the same way it does on Windows.
//
// The fundamental bridge is the PID: the compositor knows which window (and its PID)
// is focused; PulseAudio/PipeWire knows which sink input belongs to which PID
// (via the "application.process.id" stream property). There is no direct link
// between the two subsystems, so we must go through PID → process name.
//
// Recommended implementation strategy:
//
//  1. X11 / XWayland (DISPLAY is set):
//     Use EWMH via a pure-Go XCB library (e.g. github.com/jezek/xgb or BurntSushi/xgb).
//     - Read _NET_ACTIVE_WINDOW from the root window to get the focused window XID.
//     - Read _NET_WM_PID from that window to get its PID.
//     - Resolve PID → process name via /proc/<pid>/comm or go-ps (already a dep).
//     This covers X11 sessions and apps running under XWayland on any compositor.
//
//  2. Wayland (WAYLAND_DISPLAY is set, no DISPLAY):
//     There is no universal Wayland protocol for querying the focused window from
//     outside the compositor. Use compositor detection via $XDG_CURRENT_DESKTOP or
//     $XDG_SESSION_DESKTOP and dispatch to compositor-specific helpers:
//
//     a) wlroots-based (Sway, Hyprland, Wayfire, etc.) — via wlr-foreign-toplevel-management:
//     The protocol exposes app_id + focus state for all toplevels as a Wayland
//     client, but does NOT provide PID. For Sway and Hyprland, prefer the IPC:
//     - Sway: connect to $SWAYSOCK, send `{"type":4}` (GET_TREE), walk the tree
//     for focused:true, extract pid.
//     - Hyprland: run `hyprctl activewindow -j`, parse JSON field "pid".
//     Both give a PID directly, which is cleaner than the protocol approach.
//
//     b) KDE/KWin: use D-Bus.
//     org.kde.KWin / /KWin scripting or org.kde.ActivityManager may expose the
//     active window. Alternatively, KWin scripts can be loaded at runtime.
//     A simpler approach may be the KWin scripting console via qdbus:
//     `qdbus org.kde.KWin /KWin queryWindowInfo` or similar — needs investigation.
//     Fallback: `xdotool` via XWayland if the KDE session runs XWayland apps.
//
//     c) GNOME/Mutter: no supported external API for the focused window on Wayland.
//     Mutter intentionally does not expose this. Options are limited:
//     - Use XWayland + EWMH if DISPLAY is also set (many GNOME apps still use it).
//     - Shell extension (not viable for a background daemon).
//     - Return nil, nil (silent no-op) and document the limitation.
//
//  3. Fallback:
//     If no compositor-specific method succeeds, return nil, nil (not an error)
//     so that deej simply skips deej.current without logging scary errors.
//     Only return a real error for genuine failures (e.g. IPC socket found but unreadable).
//
//  4. Caching:
//     Like the Windows implementation, apply a short cooldown (e.g. 350ms) and
//     return a cached result to avoid hammering compositor IPC on every serial tick.
//
// Dependencies to consider:
//   - github.com/jezek/xgb — pure-Go X11/XCB (X11 path, no cgo)
//   - encoding/json — for Hyprland/Sway IPC JSON parsing (already in stdlib)
//   - net — for Sway Unix socket IPC (already in stdlib)
//   - os/exec — for shelling out to hyprctl / qdbus as a last resort
//   - github.com/mitchellh/go-ps — already a transitive dep via Windows build
func getCurrentWindowProcessNames() ([]string, error) {
	return nil, errors.New("not implemented on Linux")
}
