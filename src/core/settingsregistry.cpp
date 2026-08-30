#include "settingsregistry.h"

#include <algorithm>
#include <QRegularExpression>

namespace
{
QVariantMap setting(const QString &id,
                    const QString &title,
                    const QString &description,
                    const QString &icon,
                    const QString &route,
                    const QString &categoryId,
                    const QString &category,
                    const QStringList &keywords,
                    const bool direct = false,
                    const QString &pageKind = {},
                    const QString &risk = {},
                    const QString &iconTone = {},
                    const QString &capability = {})
{
    const QString resolvedPageKind = pageKind.isEmpty()
        ? (direct ? QStringLiteral("detail") : QStringLiteral("handoff"))
        : pageKind;
    const QString resolvedRisk = risk.isEmpty()
        ? (direct ? QStringLiteral("reversible") : QStringLiteral("system"))
        : risk;
    const QString resolvedTone = iconTone.isEmpty() ? QStringLiteral("primary") : iconTone;

    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("title"), title},
        {QStringLiteral("label"), title},
        {QStringLiteral("description"), description},
        {QStringLiteral("subtitle"), description},
        {QStringLiteral("icon"), icon},
        {QStringLiteral("iconTone"), resolvedTone},
        {QStringLiteral("tone"), resolvedTone},
        {QStringLiteral("route"), route},
        {QStringLiteral("categoryId"), categoryId},
        {QStringLiteral("category"), category},
        {QStringLiteral("keywords"), keywords},
        {QStringLiteral("direct"), direct},
        // Settings pages are semantically typed so UI code cannot quietly
        // turn a system handoff or a recovery-sensitive task into a generic
        // third-level navigation page.
        {QStringLiteral("pageKind"), resolvedPageKind},
        {QStringLiteral("depth"), 2},
        {QStringLiteral("risk"), resolvedRisk},
        {QStringLiteral("presentation"), direct ? QStringLiteral("page") : QStringLiteral("external")},
        {QStringLiteral("authority"), direct ? QStringLiteral("meo") : QStringLiteral("kde")},
        // A capability is intentionally a UI visibility condition, never a
        // claim that a generic KCM can operate a particular device. It keeps
        // hardware-specific native pages out of normal navigation when their
        // real backend is absent.
        {QStringLiteral("capability"), capability},
    };
}

QVariantMap categoryDefinition(const QString &id,
                               const QString &title,
                               const QString &description,
                               const QString &icon,
                               const QString &iconTone)
{
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("title"), title},
        {QStringLiteral("label"), title},
        {QStringLiteral("description"), description},
        {QStringLiteral("subtitle"), description},
        {QStringLiteral("icon"), icon},
        {QStringLiteral("iconTone"), iconTone},
        {QStringLiteral("tone"), iconTone},
        {QStringLiteral("route"), QStringLiteral("category:") + id},
        {QStringLiteral("pageKind"), QStringLiteral("settings-category")},
        {QStringLiteral("depth"), 1},
        {QStringLiteral("risk"), QStringLiteral("read-only")},
        {QStringLiteral("presentation"), QStringLiteral("page")},
        {QStringLiteral("authority"), QStringLiteral("meo")},
    };
}

QVariantMap sidebarHeader(const QString &label)
{
    return {
        {QStringLiteral("type"), QStringLiteral("header")},
        {QStringLiteral("label"), label},
    };
}
}

