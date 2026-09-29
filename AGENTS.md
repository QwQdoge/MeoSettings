# Meo Settings Agent Rules

## Ownership

Meo Settings owns its pages, C++ backends, Meo-owned settings flows, and explicit integrations. Reuse shared controls/tokens from MeoUI via `$MEO_UI_ROOT`; Plasma-specific runtime integration belongs in `meo-kde`.

Use stable Qt/KDE/Meo.System APIs. NetworkManager, BlueZ, PipeWire, KScreen, PowerDevil, KWin, package management, credentials, privileged/destructive storage work, and recovery remain with their authoritative service or maintained KCM unless this repository has a verified native contract. Do not fake local state.

## Validation

Inspect the relevant source/contract and `git status`, then run the smallest applicable checks.

- Normal Settings C++/QML change: mirror `.github/workflows/arch-tests.yml`—configure, build, `ctest --test-dir build --output-on-failure --timeout 60`, then `./build/meo-settings --smoke`.
- Welcome change: also mirror `.github/workflows/welcome-validation.yml` and smoke both English and Chinese routes.
- Shared UI change: implement it in MeoUI; follow MeoUI's QML coverage plus Showcase/checklist evidence rules instead of duplicating the component here.

Compile/static/offscreen tests do not prove real Plasma-session, hardware, service, privilege, or recovery behavior.

Use `$MEO_DOCS_ROOT/Projects/meo-settings/` for plans/audits/decisions and `$MEO_OUTPUT_ROOT/meo-settings/{build,install,validation,packages,tmp}/` for new output. Do not add new results to legacy `out/`.

Preserve unrelated dirty work. Do not mutate live system settings or privileged state merely to validate a change unless explicitly authorized.
