# Meo System Transaction service contract

## Purpose

`org.meo.SystemTransaction1` is the narrow privileged transaction authority used by Meo Settings for system-wide configuration that cannot safely be written by the unprivileged GUI process.

The Settings application is only a client. It must never gain root authority, construct shell commands, edit PAM files, or treat a preview as an applied change.

This contract starts with **inspection only**. Apply/rollback APIs must not be enabled until the service implementation has snapshot, authorization, validation, rollback and read-back verification.

## D-Bus identity

System bus:

- service: `org.meo.SystemTransaction1`
- object path: `/org/meo/SystemTransaction1`
- interface: `org.meo.SystemTransaction1`

## Inspect

```text
Inspect(
  string kind,
  a{sv} request
) -> a{sv} plan
```

`Inspect` is non-mutating. Calling it must never trigger Polkit, change a file, restart a service, edit PAM, switch display managers, or apply configuration.

The first supported `kind` is:

```text
configuration
```

with request:

```text
{
  configurationId: string,
  operation: string,
  payload: a{sv}
}
```

Unknown kinds, configuration IDs, operations or payload fields fail closed with a D-Bus error.

A successful plan should contain at least:

```text
{
  planVersion: 1,
  requestId: string,
  kind: string,
  configurationId: string,
  operation: string,
  state: "planned",
  summary: string,
  requiresAuthorization: bool,
  reversible: bool,
  affectedResources: string[]
}
```

The plan may include additional typed presentation facts, but never a password, secret, arbitrary command, executable path supplied by the caller, raw PAM content or authorization token.

`requestId` is an opaque service-issued identifier. It is not authority by itself.

## Session-entry configuration

The first privileged target should be the Meo Login Manager presentation document.

Suggested configuration ID:

```text
session-entry.login.v1
```

Allowed initial operations:

- `replace` — validate and plan replacement of the system-managed login presentation document;
- `restore-defaults` — plan removal/replacement of only the Meo login presentation document.

The payload must conform to the Meo session-entry schema with:

```text
schemaVersion = 1
scope = "login"
```

The service must reject user-session-only sources and fields. In particular, login configuration cannot enable media, notifications, album artwork or precise user location and cannot accept arbitrary filesystem paths, URLs, QML or PAM configuration.

## Future mutation sequence

A future mutable transaction API must preserve this state sequence:

```text
Inspect
→ Validate
→ Plan
→ Preview
→ Authorize
→ Snapshot
→ Apply
→ Validate persisted state
→ Read back / Verify
→ Commit
```

Any failure after Snapshot must move through rollback before the request is considered finished.

Do not collapse inspection and mutation into one D-Bus method.

A future API may add methods such as `Authorize`, `Apply`, `GetRequest` and `Rollback`, but their exact signatures must be frozen in this document before Settings calls them.

## Authorization

Polkit belongs to the privileged service, not Meo Settings QML and not the language model/AI Router.

Authorization must bind to the exact normalized request/plan. A caller must not be able to inspect one payload and apply another under the same authorization.

## Login Manager safety boundary

For `session-entry.login.v1`, the service may write only the managed Meo Login Manager presentation document and managed presentation assets defined by the session-entry contract.

It must not directly edit:

- PAM configuration;
- passwords or hashes;
- fingerprint enrollment;
- `/etc/passwd`, `/etc/shadow` or user identity databases;
- arbitrary systemd units;
- the login manager executable;
- arbitrary QML supplied by Settings;
- unrelated SDDM/Plasma configuration.

Authentication/session-start policy remains owned by the login manager and its upstream security authority.

## Client behavior

`SystemTransactionBackend` on the Settings side:

1. checks that the service is registered;
2. calls `Inspect` asynchronously;
3. exposes `inspecting`, `planned`, `error` or `unavailable` state;
4. keeps only the returned structured plan;
5. performs no mutation while only the inspection contract exists.

An empty or malformed plan is an error rather than success.

## Current implementation status

The Meo Settings client now performs the real `Inspect` D-Bus call when the service is present.

The privileged service implementation and all mutating transaction methods remain unfinished. Until that service exists, Settings truthfully reports the transaction backend as unavailable and must keep privileged Login Manager changes read-only/preview-only.
