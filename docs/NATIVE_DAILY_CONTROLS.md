# Native daily control interfaces

Meo Settings pages use the following system owners. A successful build or
page load proves the client integration only; hardware, authentication and
real session operations remain subject to runtime acceptance.

| Page | Owner and supported writes |
| --- | --- |
| Network connections | NetworkManager: saved profile activation, automatic connection, metering, basic single-address IPv4 and DNS. Raw settings preserve unrelated connection properties. Credentials stay with NetworkManager and its secret agent. |
| Hotspot | NetworkManager: supported Wi-Fi AP with WPA2, shared IPv4 and a volatile profile. Stopping addresses only the active AP profile. |
| Proxy | KDE/KIO `kioslaverc`: none, manual, PAC, discovery or environment proxy; excludes inline credentials. These preferences are not a universal proxy for arbitrary applications. |
| Display | KScreen: mode, refresh, scale, rotation, position, primary and enabled outputs. The separate `meo-settings-display-transaction` helper holds the original configuration and rolls back on timeout, explicit rejection or client disconnect. |
| Sound | PulseAudioQt: master/microphone, application streams, stream routing, card profiles, device ports and channel volumes. Notification volume uses an existing writable event stream-restore entry. Qt Multimedia plays an explicitly requested bounded test tone through the default output. |
| Power | PowerDevil profile configuration and the live power-profile owner. The native page changes screen idle, automatic sleep, supported lid actions, external-monitor lid inhibition, and low/critical battery thresholds/actions. Profile reloads require a fresh owner confirmation that the lid is open; writes preserve unrelated keys, honor immutable entries and restore previous preferences on failure. Absent dynamic settings remain system defaults. Battery health uses Solid-reported full/design energy, with unknown values shown explicitly. |
| Appearance | Installed Meo desktop owner tools: matched modes, preset and configuration recovery. The preset does not reset panels or enable input frameworks. A restored configuration may require signing out to reload all components. |
| Wallpaper | Plasma's public desktop scripting API: local image and supported layout for all screens or a selected screen. The library contains installed wallpaper images and managed imported copies; removing an active image is rejected. |
| Cursors | Meo.System: installed Xcursor pointer themes and their actual bitmap sizes, `kcminputrc/Mouse` preferences and KDE CursorChanged notification. Supports same-theme size changes in a KDE Wayland session. No theme installation or session restart is performed. |
| Fonts | KDE font roles through KConfig and the KDE font-change notification. Installed families and styles only. |
| Keyboard, mouse, touchpad | Meo.System: XKB layouts, KWin-observed keyboard repeat preferences, and actual input devices/capabilities including supported device enable/disable. Unsupported vendor properties are not synthesized. |
| Input methods | Meo.System: typed Fcitx5 group, method inventory and group membership APIs. Opening the page does not start a framework. |
| Language/region | Installed libc locales and Plasma locale configuration. Language changes explicitly require a new session. |
| Date/time | systemd-timedated: timezone, NTP and validated manual time, with authorization handled by the service. |
| Default applications | KService/KApplicationTrader: actual installed MIME handlers, cache refresh and preferred-service readback. |
| Removable storage | Solid/UDisks: actual removable filesystem volumes and mount/unmount. Root and home unmounts are rejected. Existing usage scans remain bounded. |
| Screen lock | Meo.System: automatic lock, idle timeout, resume/start lock and authentication grace timing. Reduced protection and device disable actions request confirmation. Authentication policy remains with the screen locker; failed apply attempts restore the saved preferences. |
| Privacy | XDG PermissionStore: existing camera, microphone, speakers, notification and background decisions. Unsupported records are read-only. This does not revoke direct native device access or terminate active capture. |
| Accessibility | KDE accessibility configuration and KWin's supported effects. Reduced motion uses the shared KDE animation factor. |

## Meo desktop tool contract

`meo-desktop-apply --appearance-only --quiet` validates the installed preset,
saves a private configuration snapshot, applies the supported Look-and-Feel
and canonical KWin defaults, and skips panel reset, input-framework lifecycle
and watcher activation. Unrelated keys in the canonical source are not added
by Settings itself. Failures are reported; the snapshot remains available.

`meo-desktop-apply --restore-latest --quiet` accepts only its own most recent
snapshot and restores a fixed inventory of desktop configuration files and
KDE defaults. The UI confirms replacement of subsequent edits before invoking
this action. It neither restarts Plasma/KWin nor restores arbitrary paths.

`meo-theme-mode light|dark` selects matching Meo color, desktop and icon
variants. Settings checks component availability before invoking it.

## Display recovery boundary

The display helper accepts a closed request over its client channel and uses
KScreen, not shell commands. A confirmed change commits; otherwise the
independent guardian attempts rollback. On a topology change, rollback matches
connected outputs and available modes against the current backend configuration.
A failed or unresponsive display service can prevent successful rollback;
the client must retain that failure instead of reporting recovery as complete.

## Runtime dependencies

The Settings application links Qt Multimedia for the test tone and KService
for preferred applications. Display recovery installs under
`${CMAKE_INSTALL_LIBEXECDIR}/meo-settings`. Meo.System must provide the
`InputDevices`, `InputMethods` and `Platform` singleton APIs consumed by the
native pages. Native Night Light mode/temperature controls require the installed
KWin schema with `Constant` / `DarkLight` modes; older numeric enums are not
interpreted as that schema.

## Authority references

- [Plasma wallpaper owner implementation](https://invent.kde.org/plasma/plasma-workspace/-/blob/master/wallpapers/image/plasma-apply-wallpaperimage.cpp)
- [XDG PermissionStore tables](https://github.com/flatpak/xdg-desktop-portal/wiki/The-Permission-Store)
- [NetworkManager connection API](https://networkmanager.dev/docs/api/latest/gdbus-org.freedesktop.NetworkManager.Settings.Connection.html)

`SETTINGS_COVERAGE.md` remains the full product target. This interface document
specifies implemented controls and does not declare every long-term capability
covered. Specialist and unavailable operations retain their explicit advanced
compatibility boundary.
