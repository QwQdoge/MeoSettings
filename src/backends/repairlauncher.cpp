#include "repairlauncher.h"

#include <QProcess>
#include <QStandardPaths>

namespace {
const QStringList kSupportedCategories{
    QStringLiteral("all"),
    QStringLiteral("general"),
    QStringLiteral("audio"),
    QStringLiteral("display"),
    QStringLiteral("network"),
    QStringLiteral("boot"),
    QStringLiteral("packages"),
    QStringLiteral("storage"),
    QStringLiteral("graphics"),
    QStringLiteral("security"),
};
}

RepairLauncher::RepairLauncher(QObject *parent)
    : BackendBase(parent)
{
    refresh();
}

QStringList RepairLauncher::supportedCategories() const
{
    return kSupportedCategories;
}

QString RepairLauncher::normalizedCategory(const QString &category) const
{
    return category.trimmed().toLower();
}

bool RepairLauncher::supportsCategory(const QString &category) const
{
    return kSupportedCategories.contains(normalizedCategory(category));
}

bool RepairLauncher::open(const QString &category)
{
    clearError();
    const QString selectedCategory = normalizedCategory(category);
    if (!kSupportedCategories.contains(selectedCategory)) {
        setError(tr("This troubleshooting category is not supported."));
        return false;
    }
    if (m_launcher.isEmpty()) {
        setError(tr("MeoArch Repair is not installed."));
        return false;
    }
    if (!QProcess::startDetached(m_launcher,
                                 {QStringLiteral("--category"), selectedCategory})) {
        setError(tr("Unable to open MeoArch Repair."));
        return false;
    }
    return true;
}

void RepairLauncher::refresh()
{
    m_launcher = QStandardPaths::findExecutable(QStringLiteral("meoarch-repair"));
    setAvailable(!m_launcher.isEmpty());
}
