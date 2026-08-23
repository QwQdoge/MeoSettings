#pragma once

#include <QObject>

class NetworkBackend;
class BluetoothBackend;
class AudioBackend;
class DisplayBackend;
class PowerBackend;

class CapabilityManager final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool network READ network NOTIFY changed)
    Q_PROPERTY(bool wifi READ wifi NOTIFY changed)
    Q_PROPERTY(bool bluetooth READ bluetooth NOTIFY changed)
    Q_PROPERTY(bool audio READ audio NOTIFY changed)
    Q_PROPERTY(bool display READ display NOTIFY changed)
    Q_PROPERTY(bool battery READ battery NOTIFY changed)

public:
    explicit CapabilityManager(NetworkBackend *networkBackend,
                               BluetoothBackend *bluetoothBackend,
                               AudioBackend *audioBackend,
                               DisplayBackend *displayBackend,
                               PowerBackend *powerBackend,
                               QObject *parent = nullptr);

    bool network() const;
    bool wifi() const;
    bool bluetooth() const;
    bool audio() const;
    bool display() const;
    bool battery() const;

Q_SIGNALS:
    void changed();

private:
    NetworkBackend *m_networkBackend;
    BluetoothBackend *m_bluetoothBackend;
    AudioBackend *m_audioBackend;
    DisplayBackend *m_displayBackend;
    PowerBackend *m_powerBackend;
};
