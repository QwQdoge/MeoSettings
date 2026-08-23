# Meo Settings architecture

This document describes the implemented architecture and its deliberate
boundaries. Meo Settings is the normal, primary daily settings application for
MeoArch; it is not a second-choice skin over System Settings. KDE services
remain the authority for their system state, privileges, and recovery paths.
The document is a contract for future work, not a promise that every KDE
setting has a native Meo page.

## Purpose and ownership

Meo Settings is the MeoArch application for normal day-to-day configuration.
It gives the common workflow a single MeoUI-native home while retaining the
KDE services and tools that already own the underlying state. It does not fork
System Settings and does not replace NetworkManager, BlueZ,
PipeWire/PulseAudio, KScreen, Solid, KWin, or PowerDevil. KDE KCMs are an
explicit advanced, compatibility, or recovery handoff—not the ordinary
navigation destination for a supported native workflow.

| Owner | Responsibility | Does not own |
| --- | --- | --- |
| **MeoUI** | Shared tokens, responsive primitives, navigation, page transitions, setting rows/groups, and task-sheet surfaces. | KDE configuration, device policy, or desktop-theme writes. |
| **MeoKDE** | The platform theme bridge (`MeoShellTheme`) and the installed `Meo.System` runtime. It supplies the complete active-session HCT/Material role table. | This application's registry, pages, or app-specific safety policy. |
| **Meo Settings** | The primary daily information architecture, capability gating, native pages/actions, backend adapters, and safe handoff decisions. | Pretending a missing backend or privileged workflow is implemented. |
| **KDE services/KCMs** | Device services and complex/privileged configuration that KDE already maintains. | Meo Settings' visual layout and navigation model. |

`Main.qml` calls `MeoShellTheme.sync()` before normal page use and requires a
complete dynamic role table before the app considers a page ready. MeoUI,
MeoKDE, and `Meo.System` are runtime dependencies, not source trees copied
into this repository.

Most pages observe the active platform theme, but Appearance has one explicit
Meo-owned, confirmation-gated action: `DynamicColorBackend` invokes the
installed `meo-dynamic-colors` generator with exactly one validated source:
KDE accent, the configured local wallpaper, or a manual `#RRGGBB` seed. That
native MeoKDE tool generates the complete HCT/CAM16 Material role table and
applies the resulting KDE scheme as one operation. Settings never synthesizes
a parallel QML palette or runs this write implicitly; its confirmation surface
states that the desktop palette will change and that Plasma and KWin are not
restarted. Wallpaper mode fails visibly for a slideshow, remote source, or
missing local image instead of sampling a different file.

## Navigation contract

Settings has two persistent route levels and one temporary task surface:

1. **Level 1 — index/category.** Home search and the category sidebar point
   to stable categories from `SettingsRegistry`.
2. **Level 2 — complete page.** A native page, a category page, or the
   explicit KDE handoff page owns a full task. These are the only persistent
   destinations.
3. **Level 3 — temporary task.** `MeoSettingsTaskSheet` is owned by its
   level-2 page for a small fact view, confirmation, or one-off choice. It is
   never added to `SettingsRegistry` as a route. Closing, accepting,
   rejecting, or navigating away retracts it.

On desktop, a task opens as a right side sheet. Below the compact threshold
(`680 * MeoTheme.globalScale`) it opens as a bounded bottom sheet instead; an
open sheet migrates when the window crosses that threshold. This protects the
two-level information architecture on small windows rather than nesting a
third persistent page.

`MeoWindowMetrics` supplies the five shared width classes. Settings maps them
to navigation as follows:

| Width class | Navigation surface |
| --- | --- |
| Compact | Top app bar plus a temporary category drawer. |
| Medium | Icon rail. |
| Expanded | Expanded rail. |
| Large | Persistent drawer. |
| Extra large | Persistent drawer with the widest content allowance. |

`MeoPageHost` owns route transitions. Motion follows the shared MeoUI motion
tokens and is disabled when the active accessibility/reduced-motion setting
requests it; pages do not introduce local, unrelated transition rules.

