# Lock screen and login settings contract

## Status and authority

The P3 **Lock screen & login** page contains two kinds of state and they must
not be described as if they have the same implementation status.

### Plasma / Meo Desktop lock-screen presentation

The ordinary Meo Desktop session currently runs Plasma 6 Wayland with KWin.
For this session, `LockScreenPresentationBackend` is a real persistent writer,
not a preview-only draft. It validates an allowlisted set of presentation
preferences and reads/writes the Meo KScreenLocker Look-and-Feel values in
`kscreenlockerrc` under `Greeter/LnF/General`.

The current persistent keys are:

- `showWeather`;
- `showWeatherLocation`;
- `showMediaControls`;
- `showAlbumArtwork`;
- `showAudioControls`;
- `showPerformance`;
- `showSystemSummary`;
- `showSessionControls`;
- `lockScreenNotificationVisibility`.

These keys are consumed by the Meo lock-screen Look-and-Feel configuration and
are covered by the Settings backend tests. This is therefore an implemented
per-user configuration path for the current Plasma/Meo Desktop lock screen.
It must not be regressed to a decorative preview or replaced by an unrelated
parallel settings file.

This implementation is intentionally narrow. It does **not** mean that the
entire future session-entry schema, arbitrary widget layout editing, per-output
layout persistence, wallpaper asset transactions, authentication policy, or
login-manager configuration has been implemented.

### Standalone Meo Session Lock

MeoKDE also carries a resident Quickshell/Wayland-session-lock implementation
for sessions that use that runtime, currently documented around the
Hyprland/UWSM path. Its runtime configuration and lifecycle are separate from
KScreenLocker's `kscreenlockerrc`.

Do not describe the standalone lock as the only canonical lock implementation
for every MeoArch session. The owning desktop/session decides which security
runtime is active. Settings must detect or be told the active supported
session before exposing runtime-specific controls; it must not write both
backends blindly.

If the standalone lock becomes a supported configurable user session, it needs
a maintained settings adapter that maps the same user-facing Meo contract to
that runtime without weakening its PAM/session-lock security boundary.

### Login screen and advanced layout editor

The privileged Login Manager writer does not exist yet. `LoginAuthBackend`
remains detection/capability reporting only and deliberately does not infer or
change login PAM, fingerprint policy, session-start policy, or credentials.

The richer lock-screen layout editor remains a simulated in-session editor for
capabilities that do not yet have a validated persistent writer. Preview-only
layout state must be labelled as such and must not be confused with the
implemented presentation toggles above.

The formal future presentation schema is owned by MeoKDE at
`docs/schemas/meo-session-entry-v1.schema.json`. The Plasma Login Manager fork
owns the upstream greeter/authentication boundary. Meo Settings owns the
user-facing editor, safe preview, validated configuration requests and an
explicit handoff when a supported backend is unavailable.

## Required settings flow

The page has two security scopes, each presented with its real authority:

| Scope | Configuration authority | User workflow |
| --- | --- | --- |
| Lock screen | Current user's active supported Meo lock-screen backend | Inspect, edit validated presentation/privacy preferences, preview where useful, apply, read back, restore defaults. |
| Login screen | Authorized system transaction for the Meo Login Manager | Inspect, plan, preview safe system assets, authorize, snapshot, apply, validate, commit or roll back. |

For the currently implemented Plasma/Meo Desktop presentation subset, Settings
writes only the allowlisted KScreenLocker Look-and-Feel presentation keys. It
must not expose a password, PAM module, fingerprint enablement, arbitrary QML,
service enablement, raw screen coordinates, or any setting that the underlying
security authority does not safely own.

Future schema-backed editing may cover clock/date/background, symbolic
wallpaper asset references, media/weather availability, notification privacy,
per-output presentation and motion preferences. Such fields are not considered
implemented merely because they exist in the schema or preview UI.

The layout editor is a simulated, in-session surface until a corresponding
validated writer exists. It may reuse drag handles, snapping and optional grid
guides from Plasma's editing vocabulary, but it may load only the reviewed Meo
secure-widget registry. A desktop Plasma applet, arbitrary third-party QML,
direct camera/face service, or a desktop widget instance cannot cross into a
security surface.

Face can only be presented after an explicit authentication gesture and only
when the active trusted authenticator exposes a corresponding method.

`LoginAuthBackend` is never a credential broker. The existing upstream KCM or
login-manager compatibility path remains available for advanced controls until
the corresponding Meo workflow has a tested native backend.

## Preview, validation and recovery

The target transaction sequence is:

`Inspect -> Validate -> Plan -> Preview -> Authorize -> Snapshot -> Apply -> Validate -> Commit -> Rollback`.

Not every low-risk per-user preference requires a privileged transaction, but
it must still validate input, write the authoritative backend, surface write
errors, and read back or refresh the authoritative value. The current
`LockScreenPresentationBackend` fulfils the narrow persistence role for its
allowlisted KScreenLocker presentation keys; broader session-entry transaction
work remains separate.

Preview renders a data-only mock surface inside the already unlocked Settings
session. It cannot authenticate, switch the display manager, cover another
screen, prove screen-reader behavior, or substitute for a real KScreenLocker,
standalone Wayland session-lock, or greeter test. Its UI labels must say that
clearly.

A future system-login writer must pass a closed, schema-validated request to an
authorized transaction service. It must create a recovery snapshot before a
write, verify the persisted result, and restore the previous validated state on
a failed validation. It must not edit PAM files, `plasmalogin.service`, a lock
runtime, KScreen topology data, or SDDM files ad hoc.

**Restore defaults** affects only the scoped Meo-owned presentation/configuration
values after showing the affected scope and fallback behavior. It must not
remove assets, user media data, the upstream security runtime, fingerprints or
KDE authentication configuration.

## Privacy and display policy

The lock-screen notification default is **count only**. Application names and
full content require a more permissive explicit choice. Precise weather
location is not exposed by default. Album artwork is independently controlled
and must not be treated as permission to expose notification content.

Login scope is stricter: no current-user notifications/media/album artwork and
only bounded system-owned data that the login-manager contract explicitly
permits. Provider timeouts, unavailable hardware and disconnected displays
make the relevant card/option unavailable; they never prevent authentication.

Per-display preferences use an opaque output identity when a supported writer
is implemented. Settings must not write a live display topology merely to
position lock-screen content. Any topology operation follows the maintained
KScreen confirmation-and-recovery contract or remains an advanced handoff.

## Completion boundary

The following are **implemented** for the Plasma/Meo Desktop lock-screen path:

- validated persistent presentation toggles/privacy value;
- `kscreenlockerrc` writer/reader for the Meo Look-and-Feel keys;
- reset-to-default behavior for that allowlist;
- Settings-side tests for the presentation backend.

The following remain **unfinished** and must not be claimed complete:

- privileged Login Manager configuration writer/transaction service;
- full schema-backed lock/login transaction pipeline;
- persistent secure-widget layout editor and per-output layout writer;
- equivalent configurable adapter for the standalone Quickshell lock runtime,
  if that runtime is exposed as a supported user-selectable session;
- real-session manual/runtime validation beyond source/backend tests.
