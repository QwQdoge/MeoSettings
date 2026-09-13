# Lock screen and login settings contract

## Status and authority

The P3 **Lock screen & login** page now exists as a data-only in-session
preview and session-only layout editor. Its switches and widget placement change
only the preview draft, never a real lock screen,
PAM policy, display manager, KScreen layout or current system configuration.
The validated lock-screen writer and privileged Login Manager writer do not
exist yet, so current Settings continues to hand existing lock-screen policy to
`kcm_screenlocker`; `LoginAuthBackend` only detects Plasma Login Manager or
the SDDM compatibility adapter and deliberately does not infer a login PAM or
fingerprint policy.

The formal presentation schema is owned by MeoKDE at
`/home/shekong/Projects/meo-kde/docs/schemas/meo-session-entry-v1.schema.json`.
The Plasma Login Manager fork owns the upstream greeter/authentication
boundary.  This application owns the editor, preview, validated configuration
requests and an explicit handoff when a supported backend is unavailable.

## Required settings flow

The page has two separate scopes, each presented with its real authority:

| Scope | Configuration authority | User workflow |
| --- | --- | --- |
| Lock screen | Current user's MeoKDE configuration | Inspect, edit validated visual/privacy/layout preferences, preview in-session, apply, restore defaults. |
| Login screen | Authorized system transaction for the Meo Login Manager | Inspect, plan, preview safe system assets, authorize, snapshot, apply, validate, commit or roll back. |

The page may edit only the schema's presentation values: clock/date/background,
wallpaper asset references, media/weather availability, notification privacy,
per-output layout and motion preference.  It must not offer fields for a
password, PAM module, fingerprint enablement, arbitrary QML, service enablement
or raw screen coordinates.  Fingerprint availability is displayed only after a
future transaction backend validates a password-preserving upstream PAM policy.

The page's layout editor is also a simulated, in-session surface. Its draft is
discarded when Settings closes until the validated MeoKDE writer exists. It
may reuse drag handles, snapping and optional grid guides from Plasma's editing
vocabulary, but it may load only the signed Meo secure-widget registry. A
desktop Plasma applet, arbitrary third-party QML, direct camera/face service,
or a desktop widget instance cannot cross into the KScreenLocker surface.
Face can only be presented after an explicit authentication gesture and only
when the upstream authenticator exposes a corresponding trusted method.

`LoginAuthBackend` remains a detection and capability reporter.  It is never a
credential broker.  The existing upstream KCM stays as the advanced and
compatibility entry until the corresponding Meo workflow has a tested native
backend.

## Preview, validation and recovery

For both scopes the implementation must follow this exact order:

`Inspect -> Validate -> Plan -> Preview -> Authorize -> Snapshot -> Apply -> Validate -> Commit -> Rollback`.

Preview renders a data-only mock surface inside the already unlocked Settings
session.  It cannot authenticate, switch the display manager, cover another
screen, prove screen-reader behavior, or substitute for a real KScreenLocker
or greeter test.  Its UI labels must say that clearly.

The system-login writer passes a closed, schema-validated request to the
authorized transaction service.  It creates a recovery snapshot before a
write, verifies that the persisted document still validates, and restores the
previous document on a failed validation.  It does not edit PAM files,
`plasmalogin.service`, `kscreenlocker`, KScreen's own layout data, or an SDDM
file.  If the writer/service is unavailable, the page remains read-only and
offers only the maintained KDE compatibility handoff.

**Restore defaults** deletes or replaces only the scoped Meo session-entry
document after showing the affected scope and fallback behavior.  It must not
remove assets, user media data, the upstream theme, fingerprints or any KDE
security configuration.

## Privacy and display policy

The lock-screen default shows only the notification count.  Application names,
full content, album artwork and precise weather location are independently
opt-in.  Login scope is stricter: no notifications/media/album artwork and
only a city-level system weather cache when it has been explicitly enabled.
Provider timeouts, unavailable hardware and disconnected displays make the
relevant card/option unavailable; they never prevent authentication.

Per-display preferences use an opaque KScreen output identity.  The page may
offer the three schema policies (`auto`, `fixed-primary`, and
`follow-interaction`) and a per-output wallpaper asset/fill mode.  It may not
write a live display layout directly.  Any future topology operation follows
the existing KScreen confirmation-and-recovery contract or stays a KDE KCM
handoff.