SettingsRegistry::SettingsRegistry(QObject *parent)
    : QObject(parent)
{
    // The registry is the authority for Settings information architecture.
    // Level 1 is a stable category. Each direct route is a complete level-2
    // page; one-shot detail/edit flows are transient sheets owned by that page
    // and must never be registered as another route.
    m_categories = {
        categoryDefinition(QStringLiteral("network"), tr("Network & Internet"),
                           tr("Wi-Fi, wired connections, VPN, proxy, and hotspot"),
                           QStringLiteral("wifi"), QStringLiteral("primary")),
        categoryDefinition(QStringLiteral("devices"), tr("Connected devices"),
                           tr("Bluetooth, keyboards, pointing devices, and peripherals"),
                           QStringLiteral("devices"), QStringLiteral("secondary")),
        categoryDefinition(QStringLiteral("display-sound"), tr("Display & sound"),
                           tr("Displays, audio devices, brightness, and night light"),
                           QStringLiteral("monitor"), QStringLiteral("secondary")),
        categoryDefinition(QStringLiteral("personalization"), tr("Wallpaper & style"),
                           tr("Appearance, dynamic colors, wallpaper, icons, and fonts"),
                           QStringLiteral("palette"), QStringLiteral("tertiary")),
        categoryDefinition(QStringLiteral("apps"), tr("Apps & notifications"),
                           tr("Notifications, defaults, permissions, and startup behavior"),
                           QStringLiteral("apps"), QStringLiteral("primary")),
        categoryDefinition(QStringLiteral("accounts"), tr("Accounts & sync"),
                           tr("Meo Account, local device users, online providers, and the KDE wallet"),
                           QStringLiteral("account_circle"), QStringLiteral("secondary")),
        categoryDefinition(QStringLiteral("storage"), tr("Storage & backup"),
                           tr("Mounted volumes, removable media, disk tools, and recovery boundaries"),
                           QStringLiteral("storage"), QStringLiteral("tertiary")),
        categoryDefinition(QStringLiteral("system"), tr("System"),
                           tr("Power, time, language, search, workspace, and sessions"),
                           QStringLiteral("settings"), QStringLiteral("neutral")),
        categoryDefinition(QStringLiteral("privacy"), tr("Privacy & security"),
                           tr("Permissions, screen lock, firewall, and device-security information"),
                           QStringLiteral("security"), QStringLiteral("neutral")),
        categoryDefinition(QStringLiteral("accessibility"), tr("Accessibility"),
                           tr("Assistive technology and input accessibility"),
                           QStringLiteral("accessibility"), QStringLiteral("primary")),
        categoryDefinition(QStringLiteral("updates"), tr("Updates & support"),
                           tr("System updates and supported diagnostics"),
                           QStringLiteral("system_update"), QStringLiteral("primary")),
    };

    m_entries = {
        setting(QStringLiteral("wifi"), tr("Wi-Fi"), tr("Connect to wireless networks"),
                QStringLiteral("wifi"), QStringLiteral("wifi"), QStringLiteral("network"), tr("Network & Internet"),
                {QStringLiteral("wifi"), QStringLiteral("wireless"), QStringLiteral("wlan"), QStringLiteral("internet"), QStringLiteral("ssid")},
                true, QStringLiteral("control"), QStringLiteral("reversible"), QStringLiteral("primary"), QStringLiteral("wifi")),
        setting(QStringLiteral("network-advanced"), tr("Wired, VPN & saved networks"), tr("Manage wired, VPN, proxy, and saved connections"),
                QStringLiteral("lan"), QStringLiteral("kcm:kcm_networkmanagement"), QStringLiteral("network"), tr("Network & Internet"),
                {QStringLiteral("ethernet"), QStringLiteral("vpn"), QStringLiteral("proxy"), QStringLiteral("ip"), QStringLiteral("dns")},
                false, QStringLiteral("handoff"), QStringLiteral("system"), QStringLiteral("primary")),
        setting(QStringLiteral("proxy"), tr("Proxy"), tr("Configure proxy settings for the desktop"),
                QStringLiteral("vpn_lock"), QStringLiteral("kcm:kcm_proxy"), QStringLiteral("network"), tr("Network & Internet"),
                {QStringLiteral("proxy"), QStringLiteral("http"), QStringLiteral("socks")},
                false, QStringLiteral("handoff"), QStringLiteral("system"), QStringLiteral("primary")),
        setting(QStringLiteral("mobile-hotspot"), tr("Mobile hotspot"), tr("Share a connection through a Wi-Fi hotspot"),
                QStringLiteral("portable_wifi_off"), QStringLiteral("kcm:kcm_mobile_hotspot"), QStringLiteral("network"), tr("Network & Internet"),
                {QStringLiteral("hotspot"), QStringLiteral("tethering"), QStringLiteral("share")},
                false, QStringLiteral("handoff"), QStringLiteral("system"), QStringLiteral("primary")),

        setting(QStringLiteral("bluetooth"), tr("Bluetooth"), tr("Manage adapters and nearby devices"),
                QStringLiteral("bluetooth"), QStringLiteral("bluetooth"), QStringLiteral("devices"), tr("Connected devices"),
                {QStringLiteral("bluetooth"), QStringLiteral("pair"), QStringLiteral("headphones")},
                true, QStringLiteral("control"), QStringLiteral("reversible"), QStringLiteral("secondary"), QStringLiteral("bluetooth")),
        setting(QStringLiteral("sound"), tr("Sound"), tr("Choose output and input devices"),
                QStringLiteral("volume_up"), QStringLiteral("sound"), QStringLiteral("display-sound"), tr("Display & sound"),
                {QStringLiteral("speaker"), QStringLiteral("audio"), QStringLiteral("volume"), QStringLiteral("microphone"), QStringLiteral("mic")},
                true, QStringLiteral("control"), QStringLiteral("reversible"), QStringLiteral("secondary"), QStringLiteral("audio")),
        setting(QStringLiteral("display"), tr("Displays"), tr("Adjust brightness and Night Light, then open advanced display configuration"),
                QStringLiteral("monitor"), QStringLiteral("display"), QStringLiteral("display-sound"), tr("Display & sound"),
                {QStringLiteral("screen"), QStringLiteral("monitor"), QStringLiteral("resolution"), QStringLiteral("display"),
                 QStringLiteral("brightness"), QStringLiteral("night light")},
                true, QStringLiteral("control"), QStringLiteral("reversible"), QStringLiteral("secondary")),
        setting(QStringLiteral("night-light"), tr("Night light"), tr("Turn Night Light on or off and open its advanced schedule"),
                QStringLiteral("dark_mode"), QStringLiteral("display"), QStringLiteral("display-sound"), tr("Display & sound"),
                {QStringLiteral("night light"), QStringLiteral("blue light"), QStringLiteral("night color"), QStringLiteral("warm screen")},
                true, QStringLiteral("control"), QStringLiteral("reversible"), QStringLiteral("secondary")),
        setting(QStringLiteral("keyboard"), tr("Keyboard"), tr("Keyboard hardware, layout, and shortcuts"),
                QStringLiteral("keyboard"), QStringLiteral("kcm:kcm_keyboard"), QStringLiteral("devices"), tr("Connected devices"),
                {QStringLiteral("keyboard"), QStringLiteral("layout"), QStringLiteral("shortcut")},
                false, QStringLiteral("handoff"), QStringLiteral("system"), QStringLiteral("secondary")),
        setting(QStringLiteral("mouse"), tr("Mouse & touchpad"), tr("Pointing-device configuration"),
                QStringLiteral("mouse"), QStringLiteral("kcm:kcm_mouse"), QStringLiteral("devices"), tr("Connected devices"),
                {QStringLiteral("mouse"), QStringLiteral("touchpad"), QStringLiteral("pointer")},
                false, QStringLiteral("handoff"), QStringLiteral("system"), QStringLiteral("secondary")),
        setting(QStringLiteral("touchpad"), tr("Touchpad"), tr("Gestures, scrolling, tapping, and pointer acceleration"),
                QStringLiteral("touch_app"), QStringLiteral("kcm:kcm_touchpad"), QStringLiteral("devices"), tr("Connected devices"),
                {QStringLiteral("touchpad"), QStringLiteral("gesture"), QStringLiteral("scroll"), QStringLiteral("tap")},
                false, QStringLiteral("handoff"), QStringLiteral("system"), QStringLiteral("secondary")),
        setting(QStringLiteral("shortcuts"), tr("Keyboard shortcuts"), tr("Global shortcuts for KDE, applications, and custom actions"),
                QStringLiteral("keyboard_command_key"), QStringLiteral("kcm:kcm_keys"), QStringLiteral("devices"), tr("Connected devices"),
                {QStringLiteral("shortcut"), QStringLiteral("hotkey"), QStringLiteral("key binding")},
                false, QStringLiteral("handoff"), QStringLiteral("system"), QStringLiteral("secondary")),
        setting(QStringLiteral("input-method"), tr("Input method"), tr("Configure Fcitx 5 languages, layouts, and input engines"),
                QStringLiteral("language"), QStringLiteral("kcm:kcm_fcitx5"), QStringLiteral("devices"), tr("Connected devices"),
                {QStringLiteral("input method"), QStringLiteral("fcitx"), QStringLiteral("ime"), QStringLiteral("pinyin")},
                false, QStringLiteral("handoff"), QStringLiteral("system"), QStringLiteral("secondary")),
        setting(QStringLiteral("drawing-tablet"), tr("Drawing tablet"), tr("Tablet, stylus, pressure, and button configuration"),
                QStringLiteral("draw"), QStringLiteral("kcm:kcm_tablet"), QStringLiteral("devices"), tr("Connected devices"),
                {QStringLiteral("tablet"), QStringLiteral("stylus"), QStringLiteral("pen"), QStringLiteral("wacom")},
                false, QStringLiteral("handoff"), QStringLiteral("system"), QStringLiteral("secondary")),
        setting(QStringLiteral("game-controller"), tr("Game controller"), tr("Test and configure connected game controllers"),
                QStringLiteral("sports_esports"), QStringLiteral("kcm:kcm_gamecontroller"), QStringLiteral("devices"), tr("Connected devices"),
                {QStringLiteral("controller"), QStringLiteral("gamepad"), QStringLiteral("joystick")},
                false, QStringLiteral("handoff"), QStringLiteral("system"), QStringLiteral("secondary")),
        setting(QStringLiteral("thunderbolt"), tr("Thunderbolt devices"), tr("Authorize and inspect connected Thunderbolt hardware"),
                QStringLiteral("bolt"), QStringLiteral("kcm:kcm_bolt"), QStringLiteral("devices"), tr("Connected devices"),
                {QStringLiteral("thunderbolt"), QStringLiteral("bolt"), QStringLiteral("dock")},
                false, QStringLiteral("handoff"), QStringLiteral("privileged"), QStringLiteral("secondary")),
        setting(QStringLiteral("printers"), tr("Printers"), tr("Add, remove, and configure printers and print queues"),
                QStringLiteral("print"), QStringLiteral("kcm:kcm_printer_manager"), QStringLiteral("devices"), tr("Connected devices"),
                {QStringLiteral("printer"), QStringLiteral("printing"), QStringLiteral("cups")},
                false, QStringLiteral("handoff"), QStringLiteral("system"), QStringLiteral("secondary")),

        setting(QStringLiteral("appearance"), tr("Appearance"), tr("Inspect the active Meo visual system and apply its current dynamic color scheme"),
                QStringLiteral("palette"), QStringLiteral("appearance"), QStringLiteral("personalization"), tr("Wallpaper & style"),
        {QStringLiteral("theme"), QStringLiteral("dark"), QStringLiteral("light"), QStringLiteral("color"),
                 QStringLiteral("dynamic"), QStringLiteral("hct"), QStringLiteral("meo"), QStringLiteral("wallpaper"),
                 QStringLiteral("look and feel"), QStringLiteral("plasma style")},
                true, QStringLiteral("control"), QStringLiteral("system"), QStringLiteral("tertiary")),
        setting(QStringLiteral("colors"), tr("Dynamic color & contrast"), tr("Color scheme and contrast settings from KDE"),
                QStringLiteral("colors"), QStringLiteral("kcm:kcm_colors"), QStringLiteral("personalization"), tr("Wallpaper & style"),
                {QStringLiteral("dynamic"), QStringLiteral("color"), QStringLiteral("contrast"), QStringLiteral("scheme")},
                false, QStringLiteral("handoff"), QStringLiteral("system"), QStringLiteral("tertiary")),
        setting(QStringLiteral("wallpaper"), tr("Wallpaper"), tr("Desktop wallpaper and background behavior"),
                QStringLiteral("wallpaper"), QStringLiteral("kcm:kcm_wallpaper"), QStringLiteral("personalization"), tr("Wallpaper & style"),
                {QStringLiteral("wallpaper"), QStringLiteral("background")},
                false, QStringLiteral("handoff"), QStringLiteral("system"), QStringLiteral("tertiary")),
        setting(QStringLiteral("icons"), tr("Icons"), tr("Icon theme for applications and the workspace"),
                QStringLiteral("apps"), QStringLiteral("kcm:kcm_icons"), QStringLiteral("personalization"), tr("Wallpaper & style"),
                {QStringLiteral("icon"), QStringLiteral("theme")},
                false, QStringLiteral("handoff"), QStringLiteral("system"), QStringLiteral("tertiary")),
        setting(QStringLiteral("fonts"), tr("Fonts"), tr("Font family, rendering, and sizing"),
                QStringLiteral("format_size"), QStringLiteral("kcm:kcm_fonts"), QStringLiteral("personalization"), tr("Wallpaper & style"),
                {QStringLiteral("font"), QStringLiteral("text"), QStringLiteral("antialiasing")},
                false, QStringLiteral("handoff"), QStringLiteral("system"), QStringLiteral("tertiary")),
        setting(QStringLiteral("application-style"), tr("Application style"), tr("Widget style and application toolbar behavior"),
                QStringLiteral("web_asset"), QStringLiteral("kcm:kcm_style"), QStringLiteral("personalization"), tr("Wallpaper & style"),
                {QStringLiteral("application style"), QStringLiteral("widget style"), QStringLiteral("toolbar")},
                false, QStringLiteral("handoff"), QStringLiteral("system"), QStringLiteral("tertiary")),
        setting(QStringLiteral("plasma-style"), tr("Plasma style"), tr("Theme for panels, widgets, popups, and notifications"),
                QStringLiteral("dashboard"), QStringLiteral("kcm:kcm_desktoptheme"), QStringLiteral("personalization"), tr("Wallpaper & style"),
                {QStringLiteral("plasma style"), QStringLiteral("desktop theme"), QStringLiteral("panel"), QStringLiteral("widget")},
                false, QStringLiteral("handoff"), QStringLiteral("system"), QStringLiteral("tertiary")),
        setting(QStringLiteral("cursors"), tr("Cursors"), tr("Pointer theme, size, and animation"),
                QStringLiteral("mouse"), QStringLiteral("kcm:kcm_cursortheme"), QStringLiteral("personalization"), tr("Wallpaper & style"),
                {QStringLiteral("cursor"), QStringLiteral("pointer"), QStringLiteral("mouse theme")},
                false, QStringLiteral("handoff"), QStringLiteral("system"), QStringLiteral("tertiary")),
        setting(QStringLiteral("window-decorations"), tr("Window decorations"), tr("Title bars, borders, buttons, and decoration themes"),
                QStringLiteral("select_window"), QStringLiteral("kcm:kcm_kwindecoration"), QStringLiteral("personalization"), tr("Wallpaper & style"),
                {QStringLiteral("window decoration"), QStringLiteral("title bar"), QStringLiteral("border"), QStringLiteral("buttons")},
                false, QStringLiteral("handoff"), QStringLiteral("system"), QStringLiteral("tertiary")),
        setting(QStringLiteral("welcome-screen"), tr("Welcome screen"), tr("Plasma session startup animation"),
                QStringLiteral("animation"), QStringLiteral("kcm:kcm_splashscreen"), QStringLiteral("personalization"), tr("Wallpaper & style"),
                {QStringLiteral("splash"), QStringLiteral("welcome"), QStringLiteral("startup animation")},
                false, QStringLiteral("handoff"), QStringLiteral("system"), QStringLiteral("tertiary")),
        setting(QStringLiteral("sound-theme"), tr("Sound theme"), tr("Notification and desktop event sounds"),
                QStringLiteral("music_note"), QStringLiteral("kcm:kcm_soundtheme"), QStringLiteral("personalization"), tr("Wallpaper & style"),
                {QStringLiteral("sound theme"), QStringLiteral("notification sound"), QStringLiteral("event sound")},
                false, QStringLiteral("handoff"), QStringLiteral("system"), QStringLiteral("tertiary")),

        setting(QStringLiteral("notifications"), tr("Notifications"), tr("Do Not Disturb, popups, task notifications, and badges"),
                QStringLiteral("notifications"), QStringLiteral("notifications"), QStringLiteral("apps"), tr("Apps & notifications"),
                {QStringLiteral("notification"), QStringLiteral("do not disturb"), QStringLiteral("dnd"),
                 QStringLiteral("focus"), QStringLiteral("popup"), QStringLiteral("timeout"), QStringLiteral("badge")},
                true, QStringLiteral("control"), QStringLiteral("reversible"), QStringLiteral("primary")),
        setting(QStringLiteral("default-apps"), tr("Default apps"), tr("Choose preferred applications"),
                QStringLiteral("apps"), QStringLiteral("kcm:kcm_componentchooser"), QStringLiteral("apps"), tr("Apps & notifications"),
                {QStringLiteral("default"), QStringLiteral("browser"), QStringLiteral("mail"), QStringLiteral("application")},
                false, QStringLiteral("handoff"), QStringLiteral("system"), QStringLiteral("primary")),
        setting(QStringLiteral("filetypes"), tr("File associations"), tr("Choose applications for file and MIME types"),
                QStringLiteral("file_open"), QStringLiteral("kcm:kcm_filetypes"), QStringLiteral("apps"), tr("Apps & notifications"),
                {QStringLiteral("mime"), QStringLiteral("file"), QStringLiteral("association")},
                false, QStringLiteral("handoff"), QStringLiteral("system"), QStringLiteral("primary")),
        setting(QStringLiteral("autostart"), tr("Autostart"), tr("Applications that run when you sign in"),
                QStringLiteral("play_circle"), QStringLiteral("kcm:kcm_autostart"), QStringLiteral("apps"), tr("Apps & notifications"),
                {QStringLiteral("startup"), QStringLiteral("autostart"), QStringLiteral("login")},
                false, QStringLiteral("handoff"), QStringLiteral("system"), QStringLiteral("primary")),

        setting(QStringLiteral("accounts"), tr("Meo Account"), tr("Connect your cloud profile, then manage the local device account from one page"),
                QStringLiteral("account_circle"), QStringLiteral("accounts"), QStringLiteral("accounts"), tr("Accounts & sync"),
                {QStringLiteral("meo account"), QStringLiteral("cloud profile"), QStringLiteral("identity"),
                 QStringLiteral("avatar"), QStringLiteral("local user"), QStringLiteral("account")},
                true, QStringLiteral("account"), QStringLiteral("system"), QStringLiteral("secondary")),
        setting(QStringLiteral("online-accounts"), tr("Online accounts"), tr("Manage supported KAccounts providers and integrations"),
                QStringLiteral("cloud"), QStringLiteral("kcm:kcm_kaccounts"), QStringLiteral("accounts"), tr("Accounts & sync"),
                {QStringLiteral("google"), QStringLiteral("nextcloud"), QStringLiteral("calendar"), QStringLiteral("contacts")},
                false, QStringLiteral("handoff"), QStringLiteral("system"), QStringLiteral("secondary")),
        setting(QStringLiteral("wallet"), tr("Password wallet"), tr("Manage the KDE wallet and application credential access"),
                QStringLiteral("key"), QStringLiteral("kcm:kcm_kwallet5"), QStringLiteral("accounts"), tr("Accounts & sync"),
                {QStringLiteral("wallet"), QStringLiteral("password"), QStringLiteral("credentials"), QStringLiteral("kwallet")},
                false, QStringLiteral("handoff"), QStringLiteral("privileged"), QStringLiteral("secondary")),

        setting(QStringLiteral("storage"), tr("Storage & applications"), tr("Read mounted volumes, free space, and OmniStore application usage without changing disks"),
                QStringLiteral("storage"), QStringLiteral("storage"), QStringLiteral("storage"), tr("Storage & backup"),
                {QStringLiteral("storage"), QStringLiteral("disk"), QStringLiteral("space"), QStringLiteral("volume"),
                 QStringLiteral("mount"), QStringLiteral("application"), QStringLiteral("app usage"),
                 QStringLiteral("installed apps"), QStringLiteral("omnistore"), QStringLiteral("source share")},
                true, QStringLiteral("inspector"), QStringLiteral("read-only"), QStringLiteral("tertiary")),
        setting(QStringLiteral("automount"), tr("Removable devices"), tr("Configure automatic handling of removable media"),
                QStringLiteral("usb"), QStringLiteral("kcm:kcm_device_automounter"), QStringLiteral("storage"), tr("Storage & backup"),
                {QStringLiteral("usb"), QStringLiteral("removable"), QStringLiteral("automount"), QStringLiteral("external")},
                false, QStringLiteral("handoff"), QStringLiteral("system"), QStringLiteral("tertiary")),
        setting(QStringLiteral("disk-health"), tr("Disk health"), tr("Open KDE's installed disk information module"),
                QStringLiteral("hard_drive"), QStringLiteral("kcm:kcm_disks"), QStringLiteral("storage"), tr("Storage & backup"),
                {QStringLiteral("disk"), QStringLiteral("health"), QStringLiteral("smart"), QStringLiteral("storage")},
                false, QStringLiteral("handoff"), QStringLiteral("system"), QStringLiteral("tertiary")),

        setting(QStringLiteral("power"), tr("Power & battery"), tr("Choose a power profile, inspect battery state, and manage safe session controls"),
                QStringLiteral("battery_full"), QStringLiteral("power"), QStringLiteral("system"), tr("System"),
                {QStringLiteral("battery"), QStringLiteral("power"), QStringLiteral("energy"), QStringLiteral("performance"),
                 QStringLiteral("power saver")},
                true, QStringLiteral("control"), QStringLiteral("reversible"), QStringLiteral("neutral")),
        setting(QStringLiteral("control-center"), tr("Control Center"), tr("Arrange Meo Quick Settings tiles and choose their visibility and density"),
                QStringLiteral("tune"), QStringLiteral("control-center"), QStringLiteral("system"), tr("System"),
                {QStringLiteral("control center"), QStringLiteral("quick settings"), QStringLiteral("quick controls"),
                 QStringLiteral("tiles"), QStringLiteral("panel"), QStringLiteral("density")},
                true, QStringLiteral("control"), QStringLiteral("reversible"), QStringLiteral("neutral")),
        setting(QStringLiteral("desktop-integration"), tr("Desktop integration"), tr("Inspect how Meo Settings is connected to KDE, Plasma, hardware services, Account, and OmniStore"),
                QStringLiteral("hub"), QStringLiteral("desktop-integration"), QStringLiteral("system"), tr("System"),
                {QStringLiteral("integration"), QStringLiteral("kde"), QStringLiteral("plasma"), QStringLiteral("service"),
                 QStringLiteral("connection"), QStringLiteral("backend"), QStringLiteral("kcm")},
                true, QStringLiteral("inspector"), QStringLiteral("read-only"), QStringLiteral("neutral")),
        setting(QStringLiteral("date-time"), tr("Date & time"), tr("Clock, date, time zone, and synchronization"),
                QStringLiteral("schedule"), QStringLiteral("kcm:kcm_clock"), QStringLiteral("system"), tr("System"),
                {QStringLiteral("date"), QStringLiteral("time"), QStringLiteral("timezone"), QStringLiteral("clock")},
                false, QStringLiteral("handoff"), QStringLiteral("system"), QStringLiteral("neutral")),
        setting(QStringLiteral("language"), tr("Language & region"), tr("Language, formats, and locale"),
                QStringLiteral("language"), QStringLiteral("kcm:kcm_regionandlang"), QStringLiteral("system"), tr("System"),
                {QStringLiteral("language"), QStringLiteral("locale"), QStringLiteral("region")},
                false, QStringLiteral("handoff"), QStringLiteral("system"), QStringLiteral("neutral")),
        setting(QStringLiteral("search-indexing"), tr("Search & indexing"), tr("Manage KDE file search and indexing"),
                QStringLiteral("search"), QStringLiteral("kcm:kcm_baloofile"), QStringLiteral("system"), tr("System"),
                {QStringLiteral("search"), QStringLiteral("index"), QStringLiteral("baloo"), QStringLiteral("files")},
                false, QStringLiteral("handoff"), QStringLiteral("system"), QStringLiteral("neutral")),
        setting(QStringLiteral("workspace"), tr("Workspace"), tr("Virtual desktops and workspace behavior"),
                QStringLiteral("dashboard"), QStringLiteral("kcm:kcm_workspace"), QStringLiteral("system"), tr("System"),
                {QStringLiteral("workspace"), QStringLiteral("virtual desktops"), QStringLiteral("activities")},
                false, QStringLiteral("handoff"), QStringLiteral("system"), QStringLiteral("neutral")),
        setting(QStringLiteral("startup-session"), tr("Startup & session"), tr("Choose how your KDE session starts and restores"),
                QStringLiteral("restart_alt"), QStringLiteral("kcm:kcm_smserver"), QStringLiteral("system"), tr("System"),
                {QStringLiteral("startup"), QStringLiteral("session"), QStringLiteral("restore"), QStringLiteral("login")},
                false, QStringLiteral("handoff"), QStringLiteral("system"), QStringLiteral("neutral")),
        setting(QStringLiteral("background-services"), tr("Background services"), tr("Configure KDE background services started for this session"),
                QStringLiteral("settings_suggest"), QStringLiteral("kcm:kcm_kded"), QStringLiteral("system"), tr("System"),
                {QStringLiteral("background service"), QStringLiteral("daemon"), QStringLiteral("kded")},
                false, QStringLiteral("handoff"), QStringLiteral("system"), QStringLiteral("neutral")),
        setting(QStringLiteral("virtual-desktops"), tr("Virtual desktops"), tr("Desktop count, layout, switching, and animations"),
                QStringLiteral("view_carousel"), QStringLiteral("kcm:kcm_kwin_virtualdesktops"), QStringLiteral("system"), tr("System"),
                {QStringLiteral("virtual desktop"), QStringLiteral("workspace"), QStringLiteral("desktop switch")},
                false, QStringLiteral("handoff"), QStringLiteral("system"), QStringLiteral("neutral")),
        setting(QStringLiteral("window-behavior"), tr("Window behavior"), tr("Focus, placement, title-bar actions, and window rules"),
                QStringLiteral("picture_in_picture"), QStringLiteral("kcm:kcm_kwinoptions"), QStringLiteral("system"), tr("System"),
                {QStringLiteral("window behavior"), QStringLiteral("focus"), QStringLiteral("placement"), QStringLiteral("raise")},
                false, QStringLiteral("handoff"), QStringLiteral("system"), QStringLiteral("neutral")),
        setting(QStringLiteral("desktop-effects"), tr("Desktop effects"), tr("KWin effects, compositing visuals, and accessibility effects"),
                QStringLiteral("blur_on"), QStringLiteral("kcm:kcm_kwin_effects"), QStringLiteral("system"), tr("System"),
                {QStringLiteral("effect"), QStringLiteral("blur"), QStringLiteral("animation"), QStringLiteral("kwin")},
                false, QStringLiteral("handoff"), QStringLiteral("system"), QStringLiteral("neutral")),
        setting(QStringLiteral("screen-edges"), tr("Screen edges"), tr("Actions triggered by screen corners and edges"),
                QStringLiteral("crop_free"), QStringLiteral("kcm:kcm_kwinscreenedges"), QStringLiteral("system"), tr("System"),
                {QStringLiteral("screen edge"), QStringLiteral("hot corner"), QStringLiteral("corner")},
                false, QStringLiteral("handoff"), QStringLiteral("system"), QStringLiteral("neutral")),
        setting(QStringLiteral("remote-desktop"), tr("Remote desktop"), tr("Configure KDE remote-desktop access and sharing"),
                QStringLiteral("desktop_windows"), QStringLiteral("kcm:kcm_krdpserver"), QStringLiteral("system"), tr("System"),
                {QStringLiteral("remote desktop"), QStringLiteral("rdp"), QStringLiteral("screen sharing")},
                false, QStringLiteral("handoff"), QStringLiteral("privileged"), QStringLiteral("neutral")),
        setting(QStringLiteral("spell-checking"), tr("Spell checking"), tr("Dictionaries and spell-check behavior used by KDE applications"),
                QStringLiteral("spellcheck"), QStringLiteral("kcm:kcmspellchecking"), QStringLiteral("system"), tr("System"),
                {QStringLiteral("spell"), QStringLiteral("dictionary"), QStringLiteral("language")},
                false, QStringLiteral("handoff"), QStringLiteral("system"), QStringLiteral("neutral")),
        setting(QStringLiteral("accessibility"), tr("Accessibility"), tr("Assistive technology and accessibility options"),
                QStringLiteral("accessibility"), QStringLiteral("kcm:kcm_access"), QStringLiteral("accessibility"), tr("Accessibility"),
                {QStringLiteral("accessibility"), QStringLiteral("a11y")},
                false, QStringLiteral("handoff"), QStringLiteral("system"), QStringLiteral("primary")),
        setting(QStringLiteral("virtual-keyboard"), tr("Virtual keyboard"), tr("Configure the system virtual keyboard when available"),
                QStringLiteral("keyboard"), QStringLiteral("kcm:kcm_virtualkeyboard"), QStringLiteral("accessibility"), tr("Accessibility"),
                {QStringLiteral("virtual keyboard"), QStringLiteral("on screen keyboard"), QStringLiteral("input")},
                false, QStringLiteral("handoff"), QStringLiteral("system"), QStringLiteral("primary")),
        setting(QStringLiteral("app-permissions"), tr("App permissions"), tr("Review verified Flatpak sandbox permissions by application"),
                QStringLiteral("admin_panel_settings"), QStringLiteral("privacy"), QStringLiteral("privacy"), tr("Privacy & security"),
                {QStringLiteral("permissions"), QStringLiteral("flatpak"), QStringLiteral("sandbox"),
                 QStringLiteral("camera"), QStringLiteral("microphone"), QStringLiteral("screen capture")},
                true, QStringLiteral("inspector"), QStringLiteral("read-only"), QStringLiteral("neutral")),
        setting(QStringLiteral("screen-lock"), tr("Screen lock"), tr("Lock screen behavior and security"),
                QStringLiteral("lock"), QStringLiteral("kcm:kcm_screenlocker"), QStringLiteral("privacy"), tr("Privacy & security"),
                {QStringLiteral("lock"), QStringLiteral("screen"), QStringLiteral("security")},
                false, QStringLiteral("handoff"), QStringLiteral("privileged"), QStringLiteral("neutral")),
        setting(QStringLiteral("firewall"), tr("Firewall"), tr("Network firewall rules and status"),
                QStringLiteral("security"), QStringLiteral("kcm:kcm_firewall"), QStringLiteral("privacy"), tr("Privacy & security"),
                {QStringLiteral("firewall"), QStringLiteral("security"), QStringLiteral("network")},
                false, QStringLiteral("handoff"), QStringLiteral("privileged"), QStringLiteral("neutral")),
        setting(QStringLiteral("device-security"), tr("Device security"), tr("Inspect firmware and hardware security information"),
                QStringLiteral("shield"), QStringLiteral("kcm:kcm_firmware_security"), QStringLiteral("privacy"), tr("Privacy & security"),
                {QStringLiteral("secure boot"), QStringLiteral("tpm"), QStringLiteral("firmware"), QStringLiteral("security")},
                false, QStringLiteral("handoff"), QStringLiteral("read-only"), QStringLiteral("neutral")),
        setting(QStringLiteral("updates"), tr("Updates"), tr("Inspect Meo, KDE, system, custom-repository, and AUR update candidates without installing anything"),
                QStringLiteral("system_update"), QStringLiteral("updates"), QStringLiteral("updates"), tr("Updates & support"),
                {QStringLiteral("update"), QStringLiteral("upgrades"), QStringLiteral("packages"), QStringLiteral("pacman"),
                 QStringLiteral("aur"), QStringLiteral("meo"), QStringLiteral("kde"), QStringLiteral("custom repository")},
                true, QStringLiteral("inspector"), QStringLiteral("read-only"), QStringLiteral("primary")),
        setting(QStringLiteral("system-updates-advanced"), tr("System update tool"), tr("Open the installed KDE system updater for repository refresh and package changes"),
                QStringLiteral("system_update"), QStringLiteral("kcm:kcm_updates"), QStringLiteral("updates"), tr("Updates & support"),
                {QStringLiteral("update"), QStringLiteral("upgrades"), QStringLiteral("packages"), QStringLiteral("restart")},
                false, QStringLiteral("handoff"), QStringLiteral("system"), QStringLiteral("primary")),
        setting(QStringLiteral("about"), tr("About"), tr("MeoArch and device information"),
                QStringLiteral("info"), QStringLiteral("about"), QString(), tr("About"),
                {QStringLiteral("version"), QStringLiteral("os"), QStringLiteral("memory"), QStringLiteral("kernel"), QStringLiteral("device")},
                true, QStringLiteral("info"), QStringLiteral("read-only"), QStringLiteral("neutral")),
    };

    m_sidebarEntries = {
        setting(QStringLiteral("home"), tr("Home"), tr("Search and common settings"),
                QStringLiteral("home"), QStringLiteral("home"), QString(), QString(), {}, true,
                QStringLiteral("settings-index"), QStringLiteral("read-only"), QStringLiteral("neutral")),
        sidebarHeader(tr("Connections")),
        setting(QStringLiteral("network"), tr("Network"), tr("Wi-Fi, wired, VPN, and proxy"),
                QStringLiteral("wifi"), QStringLiteral("category:network"), QStringLiteral("network"), tr("Network & Internet"), {}, true,
                QStringLiteral("settings-category"), QStringLiteral("read-only"), QStringLiteral("primary")),
        setting(QStringLiteral("devices"), tr("Devices"), tr("Bluetooth, peripherals, and input"),
                QStringLiteral("devices"), QStringLiteral("category:devices"), QStringLiteral("devices"), tr("Connected devices"), {}, true,
                QStringLiteral("settings-category"), QStringLiteral("read-only"), QStringLiteral("secondary")),
        setting(QStringLiteral("display-sound"), tr("Display & sound"), tr("Displays, audio devices, and night light"),
                QStringLiteral("monitor"), QStringLiteral("category:display-sound"), QStringLiteral("display-sound"), tr("Display & sound"), {}, true,
                QStringLiteral("settings-category"), QStringLiteral("read-only"), QStringLiteral("secondary")),
        sidebarHeader(tr("Personal")),
        setting(QStringLiteral("personalization"), tr("Style"), tr("Wallpaper, colors, icons, and fonts"),
                QStringLiteral("palette"), QStringLiteral("category:personalization"), QStringLiteral("personalization"), tr("Wallpaper & style"), {}, true,
                QStringLiteral("settings-category"), QStringLiteral("read-only"), QStringLiteral("tertiary")),
        setting(QStringLiteral("apps"), tr("Apps"), tr("Notifications, defaults, and startup"),
                QStringLiteral("apps"), QStringLiteral("category:apps"), QStringLiteral("apps"), tr("Apps & notifications"), {}, true,
                QStringLiteral("settings-category"), QStringLiteral("read-only"), QStringLiteral("primary")),
        setting(QStringLiteral("accounts"), tr("Accounts"), tr("Meo Account, local device users, and wallet"),
                QStringLiteral("account_circle"), QStringLiteral("category:accounts"), QStringLiteral("accounts"), tr("Accounts & sync"), {}, true,
                QStringLiteral("settings-category"), QStringLiteral("read-only"), QStringLiteral("secondary")),
        sidebarHeader(tr("System")),
        setting(QStringLiteral("storage"), tr("Storage"), tr("Volumes, removable devices, and disk tools"),
                QStringLiteral("storage"), QStringLiteral("category:storage"), QStringLiteral("storage"), tr("Storage & backup"), {}, true,
                QStringLiteral("settings-category"), QStringLiteral("read-only"), QStringLiteral("tertiary")),
        setting(QStringLiteral("system"), tr("System"), tr("Power, time, language, and sessions"),
                QStringLiteral("settings"), QStringLiteral("category:system"), QStringLiteral("system"), tr("System"), {}, true,
                QStringLiteral("settings-category"), QStringLiteral("read-only"), QStringLiteral("neutral")),
        setting(QStringLiteral("privacy"), tr("Privacy & security"), tr("Permissions, lock screen, firewall, and device security"),
                QStringLiteral("security"), QStringLiteral("category:privacy"), QStringLiteral("privacy"), tr("Privacy & security"), {}, true,
                QStringLiteral("settings-category"), QStringLiteral("read-only"), QStringLiteral("neutral")),
        setting(QStringLiteral("accessibility"), tr("Accessibility"), tr("Assistive technology and accessibility input"),
                QStringLiteral("accessibility"), QStringLiteral("category:accessibility"), QStringLiteral("accessibility"), tr("Accessibility"), {}, true,
                QStringLiteral("settings-category"), QStringLiteral("read-only"), QStringLiteral("primary")),
        setting(QStringLiteral("updates"), tr("Updates"), tr("Meo, KDE, system, custom-repository, and AUR updates"),
                QStringLiteral("system_update"), QStringLiteral("category:updates"), QStringLiteral("updates"), tr("Updates & support"), {}, true,
                QStringLiteral("settings-category"), QStringLiteral("read-only"), QStringLiteral("primary")),
        setting(QStringLiteral("about"), tr("About"), tr("System information"),
                QStringLiteral("info"), QStringLiteral("about"), QString(), tr("About"), {}, true,
                QStringLiteral("info"), QStringLiteral("read-only"), QStringLiteral("neutral")),
    };
}