## Information architecture and visibility

`SettingsRegistry` is the source of truth for categories, entries, route
metadata, keywords, owner, presentation, risk, and optional device
capability. Home search uses the same registry rather than a second list of
features. Category pages identify KDE-owned rows visibly and do not enable a
KCM handoff until the KCM bridge finds its plugin metadata.

| Category | Current primary Meo surface | Current advanced/recovery boundary |
| --- | --- | --- |
| Network & Internet | Wi-Fi | Wired connections, VPN, proxy, hotspot, and advanced wireless cases use KDE networking controls. |
| Connected devices | Bluetooth adapters, discovery, explicit pairing/authentication, connection, trust, block, rename, and removal | Device-specific profile tuning, codecs, and hardware firmware configuration remain KDE-owned. |
| Display & sound | Display inventory; direct per-display brightness; direct Night Light enable/disable; audio output/input, volume, mute, and default-device control | Display topology plus Night Light schedules and temperature remain in maintained KDE tools. |
| Wallpaper & style | Active appearance inspection plus an explicit, confirmation-gated dynamic-color source picker for KDE accent, configured local wallpaper, or manual seed. The MeoKDE generator makes the complete HCT/MD3 table. | Look and feel, contrast, wallpaper selection, icons, fonts, and compatibility choices retain their maintained KDE workflows. |
| Apps & notifications | Direct global Notifications page: manual/automatic Do Not Disturb, popup duration/position/visibility, background-task notifications, taskbar badges, and per-application popup/history/DND/badge rules through `NotificationManager.Settings`. | Default apps, MIME associations, and autostart retain their maintained KDE workflows. |
| Accounts & sync | Manifest-scoped Meo Account status/profile surface; the local-user control is a button within that page | Account sign-in/out and system account changes use the maintained Meo Account application; KAccounts and KWallet use KDE modules. |
| Storage & backup | Read-only mounted-volume facts, explicit system-volume context, bounded user-requested images/videos/documents/audio/known-AI category scans, an explicit local Pacman repository versus foreign/AUR-candidate size inventory, plus an optional read-only OmniStore managed-app usage overview. | Automount, disk health, backup, restore, partitions, encryption, formatting, package actions, and filesystem-wide usage analysis stay in maintained tools. |
| System | Direct power-profile selection, primary-battery facts, a user-invoked screen-lock action, and Meo Control Center tile layout/density | Power schedules, critical-battery actions, time, region, search indexing, workspace, and session policy use KDE modules. |
| Privacy & security | Read-only per-application Flatpak effective-permission inventory; this is deliberately not presented as a universal native-app permission history. | Screen lock, firewall, and device-security views use KDE modules; Linux has no trustworthy global native-app access-history API to replace with a fake log. |
| Accessibility | No native mutable page yet | Assistive technology and virtual keyboard use KDE modules. |
| Updates & support | Read-only cached pacman update grouping for Meo, KDE/Plasma, system, configured custom repositories, and an explicit AUR query | Repository refresh and any install/remove/upgrade remain the installed system updater's workflow. |
| About | Read-only MeoArch and device facts | Advanced distro information can open its KDE module when installed. |

The optional `capability` field is a visibility guard, not a claim that an
arbitrary generic tool can operate a device. At present the app reacts to
Wi-Fi, Bluetooth, audio, and display availability. Hardware-specific controls
inside an aggregate native page are hidden when their real backend is
unavailable, while the aggregate page itself remains discoverable so a second
supported backend (for example Night Light without KScreen inventory) is not
hidden. In category pages, KCM entries stay visible but disabled with an
explicit not-installed explanation when their module is absent; a search result
can instead reach the same handoff page, which reports that absence before any
launch is attempted.

## Native backend policy

