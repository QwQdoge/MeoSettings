# Backup & Restore

Meo Settings owns the user-facing backup and restore experience for MeoArch.

Backup is a system/settings concern, not a Meo Account settings page. Meo Account remains the identity and authorization authority when a cloud destination is used, but it does not own backup UI, backup scheduling, backup selection, or restore workflows.

## Product boundary

The `Storage & backup` area in Meo Settings is the single user-facing home for:

- local backup creation and restore;
- optional Meo Account cloud backup destinations;
- selected Meo settings and preferences;
- application inventory / reinstall lists supplied by OmniStore;
- selected user data when a real backend can enumerate and copy it safely;
- backup status, history, size, and destination;
- restore preview and explicit restore confirmation.

The following remain separate:

- disk capacity, mounted volumes, and storage inspection stay in Storage;
- system snapshots, fallback kernels, repair, kernel/modprobe configuration, and OS rollback stay in Recovery;
- identity, MFA, sessions, OAuth, and provider credentials stay in Meo Account;
- privileged system changes continue to use their owning Meo.System / KDE authority.

## Security rules

An ordinary backup must never contain raw secrets. In particular it must exclude:

- Meo Account access or refresh tokens;
- browser sessions or session cookies;
- device enrollment / relay credentials;
- Supabase service-role or other server-only credentials;
- AI provider API keys;
- KWallet entries or password-store contents;
- OAuth client secrets or authorization codes;
- passwords, MFA seeds, recovery codes, or passkey private material.

A backup may record non-secret metadata needed to rebuild configuration, such as a provider name or an application identifier. After restore, secret-backed services must reconnect through their normal authorization flow rather than receiving a copied credential.

Cloud backup must use the signed-in Meo Account only as an authenticated destination boundary. A desktop client must never receive a service-role key. Server-side storage remains owner-scoped, and a restore request must verify the current account again before destructive replacement of existing user state.

## Manifest v1

The current local format is `org.meo.backup/v1`. Meo Settings writes it with `QSaveFile` to `Documents/Meo Backups` and caps an inspected manifest at 2 MiB.

The format is deliberately fail-closed. Unknown top-level fields, unknown content groups, unknown application fields, unknown setting products, incomplete setting groups, unsafe values, or a manifest claiming `secretsIncluded: true` are rejected.

`contents.applications` is a reinstall list projected from OmniStore's already validated snapshot when that snapshot is available. An entry contains only:

- `id`;
- `sourceId`;
- optional `version`.

`contents.applicationsState` records whether the application inventory was actually captured. Its only accepted values are:

- `included`: the list was built from a verified OmniStore snapshot;
- `unavailable`: OmniStore inventory was unavailable or busy, so the application list is intentionally empty.

An unavailable application inventory does not block a Meo settings backup and must never be interpreted as proof that the device has zero applications. A manifest claiming `applicationsState: unavailable` while carrying application rows is rejected.

Application names, filesystem locations, cache/data paths, package-manager private metadata, and application settings are not copied into the reinstall list.

`contents.settings` is an object containing only explicitly portable Meo-owned presentation settings. The v1 allowlist currently understands:

- Control Center quick-tile order, span, visibility, and density;
- Meo top-bar presentation and visibility options;
- Shelf / Launcher presentation options;
- Meo notification-surface presentation options;
- Time Center presentation options;
- Top Tasks limit.

`BackupBackend` is the only coordinator that projects live Control Center and Shell state into this portable schema. QML asks for a backup but does not construct or duplicate the schema. Runtime-only fields exposed by the source backends are stripped before validation.

The portable schema stores semantic values, not paths to Plasma/KConfig files. Restore therefore submits these values back through the owning Control Center / Shell capabilities rather than copying or editing Plasma configuration files directly.

`contents.userData` is currently required to be an empty array. Arbitrary home-directory or application-data copying is not part of manifest v1 yet.

## Restore rules

Restore is never an implicit side effect of sign-in or sync. The user must be able to see what will change before applying it.

The current portable-settings restore coordinator follows these rules:

