#pragma once

#include "../core/backendbase.h"

#include <Solid/Device>

namespace Solid
{
class Battery;
}

/**
 * A factual view of the system's primary battery.
 *
 * This deliberately exposes no power-profile, suspend, charge-limit, or
 * other mutating controls. Those continue to belong to KDE's maintained
 * PowerDevil configuration module. The backend only consumes Solid's primary
 * battery interface so the home surface never invents battery state.
 */
class PowerBackend final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(int percent READ percent NOTIFY changed)
    Q_PROPERTY(bool percentKnown READ percentKnown NOTIFY changed)
    Q_PROPERTY(bool charging READ charging NOTIFY changed)
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(QString stateLabel READ stateLabel NOTIFY changed)
    Q_PROPERTY(qint64 timeRemaining READ timeRemaining NOTIFY changed)
    Q_PROPERTY(bool timeRemainingKnown READ timeRemainingKnown NOTIFY changed)
    Q_PROPERTY(QString summary READ summary NOTIFY changed)

public:
    explicit PowerBackend(QObject *parent = nullptr);

    /// -1 means the system reported no usable percentage.
    int percent() const;
    bool percentKnown() const;
    bool charging() const;

    /// A stable, non-localized state: unavailable, charging, discharging,
    /// fully-charged, or not-charging.
    QString state() const;
    QString stateLabel() const;

    /// Seconds until full/empty when Solid can estimate it, otherwise -1.
    qint64 timeRemaining() const;
    bool timeRemainingKnown() const;
    QString summary() const;

    /// Re-discovers the primary battery. This performs no system mutation.
    Q_INVOKABLE void refresh();

Q_SIGNALS:
    void changed();

private:
    void bindPrimaryBattery();
    void publishChanged();
    QString formattedDuration(qint64 seconds) const;

    Solid::Device m_primaryBatteryDevice;
    Solid::Battery *m_primaryBattery = nullptr;
};
