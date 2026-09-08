#include "loginauthbackend.h"

#include <QFileInfo>

#include <utility>

LoginAuthBackend::LoginAuthBackend(QObject *parent)
    : QObject(parent)
{
    refresh();
}

QString LoginAuthBackend::provider() const { return m_provider; }

QString LoginAuthBackend::providerLabel() const
{
    if (m_provider == QLatin1String("plasma-login-manager")) {
        return tr("Plasma Login Manager");
    }
    if (m_provider == QLatin1String("sddm")) {
        return tr("SDDM compatibility adapter");
    }
    return tr("No supported login manager detected");
}

bool LoginAuthBackend::passwordFallbackAvailable() const { return !m_provider.isEmpty(); }

bool LoginAuthBackend::loginFingerprintSupported() const
{
    // Login PAM policy is deliberately not guessed from a device or a package.
    // The transaction service publishes it only after it has validated a
    // password-preserving PAM configuration.
    return false;
}

void LoginAuthBackend::refresh()
{
    QString next;
    if (QFileInfo::exists(QStringLiteral("/usr/bin/plasmalogin"))
        && QFileInfo::exists(QStringLiteral("/usr/lib/systemd/system/plasmalogin.service"))) {
        next = QStringLiteral("plasma-login-manager");
    } else if (QFileInfo::exists(QStringLiteral("/usr/bin/sddm"))
               && QFileInfo::exists(QStringLiteral("/usr/lib/systemd/system/sddm.service"))) {
        next = QStringLiteral("sddm");
    }
    if (next == m_provider) {
        return;
    }
    m_provider = std::move(next);
    Q_EMIT changed();
}
