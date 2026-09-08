#include "fingerprintbackend.h"

#include <QDir>
#include <QFile>

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
}

FingerprintBackend::FingerprintBackend(QObject *parent)
    : QObject(parent)
{
    refresh();
}

bool FingerprintBackend::devicePresent() const { return m_fpc9800; }

QString FingerprintBackend::deviceLabel() const
{
    return m_fpc9800 ? tr("FPC 10a5:9800") : tr("No supported fingerprint device detected");
}

QString FingerprintBackend::supportLevel() const
{
    return m_fpc9800 ? QStringLiteral("experimental") : QStringLiteral("unavailable");
}

QString FingerprintBackend::supportSummary() const
{
    return m_fpc9800
        ? tr("Upstream libfprint support is unavailable. Meo support requires explicit consent for a third-party FPC binary component.")
        : tr("Connect a detected fingerprint reader to review its support status.");
}

bool FingerprintBackend::rawBiometricDataExposed() const { return false; }

QVariantMap FingerprintBackend::supportPackage() const
{
    return {{QStringLiteral("available"), m_fpc9800},
            {QStringLiteral("provider"), QStringLiteral("meo-fprint-fpcmoh")},
            {QStringLiteral("requiresConsent"), true},
            {QStringLiteral("passwordFallbackRequired"), true},
            {QStringLiteral("defaultPAMTargets"), QStringList{QStringLiteral("lock-screen")}},
            {QStringLiteral("disabledPAMTargets"), QStringList{QStringLiteral("login"), QStringLiteral("polkit"), QStringLiteral("sudo")}},
            {QStringLiteral("privacy"), tr("Meo does not read or export fingerprint images, templates, or matcher internals.")}};
}

void FingerprintBackend::refresh()
{
    bool next = false;
    const QDir usb(QStringLiteral("/sys/bus/usb/devices"));
    for (const auto &entry : usb.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (readTrimmed(entry.filePath() + QStringLiteral("/idVendor")) == QLatin1String("10a5")
            && readTrimmed(entry.filePath() + QStringLiteral("/idProduct")) == QLatin1String("9800")) {
            next = true;
            break;
        }
    }
    if (next == m_fpc9800) {
        return;
    }
    m_fpc9800 = next;
    Q_EMIT changed();
}