1. preview validates the complete manifest before presenting any restore plan;
2. Apply reopens and revalidates the selected file instead of trusting stale preview state;
3. only whitelisted portable Meo presentation settings are queued;
4. settings are applied serially in the order Control Center, Top Bar, Shelf, Notifications, Time Center, Top Tasks;
5. a step advances only after the owning backend emits its explicit saved signal;
6. backend errors or a 30-second confirmation timeout stop the restore immediately;
7. partial completion is reported explicitly; later groups are not attempted after a failure;
8. applications, user data, accounts, credentials, KWallet contents, and other secrets are never changed by this coordinator.

The coordinator deliberately does not implement a pretend rollback across independent Plasma surfaces. If an early group has already been confirmed and a later group fails, the confirmed earlier changes remain applied and the result is reported as a partial failure. A future rollback design must use real per-surface snapshots or another verified recovery mechanism before it can claim transactional rollback.

System rollback is not a substitute for user-data restore and stays under Recovery.

## Restore plan model

After validation, `BackupBackend.previewPlan` exposes a structure-only plan to QML. QML does not parse the manifest itself. The current plan contains:

- Applications: `included` or `unavailable`, with reinstall-record count when available;
- Control Center;
- Top Bar;
- Shelf & Launcher;
- Notifications;
- Time Center;
- Top Tasks;
- User data: currently `not-included`;
- Accounts & secrets: always `not-included` for manifest v1.

The six portable settings groups are the only categories currently eligible for Apply. The application reinstall list remains preview metadata until OmniStore has an explicit reinstall transaction with its own validation and failure reporting.

## Current implementation status

Implemented and validated in automated tests:

- Meo Settings owns the visible `Storage & backup` surface;
- a real local `Back up now` action writes a bounded v1 manifest;
- `BackupBackend` projects the current available Control Center and Shell presentation state into the portable settings object;
- OmniStore application identifiers are projected into a reinstall list when the verified inventory is available;
- an unavailable OmniStore inventory is represented explicitly and does not block a settings backup;
- the portable Control Center / Shell settings schema is covered by a dedicated fast contract CI in addition to the full Arch build/test workflow;
- ordinary manifests reject secret flags, unexpected fields, unsafe application identifiers, inconsistent application-inventory state, unsupported setting values, symlink restore inputs, and oversized files;
- the Storage & backup page exposes a local-file restore picker and structured category-level restore preview;
- restore preview accepts only local file URLs and validates the complete manifest before presenting it;
- Apply reopens and revalidates the manifest, so a file changed after preview is rejected;
- the portable-settings coordinator serializes the six Meo-owned setting groups and advances only on explicit saved signals;
- coordinator tests verify the exact success order and verify that a Notifications failure prevents Time Center and Top Tasks from running;
- a manifest with no portable settings is rejected by Apply rather than treating application metadata as a supported restore operation;
- previous restore outcome state is cleared when the user selects a different manifest through the normal file-picker path;
- the previous Meo Account `Library` / backup UI and Account search destination have been retired.

Still requiring product/UI or live-runtime validation:

- the final explicit Apply confirmation surface in `Storage & backup`;
- live Plasma validation that each real Control Center / Shell saved signal and error path behaves as expected during a restore;
- application reinstall execution through OmniStore;
- user-data payloads or encrypted user-data archives;
- scheduled backups;
- cloud backup destinations.

The automated coordinator tests use fake QObject capability surfaces. Passing those tests proves sequencing, validation, timeout/error-state logic, and partial-failure behavior at code level; it does not by itself prove a live Plasma restore.

Existing historical server-side backup data is intentionally not deleted during the UI move. It can be migrated or retired only after Meo Settings has an explicit compatibility path.

## Next implementation gates

1. Finish the explicit confirmation sheet and outcome presentation in `Storage & backup` without exposing application reinstall as supported Apply behavior.
2. Run a live Plasma restore using a disposable settings backup and verify all six real saved signals plus at least one forced failure path.
3. Add application reinstall only through an OmniStore-owned reinstall transaction with source availability checks, preview, confirmation, and per-app failure reporting.
4. Add user-selected data only after a bounded path/category contract and local encrypted archive design exist.
5. Add scheduled local backups only after the local manifest and restore transaction are stable.
6. Add an optional Meo Account cloud destination only after local format and restore behavior are stable.
