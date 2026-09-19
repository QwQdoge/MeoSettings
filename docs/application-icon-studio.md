# Application Icon Studio contract

Meo Settings owns the user flow; MeoKDE owns the renderer, icon-theme overlay
and Plasma integration; Meo Account owns cloud credentials and every provider
call. Reusable visual controls remain owned by MeoUI.

## Tracks and resolution

Application identity, KDE system semantics, and live status are separate
tracks. The Studio may change an application identity only. Wi-Fi, volume,
battery, authentication, devices, places and actions continue to resolve via
`MeoSymbols`/KDE and are never made part of an AI pack.

The default renderer is a user-level FreeDesktop icon-theme overlay:

```text
MeoUser     -> MeoSymbols     -> breeze -> hicolor
MeoUserDark -> MeoSymbolsDark -> breeze-dark -> breeze -> hicolor
```

An application uses `overlay-name` only when its actual `Icon=` name is stable,
unique among visible launchers, not an absolute path, and not already owned by
a Meo system-semantic icon. The Studio records that audited resolution in its
manifest. Shared names, absolute paths and private/runtime loaders use the
minimal managed desktop-entry fallback only when necessary. A fallback must
not freeze a complete upstream desktop entry or become the normal path.

**Original** removes an application's active overlay or fallback and returns to
normal current FreeDesktop/KDE lookup. It is not a recolored or containerized
copy of an old PNG; `Shape` is therefore `Follow original` and disabled.
`Meo Color` and `Monochrome` use the selected Meo mask. The former `pure`
spelling remains a migration alias for `monet`.

## AI pack flow

AI generation is staged for 1–128 selected applications. Studio first returns
local canonical identity descriptors; Settings sends Account only the approved
style, shape, desktop IDs and descriptor hashes. The broker keeps its KWallet
session and provider credentials, obtains explicit payload-bound consent, and
returns only an Account-private attested manifest path plus its SHA-256. No
provider PNG, key, prompt, or Account access token enters Settings or the
renderer.

After Account marks the job ready, Settings asks Studio to render exactly one
preview for that job/manifest hash. Studio validates the manifest against the
original descriptors, writes its own private preview directory, and exposes
only the complete preview list to QML. `--ai-pack` runs once after Preview is
ready; confirmed commit releases the Account staging and then discards the
Studio preview state. A failure is recovered by discarding the staged job and
creating a new pack, never by reusing an unverified material.

The commit snapshots both overlay themes, the active KDE icon-theme pointer,
legacy fallback files when applicable, the Studio manifest, and generated
assets. Each successful pack also writes a non-secret `GenerationManifest`
with contract/style/provider/model metadata, source/output hashes and recipe
version. It deliberately does not persist the full per-app prompt. Wallpaper
and dynamic-color updates recolor accepted local texture assets without a new
provider request.

## Acceptance boundary

Compilation, offscreen QML loading, deterministic renderer tests and Account
contract tests do not prove an authenticated provider or a real Plasma
launcher. Release acceptance additionally requires a package-owned Studio,
fresh install/upgrade, a real Plasma task manager session, dark/light overlay
switching, AI interruption rollback and Original after an upstream app update.
