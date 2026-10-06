# Meo Settings coverage contract

## Product role

Meo Settings is the default day-to-day system settings surface for MeoArch.

A normal user should be able to configure and maintain the desktop without repeatedly leaving Meo Settings for KDE System Settings or standalone configuration utilities.

Meo Settings does not replace the real system authorities behind a setting. It provides a MeoUI presentation over maintained Qt/KDE/Linux/Meo.System APIs and delegates privileged or destructive operations to the appropriate trusted backend.

The product target is:

```text
User
  -> Meo Settings
       -> native Meo page
            -> authoritative system backend
```

not:

```text
User
  -> Meo Settings
       -> list of links
            -> KDE System Settings / external utility
```

KDE System Settings remains an expert compatibility escape hatch, not the normal path.

---

## Fallback policy

### Daily settings must be native

Do not use a KCM handoff for a setting that is part of a normal daily workflow once the corresponding Meo page is declared covered.

Examples that should be native in Meo Settings include Wi-Fi, Bluetooth, display, sound, notifications, power, wallpaper, theme mode, dynamic color, keyboard/input methods, language/region, date/time, default applications, common accessibility settings, and common privacy controls.

### KDE System Settings is a final escape hatch

Each major category may expose one low-emphasis action near the end of the page such as:

- `Advanced system settings`
- `Open KDE System Settings`

This action is for specialist controls, unsupported hardware/vendor modules, upstream troubleshooting, and features that have not yet received a safe native Meo implementation.

Do not mix ordinary KCM links into the main list of normal controls. If a routine capability is not implemented yet, the page should state that it is not yet available in Meo Settings rather than presenting the KCM as if it were the normal Meo workflow.

A single global fallback should also remain available from the System/About/Advanced area.

---

# Required day-to-day coverage

The following areas define the minimum long-term product coverage for Meo Settings. Individual backends can remain upstream KDE/Linux services; the user-facing workflow should remain inside Meo Settings whenever practical.

## Home and search

Meo Settings should provide:

- searchable settings and capabilities;
- recent/common settings;
- status summaries that link to the owning Meo page;
- warnings when an important system capability needs attention;
- direct routes usable by Launcher, Meo AI and system notifications.

Search results should prefer native Meo pages. Advanced KCM results may appear only as clearly marked advanced/compatibility results.

## Network & internet

Normal in-app coverage should include:

- Wi-Fi on/off;
- scan and connect;
- saved networks;
- forget network;
- password/authentication flow;
- connection details;
- metered setting where supported;
- Ethernet status and basic configuration;
- hotspot where the backend/hardware supports it;
- VPN listing/connect/disconnect and add/import entry point;
- proxy settings;
- airplane/offline behavior where MeoArch exposes it.

NetworkManager remains the authority. Do not duplicate connection state in local settings data.

## Bluetooth and connected devices

Normal in-app coverage should include:

- Bluetooth on/off;
- discoverable/scanning state;
- nearby devices;
- pair/connect/disconnect;
- forget/remove device;
- trusted/bonded device state where supported;
- battery information when exposed by the device stack;
- common device details.

BlueZ remains the authority.

## Display

Normal in-app coverage should include:

- monitor discovery;
- resolution;
- refresh rate;
- scale/fractional scale;
- orientation;
- primary display;
- arrange displays;
- enable/disable display;
- HDR/VRR controls when supported safely by the backend;
- brightness when the device exposes a controllable display backlight;
- Night Light / color-temperature controls.

KScreen/KWin and the appropriate hardware service remain authoritative.

## Sound

Normal in-app coverage should include:

- master output volume;
- mute;
- output device selection;
- input device selection;
- microphone volume/mute;
- per-application stream volume when available;
- balance/profile/port for common devices;
- test sound;
- notification/system sound level;
- spatial/special profiles only when the backend exposes a stable interface.

PipeWire/PulseAudioQt or the maintained Meo.System audio authority remains authoritative.

## Notifications

Normal in-app coverage should include:

- global notifications on/off where meaningful;
- Do Not Disturb;
- application notification permissions/preferences;
- pop-up/banner behavior;
- sound behavior;
- lock-screen visibility/privacy;
- notification history policy if Meo provides history.

