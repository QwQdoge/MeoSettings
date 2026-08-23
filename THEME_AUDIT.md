# MeoKDE theme audit — 2026-08-22

This is a source and offscreen-validation audit of the sibling `meo-kde` and
`meo-ui` projects. No source changes were made in either project as part of
this audit. A passing static check is not proof of live Plasma/KWin acceptance.

## Verified baseline

`meo-kde/scripts/validate.sh` passed with the separately built MeoUI import.
That covered shell syntax, icon/theme tests, native smoke binaries, style
tests, offscreen QML instantiation, QML linting, and SVG parsing. It did not
exercise a real user-facing theme switch, wallpaper change, KWin reload, or
Plasma session.

## Findings that need a deliberate repair plan

1. **MPRIS application icons use the wrong renderer.**
   `native/system/mediacontroller.cpp` preserves MPRIS `DesktopEntry` values,
   including real KDE application-icon names. The top-bar media card renders
   that value with Material-glyph-only `MeoIcon` in
   `plasmoids/org.meo.topbar/contents/ui/QuickSettingsHome.qml`. It should use
   `Kirigami.Icon`, as the notification surface already does, with a Material
   fallback only when no desktop icon is available.

2. **Dynamic-color contrast has no end-to-end owner.**
   The native generator accepts `--contrast`, but `MeoShellTheme` calls
   `MaterialColors.schemeFor(accentColor, darkMode)` without a contrast level;
   the generated KDE scheme also does not persist that Material contrast
   choice. Exposing an accessibility-contrast setting before deciding how it
   is stored, read, synchronized, and tested would create divergent palettes.

3. **Dynamic-color watcher policy contradicts its documentation.**
   Documentation describes the watcher as opt-in, while the Arch package adds
   it to `default.target.wants`. The path unit watches only the wallpaper
   applet config, despite the generator also deriving its seed from KDE accent
   configuration. The intended policy must be chosen: global versus opt-in,
   and wallpaper-only versus wallpaper plus KDE accent changes.

4. **Static palette sources have drifted.**
   `MeoLight.colors` and the Fcitx light theme retain the older purple
   `#6750a4`, while the vendored Material `SchemeTonalSpot` smoke reference
   produces `#65558f` for the canonical seed. Choose the canonical source
   before regenerating static KDE/Fcitx assets or changing the generator.

5. **MeoUI Snackbar bypasses semantic inverse tokens.**
   `MeoTheme` provides dynamic `inverseSurface`, `contentOnInverseSurface`, and
   `inversePrimary`, but `components/MeoSnackbar.qml` hard-codes fallback
   colors. This repair belongs in `meo-ui`, with a dynamic-color regression
   test, not in `meo-kde` or this settings app.

6. **Shelf migration is incomplete as a product decision.**
   The active layout and package build use the bottom KDE task manager, while
   source, metadata, older UX documents, and validation still retain the
   legacy `org.meo.shelf` / `org.meo.toptasks` implementations. Do not delete
   them until the project decides whether they are retired/archived or remain
   supported experimental artifacts.

## Recommended sequence after confirmation

1. Repair the media icon renderer; it is a localized MeoKDE correctness fix.
2. Confirm the dynamic-color policy (contrast storage, watcher scope/default,
   and canonical palette source) before changing generated or shared tokens.
3. Repair the shared Snackbar in MeoUI and test it against dynamic roles.
4. Decide the Shelf product status, then make source, packaging, setup, and
   documentation agree in one migration.

The Meo Settings appearance page intentionally remains a KCM fallback until
those decisions produce a verified, reversible theme-control contract.