Every interactive native control has a system-backed adapter or direct
platform controller and publishes availability, busy state where relevant,
errors, and reactive state changes. The application does not use generic
command-line control tools such as `nmcli`, `pactl`, `bluetoothctl`, or
`kscreen-doctor` as a device UI backend. Two named Meo integration contracts
are deliberately narrow exceptions: `meo-dynamic-colors --apply` is the sole
writer for a complete Meo/KDE dynamic scheme, and `omnistore-apps-export` is a
read-only, schema-validated application-usage exporter.

| Adapter | Backing API | Current safe operations |
| --- | --- | --- |
| `NetworkBackend` | NetworkManagerQt | Enable/disable Wi-Fi, scan, activate a saved connection, create supported new profiles, and disconnect. New profiles explicitly choose disk persistence or NetworkManager's volatile persistence. Unsupported security is handed to the advanced NetworkManager UI. |
| `BluetoothBackend` | BluezQt plus a short-lived private `org.bluez.Agent1` | Power adapters, scan, pair through explicit PIN/passkey/confirmation/authorization prompts, connect/disconnect, trust/untrust, block/unblock, rename, and remove a device. Device actions use BlueZ object paths rather than ambiguous addresses. |
| `AudioBackend` | PulseAudioQt (including PipeWire's compatible session) | Inspect outputs/inputs, choose defaults, adjust volume up to the supported 150% range, and mute/unmute. |
| `DisplayBackend` | KScreen | Read active output inventory and refresh it. |
| `Meo.System::Platform` | PowerDevil ScreenBrightness, KWin Night Light, UPower PowerProfiles, and the KDE screen-lock service | Adjust reported display brightness, toggle Night Light, select a published power profile, and lock the current session after an explicit user action. |
| `PowerBackend` | Solid | Read the actual primary-battery percentage, charge state, and remaining-time estimate for Home and the native Power page. |
| `DynamicColorBackend` | Installed `meo-dynamic-colors` | After explicit confirmation, regenerate and select the complete Meo HCT/Material scheme from KDE accent, configured local wallpaper, or a validated manual seed. It surfaces tool availability, busy state, and errors; it does not restart Plasma or KWin. |
| `ControlCenterBackend` | Active Plasma Shell `org.meo.topbar` applet | Read and write the one active Meo Quick Settings layout: known tile order, compact/wide span, visibility, and compact/comfortable/spacious density. |
| `StorageBackend` | Qt storage information plus Solid | Inspect representative mounted filesystems, mount points, capacity, removable-drive context, and an explicit bounded Home-subfolder category scan without changing a disk. |
| `OmniStoreAppsBackend` | Optional `omnistore-apps-export` contract | Read only a bounded, versioned OmniStore managed-app snapshot and summarize source mix plus exact/reported/unknown application-size metadata. |
| `PackageInventoryBackend` | Local `pacman -Qi` then `pacman -Qqm` with `LC_ALL=C` | On explicit request only, report installed repository-package versus foreign/AUR-candidate counts and package-file sizes. It never refreshes metadata, uses privilege, calls a helper, or performs a package action. |
| `SystemInfoBackend` | Qt/system facts | Read device and OS facts for About. |
| `NotificationManager.Settings` | Plasma notification manager and `plasmanotifyrc` | Persist global Do Not Disturb, popup, background-job, automatic-inhibition, taskbar-badge, and known-app popup/history/DND/badge preferences. The running Plasma notification applet remains the owner of the live server and notification history. |
| `AppPermissionsBackend` | Local `flatpak list` and `flatpak info --show-permissions` | Read locally installed Flatpak app IDs and effective sandbox metadata only after the user opens/refeshes the page or opens an app detail sheet. It never grants, revokes, launches, or claims native-app usage history. |
| `KcmBridge` | KDE plugin metadata plus `kcmshell6` | Discover a known installed KCM and open it only after an explicit user action. It does not expose a generic process launcher to QML. |

Network credentials are passed to NetworkManager only for the requested
connection. The QML page clears the entered password immediately and does not
implement a second Secret Agent or KWallet credential store. A saved choice
creates a disk-backed profile; an unsaved choice creates a volatile profile
that NetworkManager removes after disconnecting.

### Dynamic color action

The Appearance page selects one source in a temporary task sheet and asks for
explicit confirmation before calling `DynamicColorBackend.applySource()`. The
backend discovers only the named `meo-dynamic-colors` executable and passes a
fixed argument list: `--source accent`, `--source wallpaper`, or `--source
manual --accent #RRGGBB`, followed by `--apply --remember-source`. A manual
seed is validated as exactly `#RRGGBB`; Settings never constructs a shell
command or derives colors in QML.

The generator, rather than Settings, resolves the actual source, computes the
full HCT/CAM16 Material table, persists the source only on success, and applies
one complete KDE scheme update. Wallpaper mode samples only the configured
local `org.kde.image` background and errors for a missing, remote, or slideshow
source rather than silently using another image. MeoUI and MeoKDE consumers
receive the same complete role table and reject an incomplete one.

This is the normal Meo route for refreshing the Meo dynamic scheme. It is not
a silent background write, a partial role-table patch, or a substitute for
advanced KDE color/contrast, wallpaper selection, font, icon, and compatibility
flows. Those remaining flows keep their visible KCM handoff. A successful
process exit is still not proof that every live Plasma client has accepted the
updated palette; that requires live-session acceptance.

### Meo Control Center

The native **Control Center** page configures the Meo-owned Quick Settings
surface rather than redirecting a daily workflow into a generic KCM. Its
backend connects to the active Plasma Shell and targets exactly one
`org.meo.topbar` applet. It reads and writes only that applet's `Appearance`
configuration through Plasma Shell, then asks that applet to reload its
configuration. It never edits `plasma-org.kde.plasma.desktop-appletsrc`
directly and refuses to write when there is no matching applet or more than
one matching applet.

The contract is intentionally closed: the supported tile IDs are Wi-Fi,
Bluetooth, Focus, Night Light, Keep Awake, Power Mode, Microphone, Sound,
Displays, and Screenshot. The page can reorder them, choose a compact or wide
span, show or hide them while retaining at least one visible tile, choose
compact/comfortable/spacious density, or restore the default layout. Plasma
Shell remains the authority that persists and reloads the applet state. This
is Meo-specific configuration, so there is no KCM fallback or arbitrary
applet-file fallback.

### Optional OmniStore app-usage overview

Storage can additionally present a **read-only** overview of applications
that OmniStore reports as managed. `OmniStoreAppsBackend` discovers only the
documented `omnistore-apps-export` program, runs it asynchronously without
arguments, and accepts only the bounded `org.meo.omnistore.installed-usage`
version-1 JSON contract. It validates the status, timestamp, application and
source records, counts, size totals, size evidence (`exact`, `reported`, or
`unknown`), and payload limits before exposing a source mix and at most the
six largest reported application records.

The overview is not a filesystem scan and not a claim about total disk use:
it excludes dependencies, caches, personal data, install paths, and any size
outside OmniStore's known application metadata. The Settings backend never
starts OmniStore's Flutter GUI to gather data, talks to a private OmniStore
daemon, or installs, removes, updates, cleans, or otherwise manages packages.
Opening `omnistore` is a separate explicit user action when a launcher is
found.

The exporter is optional. If it is absent or returns an invalid snapshot,
mounted-volume facts remain available and the page makes the missing/failed
overview clear. Parser/unit coverage proves that Settings can reject malformed
or incompatible contract data; it does **not** prove that an installed
OmniStore package or its exporter works on this system. Treat that package
integration as unverified until the package-side acceptance evidence exists.

### Explicit local Pacman package inventory

Storage also has a separate **Inspect installed packages** action for systems
without an OmniStore exporter. `PackageInventoryBackend` runs only `pacman
-Qi`, followed by `pacman -Qqm`, under `LC_ALL=C`; both commands read Pacman's
local installed database and do not refresh repositories, access the network,
use sudo, invoke an AUR helper, or modify package state. The process has a
bounded output and timeout, and is never started when the Storage page loads.

The result separates Pacman repository packages from Pacman foreign packages.
The latter are labelled **Foreign / AUR candidates** rather than simply AUR:
a package built locally can be foreign too. Reported installed package-file
sizes are not application share, cache size, filesystem allocation, or a
second system-volume total, so the UI never adds them to mounted capacity or
OmniStore's application figures.

### Meo Account profile projection

`MeoAccountBackend` only reads `GetStatus` and, when the broker reports a
signed-in session, asks for `GetIdentity("org.meo.Settings")` through the
broker's existing manifest-and-executable gate. The installed
`data/meo-account/clients/org.meo.Settings.json` manifest requests only
`openid profile`; it grants Settings no token, refresh credential, password,
or arbitrary client launch capability. The account broker owns KWallet and
opens the maintained Meo Account application for account mutation. Settings
does not call its sign-in/sign-out methods.

The public status can provide a signed-in name/avatar. A raw cloud ID is shown
only when the manifest-scoped identity reply actually contains one. When that
reply is denied or the manifest is not yet installed, Settings intentionally
keeps the ID absent rather than falling back to a local Unix login or a made-up
identifier. Once signed in, the Settings home and About identity surfaces use
the cloud identity instead of showing the local account as the primary user.
The local-user KCM remains a single explicit button inside the Accounts page.

### Read-only system update projection

`UpdatesBackend` runs only `pacman -Qu`, `pacman -Qqm`, and `pacman -Si` with
`LC_ALL=C` against local package/sync databases. It bounds output and package
count, parses only well-formed package records, reads active repository section
names from `/etc/pacman.conf`, and records the newest local sync database time.
It does not run `pacman -Sy`, any `pacman -S*` action, sudo, PackageKit action,
or an OmniStore update path. A missing or unreadable local cache is an explicit
absence/error, never a false "up to date" state.

Package names and local sync repository metadata place records into Meo,
KDE/Plasma, system, or configured-custom-repository sections. AUR helpers are
not invoked automatically: a user opens a confirmation sheet before Settings
runs the configured `paru -Qua` or `yay -Qua` query. That query has no
install/update arguments, but may contact the helper's remote metadata service,
so its result is isolated from the local pacman snapshot. The installed KDE
updater remains the explicit handoff for refreshing repositories and making
changes.

The Notifications page must not assign `NotificationManager.Server.inhibited`
from the standalone Settings process. That singleton is local to the owning
process. Instead it uses the same persisted `notificationsInhibitedUntil`
contract as Plasma's official notification applet and calls `save()`. The
running Plasma surface observes that config change, updates its live server,
and remains the sole source of notification history, unread state, actions,
replies, and jobs. Turning off Meo's manual Do Not Disturb clears only the
manual persisted deadline; it does not revoke a separate application's or an
automatic presentation/fullscreen inhibition.

For every application that Plasma has actually observed,
`NotificationManager.Settings` also exposes that application's persisted
behavior flags. The Meo per-application task sheet changes only the real
popup, history, Do Not Disturb override, and taskbar-badge flags, then calls
`save()`. It does not create invented app records, rewrite notification
history, or launch an application merely to make it appear in the list.

### Privacy permission inventory

`AppPermissionsBackend` intentionally provides a narrower but verifiable
answer than a fake universal permission dashboard. It lists locally installed
Flatpak apps and parses their `flatpak info --show-permissions` effective
metadata into shared services, sockets, devices, files, persistent folders,
sandbox features, and bus policy rows. The command runs only after the Privacy
page is opened/refreshed or a specific app is inspected; no grant, revoke, app
launch, or network request occurs.

Native Linux desktop applications do not share a trustworthy global API that
answers which app historically used camera, microphone, files, or screen
capture. Meo states that boundary in the UI and documentation rather than
manufacturing an activity history. Screen lock, firewall, and device-security
operations remain explicit maintained-tool handoffs.

KCMs are launched externally rather than embedded. Some installed KCMs are
QWidget-based and cannot be made a safe Qt Quick child simply by placing them
inside a QML item. The KCM handoff page makes that transition explicit instead
of presenting a visually native but non-functional copy.

## Deliberate deferrals

The following are intentionally not represented as fake toggles or incomplete
native editors:

| Deferred area | Why it is not a native Meo action yet | Prerequisite for promotion |
| --- | --- | --- |
| Display topology writes | A bad topology can remove the user's only usable output. Brightness is promoted separately because PowerDevil exposes it per display without changing topology. | Confirmation timer, automatic rollback, live multi-output testing, and a recovery-safe backend. |
| Power schedules and critical-battery policy | Profile selection is a stable service-owned operation, but timers, device actions, and critical thresholds need device-specific recovery and policy coverage. | A reviewed PowerDevil policy backend with permission, error, and live hardware acceptance coverage. |
| Disk mutation | Formatting, partitioning, encryption, repair, and restore are privileged and recovery-sensitive. | Privilege model, confirmed target identity, progress/cancellation, recovery path, and destructive-operation tests. |
| Advanced appearance and color policy | Meo can explicitly regenerate one complete dynamic scheme from accent, configured local wallpaper, or manual seed, but arbitrary theme selection, contrast policy, wallpaper selection, icons, fonts, and compatibility settings have different owners and rollback needs. | A reviewed per-setting ownership, persistence, recovery, and live-client acceptance contract; until then, use the named KDE KCM handoffs. |
| OmniStore package actions and disk-use claims | The optional overview is only schema-validated managed-app metadata; it is not a package manager, cleaner, installer, uninstaller, updater, or filesystem accounting tool. | Package-side exporter acceptance plus a separately designed, privileged action/recovery workflow for any mutation. |
| Account mutation, universal privacy history, accessibility, package mutation, backup breadth | Meo can read manifest-scoped account state, effective Flatpak metadata, and cached update facts, but credentials, a universal native-app permission history, user policy, repository refresh, and mutations remain protected owner workflows. | A stable public API plus complete privilege, cancellation, recovery, and live acceptance coverage; otherwise retain the Meo Account/KDE owner. |

This is a product-safety rule: a row is native only when the system API,
permission model, error recovery, and user workflow are all implemented. A
visually complete control without those properties is worse than a clear KDE
handoff.

## Relationship to MeoKDE `SystemStateHub`

MeoKDE already has a `SystemStateHub` for shell surfaces, but it must not be
imported as the Settings backend by default. It overlaps Wi-Fi, Bluetooth, and
audio state, yet its policy is not the same:

- `SystemStateHub::connectWifi()` creates a disk-persistent profile and turns
  on autoconnect. `NetworkBackend` deliberately exposes the user's saved
  versus volatile choice.
- `SystemStateHub::toggleBluetoothDevice()` only connects or disconnects an
  existing pairing. Its compact surface opens Meo Settings for a new device;
  it never begins a pairing, auto-trusts a device, or depends on a default
  authentication agent.
- `BluetoothBackend` owns the explicit pairing transaction. It registers its
  private `org.bluez.Agent1` object with the `KeyboardDisplay` capability for
  a Meo-initiated `Pair()` call only, and never calls `RequestDefaultAgent`.
  Incoming/background pairing therefore continues to use the established
  system agent when Settings is not open.
- The shell hub combines several domains behind shared operation state. The
  Settings adapters expose per-page availability, error, and safety policy,
  and also cover display, storage, system facts, and KCM discovery.

The M0 proposal is therefore a **versioned shared C++ controller contract**,
not a cross-process singleton or a shared QML object. It should centralize
observable device state and typed requests while leaving presentation and
policy at the caller:

1. Define request types that preserve intent, such as Wi-Fi persistence,
   explicit credential handling, and an operation-specific error/result.
2. Keep Bluetooth pairing as an explicit transaction: use a BlueZ object path
   as device identity, route all agent callbacks to an explicit task sheet,
   never auto-trust or auto-connect, and never make Meo the default agent.
3. Let both shell and Settings adapt that contract to their own UI and safety
   boundaries, with Settings retaining its KCM handoff decisions.
4. Migrate only after parity tests cover saved and volatile Wi-Fi profiles,
   Bluetooth agent replies/cancellation, paired-device behavior, busy/error
   propagation, and the absence of unintended live actions.

Until that work exists, the duplicated adapters are intentional: replacing
them with a superficially shared singleton would change user-visible behavior.

## Validation and acceptance boundary

The repository supports several useful but limited checks:

- The registry test checks the information-architecture metadata, search
  aliases, capabilities, and direct-route classifications.
- The QML smoke test navigates the current supported route set, including
  Appearance, Notifications, Privacy, Accounts, Updates, Control Center,
  Storage, native Power, and Display, and verifies that the MeoKDE dynamic
  color bridge delivered a complete role table. It performs no hardware action
  and does not launch a KCM.
- The compact task-sheet smoke test opens and closes the adaptive bottom-sheet
  presentation in an offscreen QML runtime.
- The Control Center backend tests validate its closed tile schema,
  normalization, serialization, and generated Plasma Shell script without
  changing a live applet.
- The OmniStore overview tests validate bounded parsing of representative
  version-1 JSON payloads. They do not invoke an installed
  `omnistore-apps-export` program or prove package compatibility.
- The local package-inventory parser test validates C-locale installed-size
  parsing and the official versus foreign/AUR-candidate split without running
  Pacman against the host database.
- The Updates backend tests validate C-locale pacman parsing, configured
  repository filtering, and Meo/KDE/custom/system classification. They never
  run pacman, an AUR helper, a repository refresh, or a package action.
- The Meo Account backend tests validate profile-text and HTTPS-avatar
  sanitization. They do not start a broker, open OAuth, access KWallet, or
  prove that a production manifest has been registered.
- The notification-settings API smoke uses an isolated `XDG_CONFIG_HOME` to
  prove the actual `NotificationManager.Settings` load/dirty/save round trip
  without modifying the active user's `plasmanotifyrc`.
- The dynamic-color backend test validates the fixed safe invocation for each
  source and rejects malformed manual colors without spawning the generator.
- The Flatpak permission parser test checks effective context and bus-policy
  grouping without invoking `flatpak` or touching a real permission grant.
- The storage test covers bounded category recognition, unsafe-root rejection,
  symlink rejection, cancellation, and partial-result reporting; it does not
  scan a user's Home directory.
- Exact-size offscreen screenshots make the five shared window classes and
  pages reviewable without changing any system setting.

Those checks prove source/build/QML integration only. They do **not** prove a
real Plasma-session Wi-Fi connection, credential persistence behavior,
Bluetooth discovery or paired-device connection, PipeWire routing, KScreen
inventory, brightness writes, Night Light toggling, power-profile selection,
KCM launch, confirmation-gated dynamic-color application, live palette
propagation after a scheme change, Control Center targeting/persistence/reload
against a real top-bar applet, an installed OmniStore exporter/package, live
Do-Not-Disturb propagation through the running Plasma notification applet, a
running Meo Account broker/manifest-scoped cloud-profile response, cached and
remote AUR update query behavior, or
recovery from a destructive operation. Each mutable capability needs a manual
live-session acceptance run before it is described as verified on a device.

## Change checklist

When adding a setting, first answer these questions in the implementation or
review:

1. Which component owns the state and permission model?
2. Is the row native, a KCM handoff, or read-only information? Record that in
   `SettingsRegistry` metadata.
3. Does the page fit the persistent two-level architecture, or is it a
   temporary `MeoSettingsTaskSheet` task?
4. Is the hardware-specific entry capability-gated and is the fallback clear?
5. Can the action fail safely, recover, and be tested in a real session?
6. If it uses a named external contract, is its input bounded, its authority
   explicit, and its package/live acceptance status documented honestly?

If any answer is missing, keep the entry read-only or KDE-owned rather than
adding an imitation control.
