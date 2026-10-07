#pragma once

#include "../core/backendbase.h"
#include "../core/lockscreenconfigstore.h"

#include <QVariantMap>

// User-scoped editor/writer for the versioned Meo session-entry presentation
// document. Authentication policy, PAM, lock timing, display-manager state and
// the privileged login-scope writer deliberately remain outside this backend.
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
    static QVariantMap settingsFromDocument(const QVariantMap &document);
    static bool mergeSettingsIntoDocument(QVariantMap *document,
                                          const QVariantMap &settings,
                                          QString *error);

    LockScreenConfigStore m_store;
    QVariantMap m_document;
    QVariantMap m_settings;
};
