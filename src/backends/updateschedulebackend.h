#pragma once

#include "../core/backendbase.h"

// How often OmniStore checks the Meo and OmniStore update sources in the
// background. The authority is OmniStore's systemd user timer
// `omnistore-update.timer` (packaged with OnUnitInactiveSec=6h). Meo Settings
// changes the interval with the drop-in
// ~/.config/systemd/user/omnistore-update.timer.d/interval.conf, which the
// OmniStore package already treats as user state, and turns automatic checks
// off or on by disabling or enabling the timer. Nothing is installed or
// upgraded by a check.
class UpdateScheduleBackend final : public BackendBase
{
    Q_OBJECT
    // 0 means automatic checks are off.
    Q_PROPERTY(int intervalHours READ intervalHours NOTIFY changed)
    Q_PROPERTY(QVariantList choices READ choices CONSTANT)

public:
    explicit UpdateScheduleBackend(QObject *parent = nullptr);

    int intervalHours() const { return m_intervalHours; }
    QVariantList choices() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setIntervalHours(int hours);

    static QString dropInPath();
    static int parseIntervalHours(const QString &dropIn, int fallback);

Q_SIGNALS:
    void changed();

private:
    bool callManager(const QString &method, const QVariantList &arguments, QString *error = nullptr) const;
    QString unitFileState() const;

    int m_intervalHours = 6;
};
