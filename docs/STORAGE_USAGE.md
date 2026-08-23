# Storage usage contract

Meo Settings presents two deliberately separate kinds of storage information.
They must not be added together or presented as a single "used by category"
total: doing that would double-count files which already live on a mounted
volume.

## Mounted and system storage

`StorageBackend` reads mounted filesystem capacity through Qt and Solid. The
system-volume row is the exact used capacity of the mounted `/` filesystem.
It includes system files, packages, settings, logs, caches, and every other
file on that filesystem. It is not a fabricated estimate of the OS alone, and
it may not include a separately mounted home filesystem.

The page does not mount, unmount, format, partition, repair, encrypt, clean,
or otherwise change a disk. Those recovery-sensitive operations remain in the
maintained KDE tools shown as explicit handoffs.

## Explicit personal-category scan

The **Scan selected folders** action is the only way to start category
collection. It is not run on launch, refresh, or in the background. The scan
has these fixed boundaries:

- It never scans all of Home, a mounted volume, package databases, or another
  user's files.
- It reads only the configured Pictures, Videos, Documents, and Music folders,
  plus these known local-AI roots when they exist:
  `~/.ollama/models`, `~/.cache/huggingface/hub`,
  `~/.cache/lm-studio/models`, `~/.local/share/LM Studio/models`, and
  `~/.local/share/ollama/models`.
- Image, video, document, and audio categories include only recognised file
  suffixes in their respective selected folders. The AI category includes
  files only inside the named model/cache roots; a large arbitrary file is not
  inferred to be an AI model.
- Roots outside Home, Home itself, symlinked roots, and symlinked entries are
  skipped. Directory traversal never follows symlinks.
- The worker stops after 200,000 directory entries, can be canceled, and marks
  all partial or unvisited values accordingly. An unreadable selected root is
  shown as unknown rather than zero.

The result is therefore a bounded view of selected personal folders, not a
whole-device storage analyser. `Checked`, `Partial`, `Canceled`, `Not found`,
`No access`, and `Skipped safely` are meaningful states; only a checked value
can be read as a complete value for that category's selected scope.

## Software: Pacman, foreign/AUR, and OmniStore

The **Inspect installed packages** action is a distinct, on-demand local
Pacman inventory. It runs only fixed read-only commands with `LC_ALL=C`:
`pacman -Qi` for installed package file sizes, then `pacman -Qqm` for foreign
package names. It never runs on page load, refreshes no repository database,
uses no privilege, and performs no package action or network request.

It presents two non-overlapping local-package buckets:

- **Pacman repository packages** are installed packages not marked foreign by
  Pacman, with their reported installed package-file size.
- **Foreign / AUR candidates** are Pacman foreign packages. AUR packages are
  usually here, but manually built/local packages can be foreign too, so Meo
  deliberately does not claim every foreign package came from AUR.

The reported installed size is neither a whole-volume total nor an application
share: it excludes user data and can differ from allocated disk blocks,
deduplicated files, caches, or an application's actual footprint. It is not
added to the system-volume capacity figure.

Separately, the optional versioned `omnistore-apps-export` contract can show
source-tagged managed-application records and known exact/reported application
sizes. A missing OmniStore source row is **not reported**, not a zero-package
claim. The OmniStore exporter is read-only: it does not open OmniStore's UI,
connect to its private daemon protocol, install/remove/update packages, or
perform cleanup. Package management and system updates have their own explicit
owners.
