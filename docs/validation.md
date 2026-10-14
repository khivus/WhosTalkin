# Executed validation

Date: **2026-10-14**. Windows x64 workspace: `D:\Git\WhosTalkin`.

## Build

A Release C++17 build completed using MSVC 19.29.30159 / v142 (tools directory
14.29.30133), Windows SDK 10.0.19041.0, CMake 3.31.8, official API 26 SDK commit
`4aa90a53aa150cbf81e13bc97e68c0431b26499f`, and Qt 5.15.2 msvc2019_64.
The existing Visual Studio installation lacked the C++ workload. Local compiler
and SDK downloads were used; Windows SDK MSI extraction failed with error 1603,
so its downloaded CAB files and MSI file tables were extracted directly without
installing the SDK. All build tooling resides in the ignored `.cache` directory.

Created artifacts:

- `build/whostalkin_win64.dll` and its matching copy in `dist`.
- `dist/WhosTalkin-1.1.1-win64.ts3_plugin`, a ZIP with `package.ini`, the plugin
  under `plugins/`, and its MIT license in `plugins/whostalkin/`.
- `dist/SHA256SUMS.txt` for the DLL and installer, and `dist/LICENSE.txt`.
- `build/whostalkin_simulator.exe` (development Qt kit needed on PATH).
- `docs/active-speakers.png`, captured from the actual shared UI in the simulator
  with three mocked speakers. It is not a screenshot of a live TS3 audio session.

`dumpbin /dependents` confirmed Qt5Core/Gui/Widgets, MSVC runtime, Windows CRT and
kernel dependencies. No Qt DLL, Python runtime, IPC helper or background service
is shipped inside the plugin package. The DLL and installer are unsigned.

Both the native `ts3plugin_author()` export and the rebuilt installer's
`package.ini` were checked to report **Khivus**. The packaged DLL matches the
tested Release build byte for byte.

## Automated tests

CTest completed **6/6 suites**, with **302 checks** total for version 1.1.1:

| Suite | Checks | Executed coverage |
| --- | ---: | --- |
| Speaker core | 27 | Server/client identity, simultaneous speakers, immediate removal, mode changes, rename, own-client filtering, delayed-stop cancellation, five-second retention/restart/deadline boundaries, disconnect bypass, 10,000 rapid PTT cycles |
| People filter core | 19 | Include/exclude modes, stable UIDs across reconnects/servers, identical names, renamed labels, numeric client-ID reuse, transient rules, disconnect cleanup and restoration |
| Qt UI | 70 | Real controller/canvas, Unicode and duplicate nicknames, row limits, server filtering, real Qt expiry timer including five-second retention and duplicate stops, five-second settings range and INI persistence, local muted status, settings/geometry restore, show/hide, frameless/max-height, visible settings/window destruction, empty-header containment and active-to-empty transitions at 75/90/100/150/200 percent in both frame modes under a simulated host stylesheet, proportional window dimensions |
| Filters and docking UI | 59 | Actual row context menu, captured UID after client-ID reuse, quiet-person inclusion, INI persistence, manager modes, no polling, real QDockWidget, independent geometry restoration, scale changes while docked, host destruction/fallback, ambiguous hosts and Configure parents, unload with dock/dialogs open, saved dock width/height across full host/controller recreation, delayed host show, hidden/minimized startup, immediate shutdown before geometry debounce and retention of unrelated host content |
| Columns UI | 46 | Automatic/fixed columns, both fill directions, height-first vertical stacking, height-only dock/window resize, automatic floating maximum height, live width/scale reflow, real wide dock and visible rows, context targets in each column/after scrolling/after filtering, unused cells, remainder pixels, total speaker limits, vertical overflow, empty header, settings Save and INI reload |
| Native DLL host | 81 | Actual exported functions and official SDK struct, worker callback queueing, client/global menu allocation and actions, incoming whispers, duplicate names, 2,000 PTT cycles, server disconnect, rename/mute/unmute, disabled own microphone, commands/settings, include-only filtering, official Configure parent and dock creation, pending-event cancellation, visible dock/filter/settings unload, persistent rules across DLL reload, worker-thread init/shutdown, SDK allocation cleanup and GUI-thread lookup verification |

The native-host suite also passed separately while loading **the installed TS3
client's `C:\Program Files\TeamSpeak3\Qt5Core.dll` runtime**, rather than the
development kit's copy. The test executable printed the loaded Qt runtime path.
This exercises the compiled plugin with the client's actual Qt runtime in a
separate mock host, not inside the running TeamSpeak client.

The Qt UI suite, including the five-second timer checks, also passed against the
installed client's Qt runtime for version 1.1.1. The filters/docking UI suite,
including the dock-size restart regressions, also passed against that runtime
for version 1.1.1. These tests construct
their own Qt main windows and do not attach to the live TS3 UI.

The new full-restart dock-size test failed before the fix: a new host/controller
lost the previous dock thickness. It now passes for bottom-dock height and
side-dock width, including host destruction before the geometry save timer fires.
Only WhosTalkin's dock dimensions/area are persisted, and Qt constrains the
restored size within the current host layout. A real TeamSpeak restart remains
part of the live checklist.

