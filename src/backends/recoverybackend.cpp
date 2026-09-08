#include "recoverybackend.h"

#include <QFileInfo>
#include <QStorageInfo>

#include <utility>

namespace
{
QVariantMap recoveryCapability(QString id, QString title, bool available, QString detail)
{
    return {{QStringLiteral("id"), std::move(id)}, {QStringLiteral("title"), std::move(title)},
            {QStringLiteral("available"), available}, {QStringLiteral("detail"), std::move(detail)}};
}
}

RecoveryBackend::RecoveryBackend(QObject *parent)
    : QObject(parent)
{
    refresh();
}

QString RecoveryBackend::mode() const { return m_mode; }

QString RecoveryBackend::summary() const
{
    return m_mode == QLatin1String("btrfs")
        ? tr("Full Btrfs recovery is available when the Meo transaction service is installed.")
        : tr("Limited recovery: configuration history, package cache, LTS kernel, and repair environment.");
}

QVariantList RecoveryBackend::capabilities() const { return m_capabilities; }

void RecoveryBackend::refresh()
{
    const QStorageInfo root = QStorageInfo::root();
    const bool btrfs = root.fileSystemType().toLower() == QByteArrayLiteral("btrfs");
    const bool lts = QFileInfo::exists(QStringLiteral("/boot/vmlinuz-linux-lts"));
    const bool repair = QFileInfo::exists(QStringLiteral("/usr/bin/meoarch-repair"));
    const QString nextMode = btrfs ? QStringLiteral("btrfs") : QStringLiteral("limited");
    const QVariantList next{
        recoveryCapability(QStringLiteral("snapshot"), tr("Update snapshots"), btrfs,
                           btrfs ? tr("Btrfs root detected; snapshots can provide an atomic recovery point.")
                                 : tr("Requires a Btrfs root filesystem.")),
        recoveryCapability(QStringLiteral("known-good-kernel"), tr("Known-good kernel"), lts,
                           lts ? tr("linux-lts is installed as a fallback boot option.")
                               : tr("Install linux-lts before relying on a fallback kernel.")),
        recoveryCapability(QStringLiteral("repair-environment"), tr("Repair environment"), repair,
                           repair ? tr("MeoArch Quick Repair is installed.")
                                  : tr("Boot the MeoArch repair media when the installed system cannot start.")),
    };
    if (nextMode == m_mode && next == m_capabilities) {
        return;
    }
    m_mode = nextMode;
    m_capabilities = next;
    Q_EMIT changed();
}
