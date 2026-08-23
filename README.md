# Meo Settings

Meo Settings is MeoArch's primary, day-to-day settings application: a MeoUI
experience over stable KDE Frameworks, KDE services, existing advanced KCMs,
and Meo-owned configuration. It is not a fork of System Settings or a
replacement for NetworkManager, BlueZ, PipeWire, KScreen, PowerDevil, or KWin.
Those services remain the backend authority for state, privileges, and
recovery; a KCM is an explicit advanced/compatibility/recovery handoff rather
than the normal daily route for a supported Meo workflow.

Native daily workflows include reactive Wi-Fi; Bluetooth adapter, discovery,
pairing, connection, trust, block, rename, and removal controls; sound; direct
per-display brightness and Night Light controls; power profiles; global and
per-application notification preferences; display inventory; appearance;
Control Center tile configuration; mounted storage, bounded personal-category
usage, and system information. The shell uses MeoUI's
five responsive window classes and shared page/navigation transitions rather
than local breakpoint copies. Bluetooth pairing uses a short-lived Meo
`org.bluez.Agent1` conversation: PIN entry, displayed PIN/passkey, numeric
comparison, and authorization all require an explicit response in a temporary
third-level task sheet. The agent is never made BlueZ's default agent, so a
pairing request received while Settings is closed remains owned by the system
agent (normally Bluedevil). Display topology and destructive disk work remain
deliberately outside the native daily flow until they have separately verified
recovery paths.

For a new secured Wi-Fi profile, the user explicitly chooses whether to save
the network. A saved choice creates a disk-backed NetworkManager profile; an
unsaved choice creates a volatile profile that NetworkManager removes after it
disconnects. The QML page clears the entered password immediately; Meo Settings
does not register its own Secret Agent or KWallet credential UI, so credential
handling remains NetworkManager's system policy rather than an app claim.

## Theme and information-architecture contract

Meo Settings is a real Meo/KDE session application: it must receive the
complete HCT/Material dynamic role table from `MeoShellTheme` before its first
page is ready. In a sibling checkout that bridge comes from `meo-kde/qml` and
the `Meo.System` plugin; a package must depend on both MeoUI and the installed
MeoKDE QML runtime.

Appearance has an explicit, confirmation-gated Meo action. It selects exactly
one source—KDE accent, the configured local wallpaper, or a validated manual
`#RRGGBB` seed—then invokes the installed `meo-dynamic-colors` generator.
The generator derives the complete HCT/CAM16 Material role table and applies
one KDE scheme update. Settings does not synthesize a parallel palette,
compose a shell command from user input, or run the write in the background;
the action does not restart Plasma or KWin. A missing/local-invalid wallpaper
is reported rather than silently replaced with a different image. Advanced
contrast, wallpaper, icon, font, global-theme, and compatibility workflows
remain available through their maintained KCMs.

The normal Settings hierarchy is a clear first-level category and a complete
second-level page. Small one-off choices, volume facts, and confirmation flows
use a temporary third-level sheet and retract when finished; they are never
registered routes. Backup, restore, partitioning, formatting, encryption, and
other recovery-sensitive work remain a full maintained KDE task/tool until this
application has a verified backend, privilege model, progress UI, and recovery
path. The Storage page therefore provides real read-only volume facts,
explicit system-volume context, and safe handoffs, rather than pretending that
destructive disk actions are native. On explicit request it can make a bounded
scan of selected Home subfolders for images, videos, documents, audio, and
known local AI model/cache roots; it never treats that partial view as a
whole-disk total. On a separate explicit click, it can also inspect Pacman's
local installed-package database and separate official repository packages
from foreign/AUR candidates without refreshing metadata or touching packages.
OmniStore remains a separate, schema-validated read-only managed-app usage
overview; its source-tagged application-size mix is not a package inventory.
Neither view is a package-management surface or proof that OmniStore is
installed and working.

Updates are deliberately separate from OmniStore. The Updates page reads only
already-cached pacman metadata (`-Qu`, `-Qqm`, and `-Si`) to group pending Meo,
KDE/Plasma, system, and configured-custom-repository packages. It never
refreshes databases, runs `pacman -S*`, requests privilege, or changes a
package. An AUR check is an explicit, confirmation-gated query through the
user's installed `paru` or `yay`; it also performs no package action. The
maintained system updater remains the owner of repository refresh and install
work.

Meo Account is integrated only through the optional `org.meo.Accounts1`
session-bus broker. Its package-installed, manifest-scoped profile reader can
show the cloud name, HTTPS avatar, and broker-granted cloud ID without ever
receiving an OAuth or KWallet token. When the broker denies that profile grant,
Settings shows no invented ID and routes account changes to the maintained Meo
Account application. The local-user tool is a button inside the Accounts page,
not a parallel account destination.

