#pragma once

#include "../core/backendbase.h"

#include <QString>
#include <QStringList>

/**
 * Narrow launcher for the separately owned MeoArch Repair application.
 *
 * Settings may select only one installed, versioned repair category.  It does
 * not expose an executable path, arbitrary arguments, or a shell to QML.
 */
class RepairLauncher final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(QStringList supportedCategories READ supportedCategories CONSTANT)

public:
    explicit RepairLauncher(QObject *parent = nullptr);

    QStringList supportedCategories() const;

    Q_INVOKABLE bool supportsCategory(const QString &category) const;
    Q_INVOKABLE bool open(const QString &category);
    Q_INVOKABLE void refresh();

private:
    QString normalizedCategory(const QString &category) const;

    QString m_launcher;
};
