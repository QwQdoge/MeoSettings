# Meo Settings Agent Rules

## Ownership and product boundary

- Meo Settings owns its application pages, C++ backends, Meo-owned settings flows, and explicit integrations. Reuse shared controls/tokens from `/home/shekong/Projects/meo-ui`; keep Plasma-specific integration in `meo-kde`.
- Use stable Qt/KDE/Meo.System APIs. Do not implement fake local state or use generic command-line control tools as a substitute for a supported backend.
- Keep NetworkManager, BlueZ, PipeWire, KScreen, PowerDevil, KWin, package management, credentials, privileges, destructive storage work, and recovery paths with their authoritative service or maintained KCM unless this repository has a verified native contract.
- Inspect relevant source, public contract, Git status, and runtime dependency boundary before editing. Preserve unrelated dirty work.

## Repository hygiene

- New root content is limited to README/AGENTS, source directories, and required build/release configuration. Never add loose plans, architecture drafts, audits, journals, screenshots, or generated logs.
- `docs/` holds only maintained public contracts directly tied to code. Store plans, audits, decisions, agent journals, and historical reports in `/home/shekong/Documents/Obsidian Vault/MeoArch/Projects/meo-settings/`, using the numbered `00-inbox/`, `01-overview/`, `02-decisions/`, `03-work/`, `04-validation/`, and `99-archive/` folders described by that project's reader-facing `README.md`.
- Store generated material only in `/home/shekong/Projects/outputs/meo-settings/{build,install,validation,packages,tmp}/`: compiler results, staged installs, evidence, releasable packages, and disposable work respectively. Each validation run is `validation/<UTC-run-id>/` and includes a `README.md` and evidence. Do not create new results in repository `out/`; leave pre-existing files untouched unless an approved migration explicitly covers them.

## Validation and claims

- Validate the smallest affected backend/UI layer, then the necessary integration layer. Record commands and results only after they actually run.
- Clearly distinguish compile/static/offscreen results from real Plasma-session, hardware, service, privilege, and recovery acceptance. Do not claim any unperformed or unsupported command as verified.
- Changes to shared MeoUI deliverables must be made in MeoUI and meet its Showcase gate: refresh public tokens, QML items, module/runtime APIs, and visible behavior to 100% coverage; build and run the Showcase, and retain evidence in `/home/shekong/Projects/outputs/meo-ui/validation/<UTC-run-id>/`.