QVariantList SettingsRegistry::entries() const
{
    return m_entries;
}

QVariantList SettingsRegistry::sidebarEntries() const
{
    return m_sidebarEntries;
}

QVariantList SettingsRegistry::categories() const
{
    return m_categories;
}

QVariantList SettingsRegistry::search(const QString &query) const
{
    const auto needle = query.trimmed();
    if (needle.isEmpty()) {
        return {};
    }

    QVariantList matches;
    const auto normalizedNeedle = needle.toCaseFolded();
    // A query such as "popup timeout" should find a row whose aliases are
    // stored independently as "popup" and "timeout".  Requiring a literal
    // phrase in one field made Settings search weaker than the terminology
    // map it already owns.  Keep a literal phrase fast-path, then require
    // every meaningful query token across the complete entry vocabulary.
    const auto tokens = normalizedNeedle.split(
        QRegularExpression(QStringLiteral("[\\s\\p{P}]+")), Qt::SkipEmptyParts);
    for (const auto &item : m_entries) {
        const auto candidate = item.toMap();
        QStringList searchable{
            candidate.value(QStringLiteral("title")).toString(),
            candidate.value(QStringLiteral("description")).toString(),
            candidate.value(QStringLiteral("category")).toString(),
        };
        searchable.append(candidate.value(QStringLiteral("keywords")).toStringList());
        const QString vocabulary = searchable.join(QLatin1Char(' ')).toCaseFolded();
        const bool phraseMatch = vocabulary.contains(normalizedNeedle);
        const bool tokenMatch = !tokens.isEmpty()
            && std::all_of(tokens.cbegin(), tokens.cend(), [&vocabulary](const QString &token) {
                   return vocabulary.contains(token);
               });
        const bool matchesEntry = phraseMatch || tokenMatch;
        if (matchesEntry) {
            matches.push_back(candidate);
        }
    }
    return matches;
}

QVariantMap SettingsRegistry::entry(const QString &idOrRoute) const
{
    for (const auto &item : m_entries) {
        const auto candidate = item.toMap();
        if (candidate.value(QStringLiteral("id")).toString() == idOrRoute
            || candidate.value(QStringLiteral("route")).toString() == idOrRoute) {
            return candidate;
        }
    }
    for (const auto &item : m_sidebarEntries) {
        const auto candidate = item.toMap();
        if (candidate.value(QStringLiteral("id")).toString() == idOrRoute
            || candidate.value(QStringLiteral("route")).toString() == idOrRoute) {
            return candidate;
        }
    }
    return {};
}

QVariantMap SettingsRegistry::category(const QString &id) const
{
    for (const auto &item : m_categories) {
        const auto candidate = item.toMap();
        if (candidate.value(QStringLiteral("id")).toString() == id) {
            return candidate;
        }
    }
    return {};
}

QVariantList SettingsRegistry::entriesForCategory(const QString &categoryId) const
{
    QVariantList matches;
    for (const auto &item : m_entries) {
        const auto candidate = item.toMap();
        if (candidate.value(QStringLiteral("categoryId")).toString() == categoryId) {
            matches.push_back(candidate);
        }
    }
    return matches;
}
