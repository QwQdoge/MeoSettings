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
- system snapshots, fallback kernels, repair, and OS rollback stay in Recovery;
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

Meo Settings already exposes the `Storage & backup` category and a `Backup & recovery` status section. Until a verified backup backend is connected, controls must remain read-only/disabled and say that backup is not configured. The UI must not fabricate a last-backup time, cloud-sync state, backed-up byte count, or restore point.

The previous Meo Account `Library` / OmniStore backup surface is retired from Account UI. Existing server data is intentionally not deleted during the UI move; it can be migrated or retired only after the Meo Settings backend has an explicit compatibility plan.

## Backend direction

A future `BackupBackend` in Meo Settings should own local backup manifests and coordinate authoritative subsystem adapters. It should consume existing Account and OmniStore contracts rather than directly duplicating their credential or application databases.

Recommended first useful scope:

- Meo Settings configuration that is explicitly safe to export;
- OmniStore application identifiers / reinstall list;
- a user-selected set of supported data categories;
- local encrypted archive destination first;
- optional Meo Account cloud destination after the local manifest/restore format is stable.

Do not add an enabled "Back up now" or "Restore" button before those actions have a tested backend.