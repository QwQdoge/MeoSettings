#include "capabilitymanager.h"

#include "../backends/audiobackend.h"
#include "../backends/bluetoothbackend.h"
#include "../backends/displaybackend.h"
#include "../backends/networkbackend.h"
#include "../backends/powerbackend.h"

CapabilityManager::CapabilityManager(NetworkBackend *networkBackend,
                                     BluetoothBackend *bluetoothBackend,
                                     AudioBackend *audioBackend,
                                     DisplayBackend *displayBackend,
                                     PowerBackend *powerBackend,
                                     QObject *parent)
    : QObject(parent)
    , m_networkBackend(networkBackend)
    , m_bluetoothBackend(bluetoothBackend)
    , m_audioBackend(audioBackend)
    , m_displayBackend(displayBackend)
    , m_powerBackend(powerBackend)
{
    const auto publishChanged = [this] { Q_EMIT changed(); };
    connect(m_networkBackend, &NetworkBackend::changed, this, publishChanged);
    connect(m_networkBackend, &BackendBase::availableChanged, this, publishChanged);
    connect(m_bluetoothBackend, &BluetoothBackend::changed, this, publishChanged);
    connect(m_bluetoothBackend, &BackendBase::availableChanged, this, publishChanged);
    connect(m_audioBackend, &AudioBackend::changed, this, publishChanged);
    connect(m_audioBackend, &BackendBase::availableChanged, this, publishChanged);
    connect(m_displayBackend, &DisplayBackend::changed, this, publishChanged);
    connect(m_displayBackend, &BackendBase::availableChanged, this, publishChanged);
    connect(m_powerBackend, &PowerBackend::changed, this, publishChanged);
    connect(m_powerBackend, &BackendBase::availableChanged, this, publishChanged);
}

bool CapabilityManager::network() const
{
    return m_networkBackend->available();
}

bool CapabilityManager::wifi() const
{
    return m_networkBackend->wifiAvailable();
}

bool CapabilityManager::bluetooth() const
{
    return m_bluetoothBackend->available();
}

bool CapabilityManager::audio() const
{
    return m_audioBackend->available();
}

bool CapabilityManager::display() const
{
    return m_displayBackend->available();
}

bool CapabilityManager::battery() const
{
    return m_powerBackend->available();
}
