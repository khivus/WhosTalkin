# Changelog

## 1.1.1 – 2026-10-14 (unreleased)

- Remember dock dimensions across TeamSpeak restarts: restore side-dock width or
  top/bottom-dock height after the main window layout is ready. Save resizes
  automatically and flush the latest size on hide/unload or host destruction.
- Add automatic columns that fit the available window/dock width and scale,
  plus a saved choice of one through eight fixed columns.
- Add a saved Column Fill setting: left to right then down, or top to bottom then
  right. Vertical filling uses the available dock/window height before wrapping,
  grows to the maximum height in automatic floating mode, and scrolls vertically
  on overflow. Both modes retain the speaker order and total speaker limit.
- Reflow on resize, size automatic floating height from grid rows, and keep
  right-click person actions aligned with each cell, including after scrolling.
- Increase the maximum removal delay from 300 ms to 5000 ms (5 seconds), so brief
  speakers remain visible long enough to notice. The default remains 0 ms.
- Schedule the expiry timer directly for the next removal deadline instead of
  waking every 301 ms during longer delays. Restarted speech cancels removal;
  disconnects and muted/disabled speech still remove immediately.
- Verify five-second retention, expiration, restart behavior and saved settings.

## 1.1.0 – 2026-10-14

- Add included/excluded people and an optional include-only display mode.
  Saved rules match public TeamSpeak identities and survive reconnects; missing
  identities use connection-only rules that are cleared on disconnect.
- Add Include, Exclude and Remove Filter actions to the native TeamSpeak client
  context menu and the speaker window's row context menu.
- Add a People Filters manager to edit or remove rules for quiet/hidden people,
  available through Plugins, settings, the window context menu and `/whostalkin filters`.
- Add an experimental docked view inside a compatible Qt main window. It uses
  public Qt APIs, keeps an independent-window fallback, preserves floating
  geometry and restores only this plugin's dock area.
- Add tests for identity reuse, persistent rules, real context actions, dock
  ownership, host destruction, presentation switching and DLL unload/reload.
- Extend the simulator with person filter controls and a mock dock host.

## 1.0.1 – 2026-10-14

- Set the plugin and installer author to Khivus.
- Keep the complete header and its controls visible when the final speaker stops.
  Empty lists no longer reserve a scroll-area minimum height, and the header
  minimum is measured from its styled controls rather than a fixed pixel value.
- Apply the 75–200% scale to fonts, badges, spacing, resize grip and window
  dimensions, including values below 100%. Window-local font rules prevent
  TeamSpeak skin fonts from overriding the scale setting.
- Add regression checks for active-to-empty transitions at 75%, 90%, 100%,
  150% and 200%, in framed and frameless modes with a simulated host stylesheet.

## 1.0.0 – 2026-10-14

- Initial native TS3 API 26 plugin, speaker tracking, incoming whisper display,
  independent Qt window, persisted settings, simulator and automated DLL host.
