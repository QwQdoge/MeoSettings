#pragma once

#include "../core/backendbase.h"

#include <QVariantMap>

// Controls the Meo-owned Shelf and launcher hosted by the unique
// org.meo.shelf applet in the active Plasma session. Plasma Shell remains the
// persistence authority; this backend never edits plasma applet config files.
class ShelfBackend final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap settings READ settings NOTIFY changed)
    Q_PROPERTY(QString summary READ summary NOTIFY changed)

public:
    explicit ShelfBackend(QObject *parent = nullptr);

    QVariantMap settings() const;
    QString summary() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void save(const QVariantMap &settings);
    Q_INVOKABLE void reset();

    static QVariantMap normalizedSettings(const QVariantMap &settings);
    static QVariantMap serializeSettings(const QVariantMap &settings,
                                         QString *error = nullptr);
    static QString readScript();
    static QString writeScript(const QVariantMap &settings);

Q_SIGNALS:
    void changed();
    void saved();

private:
    void setSettings(const QVariantMap &settings);
    QString errorForScriptReason(const QString &reason) const;

    QVariantMap m_settings;
};