## Power & battery

Normal in-app coverage should include:

- battery status/health information exposed by the system;
- power profile;
- screen-off timing;
- sleep/suspend timing;
- lid-close action where applicable;
- low/critical battery behavior;
- performance/balanced/power-saver mode when supported;
- battery charge limit when the hardware exposes a maintained interface;
- screen brightness integration;
- wake/idle behavior for common workflows.

PowerDevil and hardware-specific maintained authorities remain authoritative.

## Appearance and MeoUI

Appearance must become a native Meo workflow rather than a directory of KDE appearance KCMs.

### Theme application

Meo Settings must be able to apply the supported Meo desktop appearance as a coherent bundle instead of requiring the user to configure each KDE layer separately.

The supported Meo bundle can include, as applicable:

- light/dark/system mode;
- Meo color scheme and dynamic color;
- MeoStyle for Qt Widgets;
- Meo Plasma fallback/desktop theme;
- Meo icon set/application icon treatment;
- KWin decoration;
- cursor preset;
- lock/login/splash presentation when owned by Meo;
- supported shell visual profile.

Applying a supported preset should be transactional where possible: validate required components, apply owned settings, report partial failures, and provide a recovery/reset path.

Do not expose a normal user to the requirement to separately choose a Plasma style, widget style, icon theme, color scheme and window decoration just to obtain the intended Meo appearance.

### Wallpaper

Meo Settings must provide an in-app wallpaper workflow with at least:

- preview grid;
- current wallpaper;
- bundled wallpapers;
- choose local image;
- fit/fill/crop behavior supported by the active wallpaper backend;
- per-screen choice when multiple screens require it;
- remove/manage imported wallpapers;
- dynamic-color refresh from the selected wallpaper;
- slideshow support when a stable backend is available.

Changing wallpaper should update the real Plasma/desktop wallpaper authority and should not maintain a fake parallel wallpaper preference.

### Meo visual preferences

Meo-owned visual preferences should be configurable without KDE handoff. At minimum expose the stable subset of:

- color source: wallpaper / manual / system accent;
- manual accent/color selection;
- light/dark/system appearance;
- font scale;
- supported interface density/global scale if Meo defines one;
- reduced motion;
- motion scale when supported as a public MeoUI preference;
- application icon style/shape;
- supported corner/shape preference only if Meo Design System intentionally makes it user-configurable;
- contrast/accessibility appearance options that are safe to expose.

MeoUI implementation details that are not intended to be public customization must not become settings merely because a token exists.

### Fonts, cursors and advanced appearance

Common font size/family and cursor size/theme controls should eventually be native if MeoArch treats them as supported user settings.

Specialist theme installation, arbitrary third-party Plasma themes, unusual KWin decorations and unsupported theme engines may remain behind the advanced KDE fallback.

## Desktop, windows and multitasking

Normal in-app coverage should include common Meo desktop behavior such as:

- dock/task presentation preferences owned by Meo;
- launcher behavior;
- top-bar/menu behavior owned by Meo;
- workspace/virtual desktop basics;
- overview behavior;
- focus behavior for supported modes;
- window animation/reduced-motion integration;
- common snapping/tiling settings exposed by maintained KWin APIs;
- lock-screen layout/preferences owned by Meo.

Do not surface internal debug switches as normal personalization options.

## Keyboard, mouse and touchpad

Normal in-app coverage should include:

- keyboard layout/input source summary;
- key repeat and delay;
- common shortcuts and shortcut search;
- pointer speed;
- primary button;
- natural scrolling;
- touchpad enable/disable;
- tap-to-click;
- scrolling behavior;
- acceleration/profile where exposed;
- common gesture settings where Meo owns the gesture contract.

Hardware/vendor-specific pages may remain advanced fallbacks.

---

# Input method contract

## Default strategy

MeoArch should provide a first-class Fcitx5 experience on Wayland, but Fcitx5 must remain a user choice rather than an irreversible desktop dependency.

