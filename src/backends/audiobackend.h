#pragma once

#include "../core/backendbase.h"

#include <QVariantList>
#include <QHash>
#include <QPointer>
class QAudioSink;

namespace PulseAudioQt
{
class Context;
class Sink;
class Source;
}

class AudioBackend final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(QVariantList streams READ streams NOTIFY changed)
    Q_PROPERTY(QVariantList cards READ cards NOTIFY changed)
    Q_PROPERTY(QVariantMap notificationSound READ notificationSound NOTIFY changed)
    Q_PROPERTY(bool testingSound READ testingSound NOTIFY changed)
    Q_PROPERTY(int outputVolume READ outputVolume WRITE setOutputVolume NOTIFY changed)
    Q_PROPERTY(bool outputMuted READ outputMuted WRITE setOutputMuted NOTIFY changed)
    Q_PROPERTY(QString outputName READ outputName NOTIFY changed)
    Q_PROPERTY(QVariantList outputs READ outputs NOTIFY changed)
    Q_PROPERTY(bool microphoneAvailable READ microphoneAvailable NOTIFY changed)
    Q_PROPERTY(int inputVolume READ inputVolume WRITE setInputVolume NOTIFY changed)
    Q_PROPERTY(bool inputMuted READ inputMuted WRITE setInputMuted NOTIFY changed)
    Q_PROPERTY(QString inputName READ inputName NOTIFY changed)
    Q_PROPERTY(QVariantList inputs READ inputs NOTIFY changed)
    Q_PROPERTY(bool pipeWire READ pipeWire NOTIFY changed)

public:
    explicit AudioBackend(QObject *parent = nullptr);

    QVariantMap notificationSound() const;
    bool testingSound() const { return !m_testAudio.isNull(); }
    Q_INVOKABLE void setNotificationSound(int volume, bool muted);
    Q_INVOKABLE void testSound();
    Q_INVOKABLE void stopTestSound();
    QVariantList streams() const;
    QVariantList cards() const;
    Q_INVOKABLE void setStreamVolume(const QString &id, int percent);
    Q_INVOKABLE void setStreamMuted(const QString &id, bool muted);
    Q_INVOKABLE void setStreamDevice(const QString &id, const QString &deviceId);
    Q_INVOKABLE void setCardProfile(const QString &id, int profile);
    Q_INVOKABLE void setDevicePort(const QString &id, bool input, int port);
    Q_INVOKABLE void setChannelVolume(const QString &id, bool input, int channel, int percent);
    int outputVolume() const;
    bool outputMuted() const;
    QString outputName() const;
    QVariantList outputs() const;
    bool microphoneAvailable() const;
    int inputVolume() const;
    bool inputMuted() const;
    QString inputName() const;
    QVariantList inputs() const;
    bool pipeWire() const;

    void setOutputVolume(int percent);
    void setOutputMuted(bool muted);
    void setInputVolume(int percent);
    void setInputMuted(bool muted);

    Q_INVOKABLE void setDefaultOutput(const QString &id);
    Q_INVOKABLE void setDefaultInput(const QString &id);

Q_SIGNALS:
    void changed();

private:
    void bindDefaultSink();
    void bindDefaultSource();
    void publishChanged();

    void watchObject(QObject *object);
    QHash<QObject *, QObject *> m_watchers;
    QPointer<QAudioSink> m_testAudio;
    PulseAudioQt::Context *m_context = nullptr;
    PulseAudioQt::Sink *m_sink = nullptr;
    PulseAudioQt::Source *m_source = nullptr;
};
