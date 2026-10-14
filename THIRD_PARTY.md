# Third-party dependencies

WhosTalkin source is MIT licensed. External dependencies retain their licenses.

- [Official TS3 Client Plugin SDK](https://github.com/teamspeak/ts3client-pluginsdk),
  pinned to `4aa90a53aa150cbf81e13bc97e68c0431b26499f`, API 26.
  Its README states copyright TeamSpeak Systems GmbH, all rights reserved.
  SDK headers are fetched for building and are not copied into this source tree
  or the plugin installer. Review the SDK's terms for your distribution.
- [Qt 5.15.2](https://www.qt.io/licensing/): Core, Gui and Widgets, dynamically
  linked. The plugin reuses the client's existing Qt runtime; the installer
  contains no Qt libraries. Qt is available under its own LGPL/GPL/commercial
  terms. A separately distributed simulator with bundled Qt needs the applicable
  notices and licensing materials.
- [TeamSpeak3-Qt-Plugin-Template](https://github.com/Gamer92000/TeamSpeak3-Qt-Plugin-Template)
  was inspected as a build/integration reference. Its code was not copied.

The `.cache` directory contains local build dependencies, reference checkouts,
and downloaded toolchain files. It is ignored and is not release source.