The default MeoArch profile may enable the supported Fcitx5 integration automatically. Meo Settings must also provide a clear mode that allows the user to stop using the Meo-managed Fcitx5 setup and manage/install another input-method framework themselves.

Suggested modes:

1. **Meo-managed Fcitx5** — recommended/default on supported installations.
2. **Fcitx5, self-managed** — keep Fcitx5 but stop Meo from automatically managing optional engines/packages and preferences.
3. **Custom / another input method** — disable Meo-managed Fcitx5 startup/integration and leave input-method ownership to the user.

Switching away from Meo-managed mode must not silently uninstall user dictionaries, engines or configuration. Destructive cleanup requires an explicit separate action.

## Native input-method page

The normal input-method workflow should stay in Meo Settings and include:

- whether the framework is available/running;
- enable/disable Meo-managed Fcitx5 integration;
- active input methods;
- add/remove/reorder input methods;
- current/default input method;
- global switch shortcut;
- per-input-method options when a stable Fcitx5 API/config contract is available;
- status of Qt, GTK and Wayland integration;
- diagnostics with actionable fixes;
- restart/reload Fcitx5 when safe and explicitly requested;
- import/migration entry point when supported by Fcitx tooling.

Do not make `kcm_fcitx5` the normal input-method page. It may remain under an `Advanced Fcitx5 configuration` action until Meo covers every specialist option.

## Input-method package discovery

Meo Settings should be able to install missing Fcitx5 integration modules and language engines contextually from the input-method page.

Examples of current Arch package families include:

- core framework/integration: `fcitx5`, `fcitx5-qt`, `fcitx5-gtk`, `fcitx5-configtool`;
- Chinese: `fcitx5-chinese-addons`, `fcitx5-rime`;
- Japanese: `fcitx5-mozc`;
- Korean: `fcitx5-hangul`;
- broader language coverage: `fcitx5-m17n`;
- additional engines/addons available in configured repositories.

This list is illustrative, not a hard-coded permanent catalog.

The implementation should discover package availability from the configured package repositories or a versioned Meo capability catalog and map a user-facing language/engine to the required packages.

Example flow:

```text
Add input method
  -> Japanese
  -> Mozc
  -> engine missing
  -> Install support
  -> trusted package transaction backend
  -> refresh Fcitx5 engine inventory
  -> add Mozc to active methods
```

The UI should show download/install size when the package service exposes it and clearly distinguish already-installed, available, unavailable and third-party/community sources.

## Package-management boundary

QML must not run `pacman -S`, `sudo`, arbitrary shell commands, or parse package-manager output as the package authority.

Package installation/removal must go through the trusted package transaction service used by MeoArch/OmniStore (or another explicitly maintained package backend). Authentication/Polkit, repository trust, signatures, transaction progress, cancellation and errors belong to that backend.

Meo Settings owns the contextual workflow; the package service owns the transaction.

---

## Language & region

Normal in-app coverage should include:

- display language selection;
- installed language packs status;
- region/locale;
- date format;
- time format;
- number format;
- currency format;
- measurement system where applicable;
- first day of week where supported;
- keyboard/input sources;
- input methods as defined above;
- spell-check languages/basic behavior;
- weather location used by Meo surfaces.

When changing system locale requires logout/restart, state that explicitly and preserve a safe apply/revert path.

## Date & time

Normal in-app coverage should include:

- automatic network time;
- current time/date where manual changes are permitted;
- time zone search/map/list;
- automatic time zone when a supported location source is available and the user permits it;
- 12/24-hour preference if not already owned by locale format settings.

Use the maintained system/KDE time authority rather than writing system files directly from QML.

## Applications

Meo Settings should own system-level application preferences, including:

- default applications;
- file/link defaults where practical;
- autostart/background behavior;
- notification preferences;
- permissions exposed by Meo's permission model;
- uninstall/repair entry points when supported;
- storage usage and cache/data actions when safely exposed.

### Installing applications from Settings

Users should be able to install missing apps/components without being forced into an unrelated KDE tool.

Meo Settings may expose a lightweight in-app software browser or contextual install results by consuming the same catalog/transaction service as OmniStore.

