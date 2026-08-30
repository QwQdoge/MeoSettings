# Application Icon Studio contract

Meo Settings owns the user flow; MeoKDE owns rendering and Dock integration;
Meo Account owns cloud credentials and every provider call.

Deterministic application styles (`monet`, `original`, and `mono`) are applied
by `meo-app-icon-studio`. The tool creates unique user-local hicolor names and
desktop-entry overrides for applications only. It does not change KDE's global
icon theme, so status, device, action, Wi-Fi, microphone, and volume icon names
remain untouched. Applying all applications updates the Dock default mode;
app-specific changes write a per-desktop-ID override to `meodockrc`. New
profiles default to `monet` with a circular container. Monet stores a
palette-independent, three-level symbol mask so wallpaper light/dark palette
changes can recolor the mark without flattening an opaque source icon into a
solid disk. The former `pure` value migrates to `monet`.

AI generation runs as a sequential, preview-first batch for 1 to 128 selected
applications. Settings lists metadata for the user's enabled Account AI
connections, builds the locked Easel/Monet prompt from each application
identity plus the editable user requirement, and asks the desktop Account
broker to prepare one payload-bound image consent per app. It then aggregates
only their display metadata into one confirmation containing provider, model,
destination, purpose, application count, data categories, and total prompt
length. Accepting that sheet spends each prepared consent sequentially;
dismissal denies them sequentially. One acceptance never changes an icon.

The Account broker retains its KWallet session and provider credentials. It
returns only a validated PNG data source. Settings writes each source into a
private auto-removed staging directory and exposes the complete pack for local
preview. `meo-app-icon-studio --ai-pack` snapshots all affected user-local
desktop entries, generated hicolor files, the Dock override file, and its own
manifest before committing the pack. A failure restores that snapshot; a
successful commit removes the staging directory. No provider key or Account
access token enters Settings or the renderer. The renderer keeps only a
palette-independent continuous-luminance texture asset for each accepted AI
icon, so Meo dynamic-color refreshes recolor that same pack locally without a
new provider request and without losing the per-app `ai` mode.

Compilation, offscreen QML loading, deterministic renderer tests, and Account
contract tests do not prove that a production Account deployment has the Edge
Function, encryption key, allowlisted provider host, usable quota, or a signed-
in user connection. Those remain runtime acceptance gates.