Privacy lists effective Flatpak sandbox permission metadata per installed app.
It does not fabricate a universal native-app camera/microphone/file-access
history because Linux has no trustworthy global API for that claim. Screen
lock, firewall, and device-security controls remain clear maintained-tool
handoffs.

See [the architecture document](docs/ARCHITECTURE.md) for the current
ownership model, category map, backend policy, explicit deferrals, shared
controller proposal, and the boundary between offscreen checks and real
Plasma-session acceptance.

## Development build

The application imports the separately built MeoUI module.  From this
repository, configure an out-of-source build with the sibling MeoUI output:

```bash
cmake -S . -B out/build -G Ninja \
  -DMEOUI_IMPORT_ROOT=/home/shekong/Projects/meo-ui/out/build/release \
  -DMEO_KDE_QML_IMPORT_ROOT=/home/shekong/Projects/meo-kde/qml \
  -DMEO_SYSTEM_IMPORT_ROOT=/home/shekong/Projects/meo-kde/out/build/system/qml
cmake --build out/build
ctest --test-dir out/build --output-on-failure
QML_IMPORT_PATH=/home/shekong/Projects/meo-ui/out/build/release:/home/shekong/Projects/meo-kde/qml:/home/shekong/Projects/meo-kde/out/build/system/qml \
  ./out/build/meo-settings
```

For repeatable non-interactive layout verification, the executable can render
a route at an exact window size without taking any system-setting action:

```bash
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
QML_IMPORT_PATH=/home/shekong/Projects/meo-ui/out/build/release:/home/shekong/Projects/meo-kde/qml:/home/shekong/Projects/meo-kde/out/build/system/qml \
  ./out/build/meo-settings --route=home --size=480x720 \
  --screenshot=/tmp/meo-settings-compact.png
```

Startup, routing, and screenshot validation are read-only with respect to
system settings. A setting changes only after an explicit user action,
including native Wi-Fi, Bluetooth, audio, brightness, Night Light,
power-profile, confirmation-gated dynamic color, or Control Center control.
The Control Center action addresses exactly one active `org.meo.topbar` applet
through Plasma Shell, persists and reloads that applet's supported tile
configuration, and never writes Plasma configuration files directly or falls
back to an arbitrary applet instance. The optional OmniStore overview may run
only its documented read-only `omnistore-apps-export` contract; it never starts
the GUI to collect data, talks to a private daemon, or changes packages. Do not
use an offscreen smoke test as evidence that hardware operations work in a
live Plasma session.

## Capability and fallback policy

1. Use a stable KDE/Qt API or an installed Meo.System platform API when one
   exists.
2. Make that native control the normal Meo route when its safety and recovery
   contract is complete.
3. Route complex KDE-owned, advanced, privileged, or recovery-sensitive
   behavior to the installed KCM.
4. Use an official system D-Bus API only when neither is suitable.
5. Never use generic command-line control tools such as `nmcli`, `pactl`,
   `bluetoothctl`, or `kscreen-doctor` as a backend. The named
   `meo-dynamic-colors --apply` generator and read-only
   `omnistore-apps-export` contract are narrow, documented integrations—not
   generic device-control fallbacks.

KCM discovery uses Qt plugin paths rather than a hard-coded host directory.
In-process KCM embedding is intentionally deferred: QWidget KCMs cannot be
safely embedded in a Qt Quick shell, so Settings launches the installed KCM
only when the user asks. Native controls are the primary route for daily
brightness, Night Light, power-profile, dynamic-color, notification, and Meo
Control Center changes; KCMs remain for advanced scheduling, hardware policy,
appearance compatibility, and recovery-sensitive display topology.

## Packaging boundary

MeoUI and MeoKDE are required runtime dependencies, not copied source trees. A
future distribution package must depend on the packages that install the
`MeoUI`, `MeoKDE`, and `Meo.System` QML modules in Qt's import path; this
repository intentionally does not vendor or silently bundle them. The
development CMake configuration validates explicit import roots when supplied,
and the QML smoke test supplies all three roots for the sibling-build workflow.
The Meo desktop package must also depend on this application's package before
its Quick Settings gear can rely on `org.meo.settings.desktop` and the
route-specific `org.meo.settings.bluetooth.desktop`; the applet uses desktop
IDs rather than assuming that a `meo-settings` binary exists.

OmniStore is an optional runtime integration, not a bundled dependency.
Settings discovers `omnistore-apps-export` and `omnistore` at runtime, and its
unit tests validate only the exported JSON contract. This repository makes no
claim that the installed OmniStore package or exporter works until package-side
acceptance provides that evidence.

The Meo Settings package also installs its `org.meo.Settings` Meo Account
client manifest under `share/meo-account/clients`. The Meo Account image
integration must replace its registered OAuth client placeholder before this
application is allowed to request an authorization flow; Settings itself does
not start that flow and uses the manifest solely for read-only profile scope.
