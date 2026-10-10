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

`contents.applications` is a reinstall list projected from OmniStore's already validated snapshot. An entry contains only:

- `id`;
- `sourceId`;
- optional `version`.

Application names, filesystem locations, cache/data paths, package-manager private metadata, and application settings are not copied into the reinstall list.

`contents.settings` is an object containing only explicitly portable Meo-owned presentation settings. The v1 allowlist currently understands:

- Control Center quick-tile order, span, visibility, and density;
- Meo top-bar presentation and visibility options;
- Shelf / Launcher presentation options;
- Meo notification-surface presentation options;
- Time Center presentation options;
- Top Tasks limit.

The portable schema stores semantic values, not paths to Plasma/KConfig files. It can therefore be validated without a running Plasma session. A future restore apply step must still submit these values through the owning Control Center / Shell backends so their current runtime validation remains authoritative.

`contents.userData` is currently required to be an empty array. Arbitrary home-directory or application-data copying is not part of manifest v1 yet.

## Restore rules

Restore is never an implicit side effect of sign-in or sync. The user must be able to see what will change before applying it.

A restore implementation must:

1. validate backup format and version before reading payload data;
2. reject unknown or unsafe paths rather than interpreting them as filesystem targets;
3. show the affected settings/data categories;
4. keep secret-backed connections disconnected until separately authorized;
5. require explicit confirmation before replacing current state;
6. report partial failures without claiming the whole restore succeeded;
7. preserve a recovery path when the underlying subsystem supports one.

System rollback is not a substitute for user-data restore and stays under Recovery.

## Current implementation status

Implemented:

- Meo Settings owns the visible `Storage & backup` surface;
- a real local `Back up now` action writes a bounded v1 manifest;
- OmniStore application identifiers are projected into a reinstall list;
- the portable Control Center / Shell settings schema is implemented and covered by contract tests;
- ordinary manifests reject secret flags, unexpected fields, unsafe application identifiers, unsupported setting values, symlink restore inputs, and oversized files;
- local manifest parsing and restore preview validation exist;
- the previous Meo Account `Library` / backup UI and Account search destination have been retired.

Not yet implemented:

- wiring the current Control Center / Shell live snapshot into the visible `Back up now` action (the schema is ready; the current UI action still creates the application-list subset until this coordinator connection lands);
- a file-picker restore surface and detailed restore preview UI;
- an Apply Restore transaction;
- user-data payloads or encrypted user-data archives;
- scheduled backups;
- cloud backup destinations.

Existing historical server-side backup data is intentionally not deleted during the UI move. It can be migrated or retired only after Meo Settings has an explicit compatibility path.

## Next implementation gates

1. Make `BackupBackend` the coordinator that projects live Control Center and Shell state into the portable v1 settings object. QML must not duplicate the schema.
2. Expose a restore file picker and category-level preview while keeping Apply disabled.
3. Apply portable settings only through their owning backends, with explicit confirmation and per-group failure reporting.
4. Add user-selected data only after a bounded path/category contract and local encrypted archive design exist.
5. Add an optional Meo Account cloud destination only after local format and restore behavior are stable.
