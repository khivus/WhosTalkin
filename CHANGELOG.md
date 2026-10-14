# Changelog

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
