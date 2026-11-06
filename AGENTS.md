> 📚 [MeoArch 主文档 / Master Doc](https://github.com/QwQdoge/MeoArch-os-workspace/blob/main/docs/MEOARCH_MASTER.md)：全部仓库地图、操作手册、Cloudflare 清单、全部文档链接。<!-- meoarch-master -->

## Workflow rules from the owner (override anything below)

- Commit directly on the repository's current/default branch (`main` unless the repo is deliberately on another working branch). Do not create feature branches or PR branches unless asked.
- After committing, push immediately. Do not ask for permission to commit or push, and do not leave finished work uncommitted or unpushed.
- Keep verification minimal: run only the quick check for the touched code (typecheck or the one relevant test). Do not run full suites or write extra tests unless asked.
- Do the whole task in one pass; do not stop to ask routine questions that have an obvious default.
- Never put secrets, tokens or credentials in code, logs or commits.

## MeoUI specification (read before UI work)

- People: https://meoarch.org/docs/ . Agents: https://meoarch.org/llms.txt , then the Markdown pages it lists; start with https://meoarch.org/docs/meoui/for-agents.md (decision tables and checklists).
- Window chrome and toolbar, where app settings live and their categories, About, and open-source credits: https://meoarch.org/docs/meoui/app-shell.md .
- The canonical source is MeoUI `docs/design/`; the site is generated from it. If they disagree, MeoUI wins. Propose rule changes there, not in this repository.

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

## MeoArch documentation map

- Docs site: https://meoarch.org/docs/ (MeoUI specification: app shell, settings pattern, page types, dynamic color, API lifecycle, localization). Agent entry points: https://meoarch.org/llms.txt and https://meoarch.org/docs/meoui/for-agents.md. The canonical source is the MeoUI repository `docs/`; if they disagree, the repository wins.
- Packaging and releases: https://github.com/QwQdoge/meo-repo - read its `AGENTS.md`, `docs/MEOARCH_IMPLEMENTATION_RUNBOOK.md`, `manifests/{beta,stable}/` and the "MeoArch repository release" workflow (https://github.com/QwQdoge/meo-repo/actions/workflows/release.yml).
- Release approval: the `sign-and-publish` and AUR jobs wait on the protected `release` Environment. After the owner confirms in chat, agents approve with `gh api -X POST repos/QwQdoge/meo-repo/actions/runs/<run>/pending_deployments -F environment_ids[]=<id> -f state=approved` (details in meo-repo `AGENTS.md`); the owner can also use Actions -> "Review deployments" -> `release`.
- Before tagging a component for release: its source archive must extract cleanly (no tracked build output such as `target/`, no absolute symlinks) and its CI must be green; tag the newest green commit.
- Other repositories: MeoUI (design system), meo-kde (Plasma shell), meo-ai (Meo AI), meo-api (cloud API), MeoArch-account (account app), MeoSettings, MeoArch-os-workspace (ISO/installer/repair), OmniStore, meo-login-manager, meo-repo (packages), meoarch-site (meoarch.org).

## Shared workspace rules (owner)

Source: the owner's local `projects/agent.md`, repeated here on purpose. These rules apply to every repository and win over generic caution in the rest of this file.

- "All repositories" means: the `projects` folder inside WSL Arch (`~/projects`), the Windows `projects` folder, and whatever `gh repo list QwQdoge` shows; confirm with `gh`.
- Reading a repository: `git pull` and `git push` first so it matches the cloud. If there are open PRs/branches, merge all of them. If a branch is large or needs a decision, ask once, then merge everything without asking again before each merge; after merging, ask whether to run tests. Exceptions: old dependency-bump branches and branches that are clearly unneeded.
- Work: update dependencies to the newest versions dynamically; only if a bug survives a real fix attempt, roll back and tell the user. Never create working branches: one shared workspace on the default branch, kept tidy. Push every piece of work to the cloud right away; always `git pull` then `git push`, no delay.
- Ask the user as little as possible: anything solvable by a simple attempt, or that matches the user's stated wishes, is done without asking. Ask only after repeated real attempts show the goal cannot be reached.
- Read every relevant document before working: local ones and https://meoarch.org/docs/ (agent entry: https://meoarch.org/llms.txt).
- Releases the owner asks for are authorized end to end (tag, manifest, dispatch, approve via `gh api .../pending_deployments`); do not stop for extra approvals.
- Never write personal or machine-specific details (usernames, absolute local paths, emails, VM or machine layout, secrets, resource ids) into repositories or published docs; keep them in local-only files (`~/.claude/CLAUDE.md`, the owner's local `projects/agent.md`).
