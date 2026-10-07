#pragma once

#include "../core/backendbase.h"
#include "../core/lockscreenconfigstore.h"

#include <QVariantMap>

// User-scoped presentation adapter for the Meo Session Lock v1 document.
//
// QML sees only the small set of controls the current lock runtime consumes.
// Authentication, PAM, lock timing, the system login document and package
// installation remain outside this backend.
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
    LockScreenConfigStore m_store;
    QVariantMap m_settings;
};
