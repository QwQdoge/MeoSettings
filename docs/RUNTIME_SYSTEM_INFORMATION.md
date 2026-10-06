# Runtime system information policy

Meo Settings and other Meo system surfaces must treat system/device information as **runtime state**, not presentation copy.

## Core rule

If a value describes the machine, active session, connected hardware, installed software, current configuration, service state, account state, capacity, capability, or version that can vary between installations, it must be read from an authoritative runtime source whenever practical.

Do not hard-code production values merely to make a page look complete.

Examples that must normally be detected include:

- device/host name;
- hardware model and vendor;
- CPU architecture and processor information;
- memory capacity;
- GPU/display inventory;
- storage devices, mount points, capacity, and removable state;
- battery presence, percentage, charge state, and estimates;
- operating-system name/version/build;
- kernel and runtime versions;
- installed package/application versions;
- active desktop/session/platform information;
- Wi-Fi, Bluetooth, audio, display, fingerprint, camera, sensor, and other hardware availability;
- network/service/update/account state;
- configured theme, wallpaper, locale, power profile, and other current settings.

## Source priority

Prefer the real owner of the state, in roughly this order:

1. Stable Qt/KDE/native library API owned by the subsystem.
2. A documented Meo.System or Meo application contract when Meo owns the integration boundary.
3. Stable kernel/system interfaces such as `/sys`, `/proc`, `os-release`, DMI/sysfs, UPower, udev, systemd, or other appropriate read-only platform interfaces.
4. A narrow, schema-validated helper only when no suitable native API exists.

Do not parse generic command output when a maintained native API exists. Do not duplicate subsystem state into a second local configuration store just for display.

## UI behavior when information is unavailable

Unknown information must remain unknown.

- Show `Unavailable`, `Unknown`, or an equivalent explicit state when the fact is useful even without a value.
- Hide a hardware-specific row/control when the hardware or authoritative backend is absent and the row has no useful unavailable state.
- Disable an action with a real explanation when the capability is unavailable.
- Never substitute a guessed vendor, model, amount of RAM, GPU, battery, OS version, account, network, device, or service state.
- A cached value may be shown only when the UI clearly treats it as cached/stale and the owning contract permits caching.

## Refresh and reactivity

Runtime information should stay current for the lifetime of the application.

- Prefer backend signals/watchers for state that can change while the app is open.
- Provide an explicit refresh path for information that is expensive or naturally snapshot-based.
- Re-read the authoritative source after state-changing actions instead of assuming the requested state was applied.
- Avoid one-time QML literals for facts that belong to a backend.

## What may remain static

Static values are appropriate when they are genuinely product constants rather than detected machine state, for example:

- product/application names and branding;
- translation keys and descriptive UI copy;
- design tokens, spacing, shapes, and default layout policy;
- capability identifiers and stable protocol/schema names;
- compile-time application version constants that describe the exact binary being run;
- safe fallback labels such as `This device`, provided they do not pretend to be detected facts.

A fallback must never silently turn into fake system information.

## Settings-specific contract

`SystemInfoBackend` is the owner of About/device facts. QML consumes backend-provided values and does not invent device facts locally. The current implementation already reads OS information through `KOSRelease`, memory through `KMemoryInfo`, user information through `KUser`, and kernel/architecture/host information through `QSysInfo`.

Other Settings pages follow the same rule through their subsystem backends: NetworkManagerQt, BluezQt, PulseAudioQt/PipeWire compatibility, KScreen, Solid, PowerDevil/UPower, Plasma notification APIs, package metadata, and other maintained owners remain authoritative.

When adding a new system-information field, document its authoritative source and unavailable behavior in the owning backend or nearest contract. If the field cannot yet be detected reliably, leave it unavailable rather than shipping a hard-coded approximation.

## Tests and previews

Mocks, fixtures, preview values, and deterministic test data are allowed only in explicit test/preview paths. Production startup must not silently fall back to those values.

Tests should verify that:

- runtime values come from the intended backend/source;
- unavailable sources do not produce plausible fake facts;
- refresh/reactive paths update displayed state;
- production and preview/test code paths cannot be confused accidentally.
