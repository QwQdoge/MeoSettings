#include "hardwarecapabilityregistry.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <utility>

namespace
{
QString readTrimmed(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromUtf8(file.readAll()).trimmed().toLower();
}

QVariantMap makeCapability(QString id, QString title, QString supportLevel, QString provider,
                           QString evidence, QString risk, QStringList keywords = {})
{
    return {
        {QStringLiteral("id"), std::move(id)},
        {QStringLiteral("title"), std::move(title)},
        {QStringLiteral("supportLevel"), std::move(supportLevel)},
        {QStringLiteral("provider"), std::move(provider)},
        {QStringLiteral("evidence"), std::move(evidence)},
        {QStringLiteral("risk"), std::move(risk)},
        {QStringLiteral("keywords"), std::move(keywords)},
    };
}
}

HardwareCapabilityRegistry::HardwareCapabilityRegistry(QObject *parent)
    : QObject(parent)
{
    refresh();
}

QVariantList HardwareCapabilityRegistry::capabilities() const
{
    return m_capabilities;
}

QString HardwareCapabilityRegistry::summary() const
{
    int ready = 0;
    for (const auto &entry : m_capabilities) {
        if (entry.toMap().value(QStringLiteral("supportLevel")) == QLatin1String("supported")) {
            ++ready;
        }
    }
    return tr("%n hardware capability/capabilities ready", "", ready);
}

QVariantMap HardwareCapabilityRegistry::capability(const QString &id) const
{
    for (const auto &entry : m_capabilities) {
        const auto candidate = entry.toMap();
        if (candidate.value(QStringLiteral("id")) == id) {
            return candidate;
        }
    }
    return {};
}

void HardwareCapabilityRegistry::refresh()
{
    QVariantList next;
    bool nvidia = false;
    const QDir drm(QStringLiteral("/sys/class/drm"));
    for (const auto &entry : drm.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const auto name = entry.fileName();
        if (!name.startsWith(QLatin1String("card")) || name.contains(QLatin1Char('-'))) {
            continue;
        }
        const auto vendor = readTrimmed(entry.filePath() + QStringLiteral("/device/vendor"));
        if (vendor == QLatin1String("0x10de")) {
            nvidia = true;
            break;
        }
    }
    if (nvidia) {
        next.push_back(makeCapability(QStringLiteral("graphics.nvidia"), tr("NVIDIA graphics"),
                                  QStringLiteral("supported"), QStringLiteral("nvidia-open"),
                                  tr("PCI display controller reports NVIDIA vendor 10de."),
                                  tr("A kernel restart is required after driver or kernel changes."),
                                  {QStringLiteral("nvidia"), QStringLiteral("gpu"), QStringLiteral("nvenc"), QStringLiteral("wayland")}));
    }

    bool fpc9800 = false;
    const QDir usb(QStringLiteral("/sys/bus/usb/devices"));
    for (const auto &entry : usb.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const auto vendor = readTrimmed(entry.filePath() + QStringLiteral("/idVendor"));
        const auto product = readTrimmed(entry.filePath() + QStringLiteral("/idProduct"));
        if (vendor == QLatin1String("10a5") && product == QLatin1String("9800")) {
            fpc9800 = true;
            break;
        }
    }
    if (fpc9800) {
        next.push_back(makeCapability(QStringLiteral("fingerprint.fpc-10a5-9800"), tr("FPC fingerprint reader"),
                                  QStringLiteral("experimental"), QStringLiteral("meo-fprint-fpcmoh"),
                                  tr("USB device 10a5:9800 was detected."),
                                  tr("Requires an opt-in third-party FPC binary component; raw biometric data remains in fprintd/libfprint."),
                                  {QStringLiteral("fingerprint"), QStringLiteral("fprint"), QStringLiteral("fpc"), QStringLiteral("biometric")}));
    }

    next.push_back(makeCapability(QStringLiteral("display.layout"), tr("Display layout"),
                              QStringLiteral("supported"), QStringLiteral("KScreen"),
                              tr("KScreen is available through KDE's supported screen-management API."),
                              tr("A bad layout can remove the visible output; Meo requires confirmation and automatic recovery."),
                              {QStringLiteral("display"), QStringLiteral("monitor"), QStringLiteral("refresh rate"), QStringLiteral("second display")}));
    if (next == m_capabilities) {
        return;
    }
    m_capabilities = std::move(next);
    Q_EMIT changed();
}
