#include "powerbackend.h"

#include <Solid/Battery>
#include <Solid/DeviceNotifier>

#include <algorithm>

namespace
{
constexpr qint64 UnknownDuration = -1;
}

PowerBackend::PowerBackend(QObject *parent)
    : BackendBase(parent)
{
    auto *notifier = Solid::DeviceNotifier::instance();
    connect(notifier, &Solid::DeviceNotifier::deviceAdded, this,
            [this](const QString &) { refresh(); });
    connect(notifier, &Solid::DeviceNotifier::deviceRemoved, this,
            [this](const QString &) { refresh(); });
    refresh();
}

int PowerBackend::percent() const
{
    if (!m_primaryBattery || !m_primaryBattery->isPresent()) {
        return -1;
    }

    const int reported = m_primaryBattery->chargePercent();
    return reported >= 0 && reported <= 100 ? reported : -1;
}

bool PowerBackend::percentKnown() const
{
    return percent() >= 0;
}

bool PowerBackend::charging() const
{
    return m_primaryBattery && m_primaryBattery->isPresent()
        && m_primaryBattery->chargeState() == Solid::Battery::Charging;
}

QString PowerBackend::state() const
{
    if (!m_primaryBattery || !m_primaryBattery->isPresent()) {
        return QStringLiteral("unavailable");
    }

    switch (m_primaryBattery->chargeState()) {
    case Solid::Battery::Charging:
        return QStringLiteral("charging");
    case Solid::Battery::Discharging:
        return QStringLiteral("discharging");
    case Solid::Battery::FullyCharged:
        return QStringLiteral("fully-charged");
    case Solid::Battery::NoCharge:
    default:
        return QStringLiteral("not-charging");
    }
}

QString PowerBackend::stateLabel() const
{
    const QString currentState = state();
    if (currentState == QStringLiteral("charging")) {
        return tr("Charging");
    }
    if (currentState == QStringLiteral("discharging")) {
        return tr("On battery");
    }
    if (currentState == QStringLiteral("fully-charged")) {
        return tr("Fully charged");
    }
    if (currentState == QStringLiteral("not-charging")) {
        return tr("Not charging");
    }
    return tr("Battery unavailable");
}

qint64 PowerBackend::timeRemaining() const
{
    if (!m_primaryBattery || !m_primaryBattery->isPresent()) {
        return UnknownDuration;
    }

    const qint64 overall = m_primaryBattery->remainingTime();
    if (overall >= 0) {
        return overall;
    }

    switch (m_primaryBattery->chargeState()) {
    case Solid::Battery::Charging:
        return std::max(UnknownDuration, m_primaryBattery->timeToFull());
    case Solid::Battery::Discharging:
        return std::max(UnknownDuration, m_primaryBattery->timeToEmpty());
    default:
        return UnknownDuration;
    }
}

bool PowerBackend::timeRemainingKnown() const
{
    return timeRemaining() >= 0;
}

QString PowerBackend::summary() const
{
    if (!available()) {
        return tr("No primary battery detected");
    }

    const QString amount = percentKnown() ? tr("%1%").arg(percent())
                                           : tr("Charge percentage unavailable");
    const qint64 remaining = timeRemaining();
    if (state() == QStringLiteral("charging")) {
        return remaining >= 0
            ? tr("%1 · Charging, %2 until full").arg(amount, formattedDuration(remaining))
            : tr("%1 · Charging").arg(amount);
    }
    if (state() == QStringLiteral("discharging")) {
        return remaining >= 0
            ? tr("%1 · %2 remaining").arg(amount, formattedDuration(remaining))
            : tr("%1 · On battery").arg(amount);
    }
    return tr("%1 · %2").arg(amount, stateLabel());
}

void PowerBackend::refresh()
{
    Solid::Device nextDevice;
    const auto devices = Solid::Device::listFromType(Solid::DeviceInterface::Battery);
    for (const auto &device : devices) {
        const auto *candidate = device.as<const Solid::Battery>();
        if (candidate && candidate->type() == Solid::Battery::PrimaryBattery && candidate->isPresent()) {
            nextDevice = device;
            break;
        }
    }

    const bool deviceChanged = m_primaryBatteryDevice.udi() != nextDevice.udi();
    if (deviceChanged) {
        if (m_primaryBattery) {
            disconnect(m_primaryBattery, nullptr, this, nullptr);
        }
        m_primaryBatteryDevice = nextDevice;
        m_primaryBattery = m_primaryBatteryDevice.as<Solid::Battery>();
        bindPrimaryBattery();
    }

    setAvailable(m_primaryBattery && m_primaryBattery->isPresent());
    publishChanged();
}

void PowerBackend::bindPrimaryBattery()
{
    if (!m_primaryBattery) {
        return;
    }

    const auto publish = [this] { publishChanged(); };
    connect(m_primaryBattery, &Solid::Battery::chargePercentChanged, this, publish);
    connect(m_primaryBattery, &Solid::Battery::chargeStateChanged, this, publish);
    connect(m_primaryBattery, &Solid::Battery::timeToEmptyChanged, this, publish);
    connect(m_primaryBattery, &Solid::Battery::timeToFullChanged, this, publish);
    connect(m_primaryBattery, &Solid::Battery::remainingTimeChanged, this, publish);
    connect(m_primaryBattery, &Solid::Battery::presentStateChanged, this,
            [this](bool, const QString &) { refresh(); });
}

void PowerBackend::publishChanged()
{
    Q_EMIT changed();
}

QString PowerBackend::formattedDuration(const qint64 seconds) const
{
    const qint64 totalMinutes = std::max<qint64>(1, seconds / 60);
    const qint64 hours = totalMinutes / 60;
    const qint64 minutes = totalMinutes % 60;
    if (hours > 0 && minutes > 0) {
        return tr("%1 h %2 min").arg(hours).arg(minutes);
    }
    if (hours > 0) {
        return tr("%1 h").arg(hours);
    }
    return tr("%1 min").arg(minutes);
}
