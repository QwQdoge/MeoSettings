#pragma once

#include "../core/backendbase.h"

#include <QVariantMap>

// Narrow, user-scoped writer for Meo Look & Feel presentation preferences.
// Authentication policy, PAM, lock timing, and display-manager configuration
// deliberately remain outside this backend.
class LockScreenPresentationBackend final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap settings READ settings NOTIFY changed)

public:
    explicit LockScreenPresentationBackend(QObject *parent = nullptr);

    QVariantMap settings() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void save(const QVariantMap &settings);
    Q_INVOKABLE void resetToDefaults();

    static QVariantMap defaults();
    static QVariantMap normalized(const QVariantMap &settings, QString *error = nullptr);

Q_SIGNALS:
    void changed();
    void saved();

private:
    QVariantMap m_settings;
};