The columns UI suite passed separately using the installed client's Qt runtime.
`docs/columns-simulator.png` was captured from its real Qt dock, with seven mocked
speakers filling three columns. The image was inspected: names, Unicode,
normal/whisper microphones, elided long names and both whisper badges stay inside
their own cells. These column changes have not been tested inside the running
TeamSpeak client.

`docs/columns-stacked-simulator.png` was also captured and inspected after choosing
vertical filling in the real settings dialog: four speakers fill the first
column down to the dock's available height, with the remaining three in the
second column. Additional checks grow/shrink only the dock height, confirm the
resulting wrap positions, test scrolling and restore the saved fill direction.

`docs/people-filters.png` and `docs/docked-simulator.png` were captured and
visually checked using the simulator's shared production widgets. The filter
manager fits its controls, and the mock host's dock shows normal/whisper/Unicode
rows. These previews depict simulated events and a mock host, not the live client.

### Requested scenarios

| Scenario | Evidence |
| --- | --- |
| One normal speaker starts/stops | Core, UI and DLL host |
| Two simultaneous speakers | Core, UI and DLL host |
| Incoming whisper | UI and DLL host |
| Normal speaker changes to whisper | Core, UI and DLL host; same identity, no additional row |
| Whisper target unavailable | API research; mock supplies only incoming boolean and UI shows only `WHISPER` |
| Disconnect while talking | Core, UI and SDK movement callback in DLL host |
| Identical nicknames | Distinct server/client keys retain distinct rows |
| Rapid Push-to-Talk | Core, UI and worker-thread SDK callback stress cases |
| Switch connected servers | UI and actual exported server-tab callback in DLL host |
| Unload with visible window | UI and actual DLL shutdown/unload with settings and queued events |

The preview was inspected visually after supplying Windows fonts to Qt's
offscreen renderer. Title, names, Unicode, green/purple microphone styling and
`WHISPER` are visible; no target name or inactive member list is present.

For 1.0.1, regression images of the empty and active windows at 75% and the empty
window at 100% were also inspected. The complete title, settings and close
controls remain visible after silence; the 75% width and text are smaller than
100%. A regression check reproduced the old header failure before the fix.

## Remaining live checks

These have **not** been executed and must not be treated as passed:

- Loading/enabling the plugin **inside TS3** through its Addons interface and
  exercising real microphone / remote voice / incoming whisper delivery.
- Real server disconnect, nickname change, own channel movement, remote kick/ban,
  and received whispers from other channels under TS3's actual event ordering.
- Startup/restart behavior with a saved config and installer integration.
- Window focus, taskbar hiding, dragging/resizing, desktop opacity, multiple
  monitors, TS3 minimization, and interaction with fullscreen games on a desktop.
- Live outgoing-whisper transitions while the local microphone stays active.
- Live TS3 client context-menu placement, cached identity availability and
  experimental docking in the actual client main window / Addons Configure parent.
- Linux compilation, client loading, and X11/Wayland window-manager behavior.
- Long-session RAM/CPU measurement or a dedicated leak detector. Mock SDK-owned
  allocations were balanced and synchronous GUI destruction was checked, but
  that is not a substitute for a long-session process-level leak audit.

## Manual client checklist

1. Install the package, enable WhosTalkin, confirm the Plugins menu and empty
   startup window. Minimize TS3 and confirm the window remains available.
2. Have two clients transmit at once. Release one PTT key; only that row should
   vanish. Release both; the window should be empty at removal delay 0.
   Set removal delay to 5000 ms, transmit briefly and stop: the row should remain
   for five seconds. Transmit again before expiry and confirm the next stop starts
   a fresh delay. Disconnect or mute the local microphone to bypass the delay.
3. Receive a direct and a group whisper; both must show purple `WHISPER` without
   a guessed recipient. Switch from normal speech to whisper without a duplicate.
4. Mute the local microphone, try PTT, then unmute and transmit. Disabled local
   speech must remain absent. Disconnect a transmitting remote client.
5. Use equal nicknames on one server and equal client IDs across two servers.
   Test active-tab and all-server modes, and change a transmitting nickname.
6. Move the local client between channels. Confirm old normal speech disappears
   and ongoing incoming whispers follow actual whisper stop/disconnect events.
7. Change appearance and row/height limits; hide/reopen; restart TS3; confirm
   geometry/settings restoration. Reset position after moving monitors.
8. With both utility and settings visible, disable/re-enable the plugin, then
   close TS3. Verify no surviving plugin windows and no client crash.
9. Right-click an actual TS3 client and include/exclude/reset their rule. Repeat
   from a speaking row, undo an exclusion in People Filters, and check include-only
   mode during silence, nickname changes, reconnects and multiple server tabs.
10. Choose experimental docking. Verify the main-window panel, native movement,
    show/hide, minimize behavior and switching back to independent geometry.
    Change scale while docked, switch back, then unload with the panel visible.
    Resize the dock, restart TS3, and confirm top/bottom-dock height or side-dock
    width is restored. Repeat with startup hidden/minimized and then show it.
11. Leave Columns on Auto and widen/narrow the dock. Speakers should fill left to
    right and wrap into more/fewer rows. Choose a fixed count, try server labels
    and whispers, and right-click people in each column after scrolling.
    Set Column fill to Top to bottom, then right. Grow/shrink the dock height;
    a column should use the available height before starting the next column.
    Save/restart and confirm the fill direction is restored.
