# Privacy and application permissions

Meo Settings has one verified per-application permission inventory: the local
effective Flatpak sandbox metadata reported by `flatpak info
--show-permissions`. It groups the values Flatpak actually reports for shared
services, display/audio/IPC sockets, devices, filesystem access, persistent
folders, sandbox features, and D-Bus policy. The page only runs local read
commands after the user opens Privacy or refreshes it; it never requests a
portal grant, launches an app, edits a sandbox, or sends data to a service.

This is intentionally labelled **effective sandbox access**, not historical
usage. Plasma and desktop Linux have no reliable universal API that proves
which arbitrary native application has used a camera, microphone, file, or
screen-capture permission. Meo therefore does not fabricate an app-usage log.
Native security controls such as screen lock, firewall, and firmware security
remain explicit maintained KDE handoffs until they have a complete privilege,
recovery, and audit contract.
