#pragma once

#include "../core/backendbase.h"

#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

// Controls the Meo-owned Quick Settings presentation that is hosted by the
// unique org.meo.topbar applet in the active Plasma session.  This deliberately
// does not copy or edit plasma-org.kde.plasma.desktop-appletsrc: Plasma Shell
// remains the configuration authority and persists its own applet state.
class ControlCenterBackend final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(QVariantList tiles READ tiles NOTIFY changed)
    Q_PROPERTY(QString density READ density NOTIFY changed)
    Q_PROPERTY(QString summary READ summary NOTIFY changed)

public:
    explicit ControlCenterBackend(QObject *parent = nullptr);

    QVariantList tiles() const;
    QString density() const;
    QString summary() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void saveLayout(const QVariantList &tiles, const QString &density);
    Q_INVOKABLE void resetLayout();

    // Pure contract helpers.  Keeping the parser and serializer here lets the
    // applet schema and this Settings page share one stable wire format while
    // unit tests stay read-only and independent from the live Plasma session.
    static QStringList defaultTileIds();
    static QVariantMap normalizedLayout(const QString &order,
                                        const QString &sizes,
                                        const QString &visibility,
                                        const QString &density);
    static QVariantMap serializeLayout(const QVariantList &tiles,
                                       const QString &density,
                                       QString *error = nullptr);
    static QString readLayoutScript();
    static QString writeLayoutScript(const QString &order,
                                     const QString &sizes,
                                     const QString &visibility,
                                     const QString &density);

Q_SIGNALS:
    void changed();
    void layoutSaved();

private:
    void setLayout(const QVariantMap &layout);
    QString errorForScriptReason(const QString &reason) const;

    QVariantList m_tiles;
    QString m_density = QStringLiteral("comfortable");
};
