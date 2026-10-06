# Meo Settings agent rules

## Start here

Meo Settings owns application pages and their real Qt/KDE/Meo.System backends. Inspect `git status`, the affected page/backend, and its nearest tests/contracts before editing. Read only task-relevant docs.

## Ownership

- `qml/`: Settings and Welcome UI/routes.
- `src/`: C++ backends, models, and platform integration.
- `data/`: versioned app data/desktop metadata.
- `tests/`: C++/QML contracts.
- Shared reusable controls/tokens/motion belong in MeoUI.
- Plasma-specific implementation belongs in meo-kde.

Use stable Qt/KDE/Meo.System APIs. Do not replace NetworkManager, BlueZ, PipeWire, KScreen, PowerDevil, KWin, package management, credentials, privilege, storage, or recovery authorities with fake local state or generic shell-command toggles. Use a maintained native backend/KCM handoff when that is the real authority.

## Runtime system information

Treat system/device information as runtime state, not UI copy. If a value can vary by machine, installation, session, connected hardware, installed software, account, service, or current configuration, read it from the authoritative runtime source whenever practical instead of hard-coding it in QML/C++.

- Prefer the owning Qt/KDE/native API or a documented Meo.System contract; use stable read-only system interfaces only when no suitable native API exists.
- Do not guess or ship plausible placeholder hardware/system facts. If a value cannot be detected reliably, report it as unavailable/unknown, hide the hardware-specific row, or disable the related capability with an explanation.
- Keep mutable state reactive where practical and provide an explicit refresh path for snapshot/expensive information. After state-changing actions, re-read the owner rather than assuming the requested result.
- Test/preview fixtures may contain deterministic fake values only behind explicit test/preview paths. Production startup must never silently fall back to them.
- Static values are appropriate only for genuine product constants such as branding, UI copy, design tokens, protocol identifiers, or the compile-time version of the exact binary being run.

The detailed contract is `docs/RUNTIME_SYSTEM_INFORMATION.md`. `SystemInfoBackend` owns About/device facts; QML must consume backend values rather than inventing machine facts locally. Apply the same rule to storage, power, display, audio, network, Bluetooth, update, account, capability, and other system-backed pages.

## Validation matrix

For normal Settings changes, mirror `.github/workflows/arch-tests.yml`:

1. Build the required MeoUI and Meo.System dependencies.
2. Configure this repo with `BUILD_TESTING=ON` and the real import roots.
3. `cmake --build build --parallel 2`
4. `QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure --timeout 60`
5. `QT_QPA_PLATFORM=offscreen ./build/meo-settings --smoke`

For Welcome-only work, also run the English and zh_CN smoke paths from `.github/workflows/welcome-validation.yml`.

Do not assume dependency branches; follow the repository's current workflow/contract when a pinned dependency ref exists.

A compile/offscreen smoke does not prove a live Plasma session, hardware, service, privilege, destructive storage, or recovery path.

## Cross-repository rule

If the change should be reusable by other Meo apps, implement the primitive in MeoUI first. If it changes Plasma/KWin/Meo.System behavior, implement that integration in meo-kde and consume its public contract here.

## Files and generated output

Keep maintained contracts in `docs/`. Project records belong under `$MEO_DOCS_ROOT/Projects/meo-settings/`. Existing workflows may use an ephemeral local `build/`; retained evidence and deliverables belong under `$MEO_OUTPUT_ROOT/meo-settings/{build,install,validation,packages,tmp}/`. Do not invent machine-specific paths if those roots are unset.

Preserve unrelated dirty work and avoid destructive cleanup or unapproved live-system changes.
