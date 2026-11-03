# Third-party notices

Meo Settings' own code is licensed under GPL-3.0 (see `LICENSE`). The
components below keep their upstream licenses. The package installs this file
to `share/licenses/meo-settings/`, where Settings → About reads it.

## Platform

These libraries are dynamically linked from the system and are not bundled.

| Component | Use | License |
| --- | --- | --- |
| Qt 6 (Core, Gui, Qml, Quick, Quick Controls, DBus, Multimedia, Concurrent) | Application framework | LGPL-3.0-only |
| KDE Frameworks 6: KCoreAddons, KConfig, KService, Solid | Device facts, configuration, services | LGPL-2.0-or-later / LGPL-2.1-or-later (per file) |
| NetworkManagerQt | Wi-Fi and network connections | LGPL-2.1-or-later |
| BluezQt | Bluetooth | LGPL-2.1-or-later |
| PulseAudioQt | Sound devices and volume | LGPL-2.1-or-later |
| libkscreen (KF6Screen) | Displays | LGPL-2.1-or-later |
| MeoUI | Shared QML design system, its fonts and Material Symbols | MIT; bundled assets are listed in MeoUI's `THIRD_PARTY_NOTICES.md` |
| Meo.System / meo-kde runtime | Desktop and system integration | See the meo-kde repository |

Build and development tools that are not shipped (for example CMake and Qt
Linguist) are not listed here.