Do not duplicate the entire store implementation inside Settings. OmniStore/package-service remains the authority for catalog metadata and transactions.

Recommended behavior:

- contextual installs (input methods, required helpers, themes, optional system components) happen directly inside the owning Settings page;
- the Applications page may expose search/install/uninstall using shared OmniStore models/services;
- rich discovery, editorial content and full store browsing may still open OmniStore, preferably through a Meo-native route rather than KDE Discover;
- KDE Discover is not the normal MeoArch application-management path.

## Storage

Normal in-app coverage should include:

- total/free storage;
- category usage where data is trustworthy;
- largest applications/files entry points;
- temporary/cache cleanup with preview/confirmation;
- removable storage summary;
- per-application storage when available;
- low-space warnings.

Partitioning, filesystem repair, destructive formatting and recovery operations may hand off to a dedicated trusted tool when Meo Settings cannot provide a complete safety/recovery path.

## Privacy & security

Normal in-app coverage should include the user-facing subset of:

- application permissions known to Meo;
- camera/microphone/location/privacy indicators and policy where supported;
- screen-lock timing;
- lock-screen notification privacy;
- clipboard/history privacy if Meo owns the feature;
- diagnostics/telemetry preference if MeoArch has any such data collection;
- credential/account entry points;
- firewall status/basic profile only if a maintained backend exists;
- security/update status summaries.

Sensitive security tools must keep their authoritative privilege and recovery model.

## Accessibility

Meo Settings should natively expose commonly needed accessibility controls such as:

- text/interface scale;
- high-contrast/contrast assistance when supported;
- reduced motion;
- screen reader enablement/status;
- keyboard accessibility basics;
- pointer/cursor size;
- visual/audio notification aids where supported;
- magnifier/zoom entry point when backed by KWin/accessibility services.

Advanced assistive-tool configuration may remain an explicit advanced handoff.

## Accounts and users

Normal in-app coverage should include:

- Meo Account state/preferences;
- local user summary;
- profile image/name where safe;
- password/change-auth entry point through the proper authority;
- additional local users when a maintained privileged backend exists;
- online account/inference permissions owned by Meo;
- session/device information relevant to account security.

Do not implement privileged account mutation with ad-hoc shell commands.

## System, updates and recovery

Normal in-app coverage should include:

- MeoArch/desktop version;
- kernel/session/build information useful to users;
- update availability and update action through the supported update/package authority;
- restart-required state;
- hardware summary;
- drivers/firmware status where a maintained service exposes it;
- backup/recovery/reset entry points when MeoArch provides them;
- logs/diagnostics export designed for user support;
- About/legal/open-source information.

Destructive reset/recovery actions require explicit confirmation and should live behind a dedicated trusted recovery path.

---

# Navigation target

The long-term top-level information architecture should be understandable without knowledge of KDE module names.

A suitable category model is:

```text
Home
Network & internet
Bluetooth & devices
Display
Sound
Notifications
Appearance
Desktop & multitasking
Keyboard, mouse & input
Language & region
Applications
Power & battery
Storage
Privacy & security
Accessibility
Accounts & users
System
```

The exact responsive navigation presentation belongs to MeoUI/Meo Settings design. The category names above define capability ownership, not a requirement to copy an Android Settings sidebar literally.

---

# Native-page acceptance rule

A settings area is not considered natively covered merely because Meo Settings displays its current status.

For a routine workflow to count as covered, the user must be able to:

1. see the authoritative current state;
2. change the common setting in Meo Settings;
3. receive validation/progress/error state;
4. have the change applied by the real system authority;
5. return to a correct refreshed state;
6. recover or understand next steps when an operation fails.

A page that only displays status plus an `Open KDE Settings` button is a compatibility page, not native coverage.

---

# Advanced compatibility boundary

KDE System Settings and specialist utilities remain valuable for:

- uncommon expert options;
- vendor-specific controls;
- third-party KCMs;
- configuration surfaces whose public API is not safe/stable enough for Meo yet;
- troubleshooting and comparison with upstream behavior.

Keep these available, but visually separate them from normal Meo settings. The default user journey should remain inside Meo Settings.
