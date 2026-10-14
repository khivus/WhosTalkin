# SDK and runtime investigation

Investigation date: 2026-10-14. Sources were inspected directly, including the
official SDK checkout, requested Qt template, and this machine's installed client.

## Checked sources

- [Official SDK, pinned commit](https://github.com/teamspeak/ts3client-pluginsdk/tree/4aa90a53aa150cbf81e13bc97e68c0431b26499f)
- [SDK function table](https://github.com/teamspeak/ts3client-pluginsdk/blob/4aa90a53aa150cbf81e13bc97e68c0431b26499f/include/ts3_functions.h)
- [Public properties and whisper enums](https://github.com/teamspeak/ts3client-pluginsdk/blob/4aa90a53aa150cbf81e13bc97e68c0431b26499f/include/teamspeak/public_definitions.h)
- [Official callback examples](https://github.com/teamspeak/ts3client-pluginsdk/blob/4aa90a53aa150cbf81e13bc97e68c0431b26499f/src/plugin.c)
- [Requested Qt integration template](https://github.com/Gamer92000/TeamSpeak3-Qt-Plugin-Template)
- Local `C:\Program Files\TeamSpeak3\ts3client_win64.exe` version 3.6.2;
  `Qt5Core.dll` and `Qt5Widgets.dll` file/product versions 5.15.2.0.
- Local TS3 `changelog.txt`: 3.6.0 raised the plugin API to 26 for the Qt update.

## Findings

The SDK example returns API **26**. The talking callback provides connection
handler, talk status, received-whisper boolean, and client ID. It supplies neither
target type nor target ID. `STATUS_TALKING_WHILE_DISABLED` is explicitly local
microphone-muted activity and must not create a row.

The full public function table exposes `isWhispering` and `isReceivingWhisper`
boolean queries and `requestClientSetWhisperList`, a setter for channel/client
target arrays. It has no received-whisper target-list getter. Public callback
signatures likewise provide no received group/channel/client target metadata.
Whisper group and target-mode enums describe configurations, not incoming payload
metadata. Group membership lookup cannot establish why a sender's whisper reached
this client.

**Conclusion for API 26:** reliably display the incoming-whisper boolean and
the current speaker nickname. Do not display group/channel/commander target names
for any received whisper. The settings target control is disabled rather than
suggesting that missing permissions can enable it. Outgoing local activity can
be tagged from `isWhispering` at talk-status changes, still without a target name.
This conclusion applies to this public SDK version, not a claim about private
client internals or hypothetical future SDKs.

The installed client already runs a Qt 5.15.2 GUI, which supports parentless
native widgets and queued GUI-thread calls. A separate helper process would add
IPC and lifecycle complexity without a demonstrated compatibility need. The
chosen plugin shares the host runtime and never creates a second QApplication.

The template uses Qt 5.15.2 with a v140 setting and the MSVC2019 Qt package. This
project uses the package's MSVC-compatible v142 toolchain, C++17, and shared Qt.
The built DLL was verified in a Qt host with the official SDK ABI; real TS3
audio-session testing remains a separate step.

## Person filters and docking in 1.1.0

`PLUGIN_MENU_TYPE_CLIENT` registers native client context actions, and
`onMenuItemEvent` supplies the selected client ID together with its server handler.
`getClientVariableAsString(..., CLIENT_UNIQUE_IDENTIFIER, ...)` obtains the public
client identity for persistent filtering. Live speaker tracking continues to use
the required server/client key; nicknames are only display labels.

The public function table does not provide a dock registration API or a main
window getter. Qt does provide public `QMainWindow::addDockWidget` and `QDockWidget`
ownership APIs. The optional implementation uses the SDK Configure parent when
compatible, otherwise one unambiguous top-level Qt `QMainWindow`. Runtime type
checks and weak pointers retain an independent-window fallback. No private
client widget names or layouts are used. This is a best-effort Qt integration,
not a claim of an SDK-guaranteed extension point. Tests exercise mock main-window
ownership using the real Qt runtime; live TS3 docking remains unverified.
