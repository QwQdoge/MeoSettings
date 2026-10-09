#include "audiobackend.h"

#include <PulseAudioQt/Context>
#include <PulseAudioQt/Server>
#include <PulseAudioQt/Sink>
#include <PulseAudioQt/Source>
#include <PulseAudioQt/SinkInput>
#include <PulseAudioQt/SourceOutput>
#include <PulseAudioQt/Card>
#include <PulseAudioQt/Profile>
#include <PulseAudioQt/Port>
#include <PulseAudioQt/StreamRestore>
#include <QAudioSink>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QBuffer>
#include <QTimer>
#include <cstring>

#include <QtMath>

AudioBackend::AudioBackend(QObject *parent)
    : BackendBase(parent)
    , m_context(PulseAudioQt::Context::instance())
{
    connect(m_context, &PulseAudioQt::Context::streamRestoreAdded, this, [this] { publishChanged(); });
    connect(m_context, &PulseAudioQt::Context::streamRestoreRemoved, this, [this] { publishChanged(); });
    connect(m_context, &PulseAudioQt::Context::stateChanged, this, [this] {
        bindDefaultSink();
        bindDefaultSource();
        publishChanged();
    });
    connect(m_context->server(), &PulseAudioQt::Server::defaultSinkChanged, this, [this] {
        bindDefaultSink();
        publishChanged();
    });
    connect(m_context->server(), &PulseAudioQt::Server::defaultSourceChanged, this, [this] {
        bindDefaultSource();
        publishChanged();
    });
    const auto refreshDevices = [this] { publishChanged(); };
    connect(m_context, &PulseAudioQt::Context::sinkAdded, this, refreshDevices);
    connect(m_context, &PulseAudioQt::Context::sinkRemoved, this, refreshDevices);
    connect(m_context, &PulseAudioQt::Context::sourceAdded, this, refreshDevices);
    connect(m_context, &PulseAudioQt::Context::sourceRemoved, this, refreshDevices);
    connect(m_context, &PulseAudioQt::Context::sinkInputAdded, this, refreshDevices);
    connect(m_context, &PulseAudioQt::Context::sinkInputRemoved, this, refreshDevices);
    connect(m_context, &PulseAudioQt::Context::sourceOutputAdded, this, refreshDevices);
    connect(m_context, &PulseAudioQt::Context::sourceOutputRemoved, this, refreshDevices);
    connect(m_context, &PulseAudioQt::Context::cardAdded, this, refreshDevices);
    connect(m_context, &PulseAudioQt::Context::cardRemoved, this, refreshDevices);
    bindDefaultSink();
    bindDefaultSource();
    publishChanged();
}

int AudioBackend::outputVolume() const
{
    return m_sink ? qRound(100.0 * m_sink->volume() / PulseAudioQt::normalVolume()) : 0;
}

bool AudioBackend::outputMuted() const
{
    return m_sink && m_sink->isMuted();
}

QString AudioBackend::outputName() const
{
    return m_sink ? m_sink->description() : QString();
}

QVariantList AudioBackend::outputs() const
{
    QVariantList result;
    if (!m_context) {
        return result;
    }
    const auto devices = m_context->sinks();
    result.reserve(devices.size());
    for (const auto *device : devices) {
        if (!device) {
            continue;
        }
        QVariantList ports, channels;
        for (int index = 0; index < device->ports().size(); ++index) {
            const auto *port = device->ports()[index];
            ports.append(QVariantMap{{"index", index}, {"name", port->description()}, {"active", uint(index) == device->activePortIndex()}, {"available", port->availability() != PulseAudioQt::Profile::Unavailable}});
        }
        for (int index = 0; index < device->channelVolumes().size(); ++index)
            channels.append(QVariantMap{{"index", index}, {"name", device->channels().value(index)}, {"volume", qRound(100.0 * device->channelVolumes()[index] / PulseAudioQt::normalVolume())}});
        result.push_back(QVariantMap{
            {"ports", ports}, {"channels", channels}, {"index", device->index()},
            {QStringLiteral("id"), device->name()},
            {QStringLiteral("name"), device->description()},
            {QStringLiteral("formFactor"), device->formFactor()},
            {QStringLiteral("active"), device == m_sink},
        });
    }
    return result;
}

bool AudioBackend::microphoneAvailable() const
{
    return m_source != nullptr;
}

int AudioBackend::inputVolume() const
{
    return m_source ? qRound(100.0 * m_source->volume() / PulseAudioQt::normalVolume()) : 0;
}

bool AudioBackend::inputMuted() const
{
    return m_source && m_source->isMuted();
}

QString AudioBackend::inputName() const
{
    return m_source ? m_source->description() : QString();
}

QVariantList AudioBackend::inputs() const
{
    QVariantList result;
    if (!m_context) {
        return result;
    }
    const auto devices = m_context->sources();
    result.reserve(devices.size());
    for (const auto *device : devices) {
        // Monitor sources represent an output mix, not a microphone.
        if (!device || device->name().endsWith(QStringLiteral(".monitor"))) {
            continue;
        }
        QVariantList ports, channels;
        for (int index = 0; index < device->ports().size(); ++index) {
            const auto *port = device->ports()[index];
            ports.append(QVariantMap{{"index", index}, {"name", port->description()}, {"active", uint(index) == device->activePortIndex()}, {"available", port->availability() != PulseAudioQt::Profile::Unavailable}});
        }
        for (int index = 0; index < device->channelVolumes().size(); ++index)
            channels.append(QVariantMap{{"index", index}, {"name", device->channels().value(index)}, {"volume", qRound(100.0 * device->channelVolumes()[index] / PulseAudioQt::normalVolume())}});
        result.push_back(QVariantMap{
            {"ports", ports}, {"channels", channels}, {"index", device->index()},
            {QStringLiteral("id"), device->name()},
            {QStringLiteral("name"), device->description()},
            {QStringLiteral("formFactor"), device->formFactor()},
            {QStringLiteral("active"), device == m_source},
        });
    }
    return result;
}

bool AudioBackend::pipeWire() const
{
    return m_context && m_context->server()->isPipeWire();
}

void AudioBackend::setOutputVolume(const int percent)
{
    if (m_sink) {
        m_sink->setVolume(qRound64(PulseAudioQt::normalVolume() * qBound(0, percent, 150) / 100.0));
    }
}

void AudioBackend::setOutputMuted(const bool muted)
{
    if (m_sink && m_sink->isMuted() != muted) {
        m_sink->setMuted(muted);
    }
}

void AudioBackend::setInputVolume(const int percent)
{
    if (m_source) {
        m_source->setVolume(qRound64(PulseAudioQt::normalVolume() * qBound(0, percent, 150) / 100.0));
    }
}

void AudioBackend::setInputMuted(const bool muted)
{
    if (m_source && m_source->isMuted() != muted) {
        m_source->setMuted(muted);
    }
}

void AudioBackend::setDefaultOutput(const QString &id)
{
    clearError();
    for (auto *device : m_context->sinks()) {
        if (device && device->name() == id) {
            m_context->server()->setDefaultSink(device);
            device->switchStreams();
            return;
        }
    }
    setError(tr("The selected audio output is no longer available."));
}

void AudioBackend::setDefaultInput(const QString &id)
{
    clearError();
    for (auto *device : m_context->sources()) {
        if (device && !device->name().endsWith(QStringLiteral(".monitor")) && device->name() == id) {
            m_context->server()->setDefaultSource(device);
            device->switchStreams();
            return;
        }
    }
    setError(tr("The selected microphone is no longer available."));
}

void AudioBackend::bindDefaultSink()
{
    if (m_sink) {
        disconnect(m_sink, nullptr, this, nullptr);
    }
    m_sink = m_context->server()->defaultSink();
    if (!m_sink) {
        return;
    }
    connect(m_sink, &PulseAudioQt::Sink::volumeChanged, this, &AudioBackend::publishChanged);
    connect(m_sink, &PulseAudioQt::Sink::mutedChanged, this, &AudioBackend::publishChanged);
    connect(m_sink, &PulseAudioQt::Sink::descriptionChanged, this, &AudioBackend::publishChanged);
}

void AudioBackend::bindDefaultSource()
{
    if (m_source) {
        disconnect(m_source, nullptr, this, nullptr);
    }
    const auto candidate = m_context->server()->defaultSource();
    // A monitor source is an output loopback, not an input microphone. Keep
    // the selected default consistent with the public microphone list.
    m_source = candidate && !candidate->name().endsWith(QStringLiteral(".monitor")) ? candidate : nullptr;
    if (!m_source) {
        return;
    }
    connect(m_source, &PulseAudioQt::Source::volumeChanged, this, &AudioBackend::publishChanged);
    connect(m_source, &PulseAudioQt::Source::mutedChanged, this, &AudioBackend::publishChanged);
    connect(m_source, &PulseAudioQt::Source::descriptionChanged, this, &AudioBackend::publishChanged);
}

void AudioBackend::publishChanged()
{
    for (auto *object : m_context->sinks()) watchObject(object);
    for (auto *object : m_context->sources()) watchObject(object);
    for (auto *object : m_context->sinkInputs()) watchObject(object);
    for (auto *object : m_context->sourceOutputs()) watchObject(object);
    for (auto *object : m_context->cards()) watchObject(object);
    for (auto *object : m_context->streamRestores()) watchObject(object);
    setAvailable(m_context && m_context->state() == PulseAudioQt::Context::State::Ready && m_sink);
    Q_EMIT changed();
}

void AudioBackend::watchObject(QObject *object)
{
    if (!object) return;
    // Ports and profiles can be replaced while the owning device stays alive.
    if (auto *device = qobject_cast<PulseAudioQt::Device *>(object))
        for (auto *port : device->ports()) watchObject(port);
    if (auto *card = qobject_cast<PulseAudioQt::Card *>(object))
        for (auto *profile : card->profiles()) watchObject(profile);
    if (m_watchers.contains(object)) return;
    auto *watcher = new QObject(this); m_watchers.insert(object, watcher);
    connect(object, &QObject::destroyed, this, [this, object, watcher] { m_watchers.remove(object); watcher->deleteLater(); });
    const auto update = [this] { publishChanged(); };
    if (auto *volume = qobject_cast<PulseAudioQt::VolumeObject *>(object)) {
        connect(volume, &PulseAudioQt::VolumeObject::volumeChanged, watcher, update);
        connect(volume, &PulseAudioQt::VolumeObject::mutedChanged, watcher, update);
        connect(volume, &PulseAudioQt::VolumeObject::channelVolumesChanged, watcher, update);
        connect(volume, &PulseAudioQt::VolumeObject::channelsChanged, watcher, update);
    }
    if (auto *device = qobject_cast<PulseAudioQt::Device *>(object)) {
        connect(device, &PulseAudioQt::Device::activePortIndexChanged, watcher, update);
        connect(device, &PulseAudioQt::Device::portsChanged, watcher, update);
    }
    if (auto *stream = qobject_cast<PulseAudioQt::Stream *>(object)) {
        connect(stream, &PulseAudioQt::Stream::deviceIndexChanged, watcher, update);
        connect(stream, &PulseAudioQt::Stream::hasVolumeChanged, watcher, update);
    }
    if (auto *card = qobject_cast<PulseAudioQt::Card *>(object)) {
        connect(card, &PulseAudioQt::Card::activeProfileIndexChanged, watcher, update);
        connect(card, &PulseAudioQt::Card::profilesChanged, watcher, update);
    }
    if (auto *event = qobject_cast<PulseAudioQt::StreamRestore *>(object)) {
        connect(event, &PulseAudioQt::StreamRestore::volumeChanged, watcher, update);
        connect(event, &PulseAudioQt::StreamRestore::mutedChanged, watcher, update);
    }
    if (auto *profile = qobject_cast<PulseAudioQt::Profile *>(object))
        connect(profile, &PulseAudioQt::Profile::availabilityChanged, watcher, update);
}

namespace {
PulseAudioQt::Stream *findStream(PulseAudioQt::Context *context, const QString &id)
{
    for (auto *stream : context->sinkInputs()) if (id == QStringLiteral("playback:") + QString::number(stream->index())) return stream;
    for (auto *stream : context->sourceOutputs()) if (id == QStringLiteral("recording:") + QString::number(stream->index())) return stream;
    return nullptr;
}
PulseAudioQt::Device *findAudioDevice(PulseAudioQt::Context *context, const QString &id, bool input)
{
    if (input) { for (auto *device : context->sources()) if (device->name() == id && !id.endsWith(QStringLiteral(".monitor"))) return device; }
    else { for (auto *device : context->sinks()) if (device->name() == id) return device; }
    return nullptr;
}
}

QVariantList AudioBackend::streams() const
{
    QVariantList result;
    const auto append = [&result](PulseAudioQt::Stream *stream, bool input) {
        const auto properties = stream->properties();
        const QString application = properties.value(QStringLiteral("application.name")).toString();
        result.append(QVariantMap{{"id", (input ? QStringLiteral("recording:") : QStringLiteral("playback:")) + QString::number(stream->index())},
            {"name", application.isEmpty() ? stream->name() : application}, {"input", input},
            {"volume", qRound(100.0 * stream->volume() / PulseAudioQt::normalVolume())}, {"muted", stream->isMuted()},
            {"hasVolume", stream->hasVolume() && stream->isVolumeWritable()}, {"deviceIndex", stream->deviceIndex()}});
    };
    for (auto *stream : m_context->sinkInputs()) if (!stream->isVirtualStream()) append(stream, false);
    for (auto *stream : m_context->sourceOutputs()) if (!stream->isVirtualStream()) append(stream, true);
    return result;
}

QVariantList AudioBackend::cards() const
{
    QVariantList result;
    for (auto *card : m_context->cards()) {
        QVariantList profiles;
        for (int index = 0; index < card->profiles().size(); ++index) {
            const auto *profile = card->profiles()[index];
            profiles.append(QVariantMap{{"index", index}, {"label", profile->description()}, {"available", profile->availability() != PulseAudioQt::Profile::Unavailable}});
        }
        result.append(QVariantMap{{"id", card->name()}, {"name", card->properties().value("device.description", card->name())}, {"profiles", profiles}, {"activeProfile", card->activeProfileIndex()}});
    }
    return result;
}

void AudioBackend::setStreamVolume(const QString &id, int percent)
{
    auto *stream = findStream(m_context, id);
    if (!stream || !stream->hasVolume() || !stream->isVolumeWritable() || percent < 0 || percent > 100) { setError(tr("The audio stream or volume is unavailable.")); return; }
    clearError(); stream->setVolume(qRound64(PulseAudioQt::normalVolume() * percent / 100.0));
}
void AudioBackend::setStreamMuted(const QString &id, bool muted)
{
    auto *stream = findStream(m_context, id);
    if (!stream) { setError(tr("This audio stream is no longer available.")); return; }
    clearError(); stream->setMuted(muted);
}
void AudioBackend::setStreamDevice(const QString &id, const QString &deviceId)
{
    auto *stream = findStream(m_context, id);
    auto *device = findAudioDevice(m_context, deviceId, id.startsWith(QStringLiteral("recording:")));
    if (!stream || !device) { setError(tr("The stream or device is no longer available.")); return; }
    clearError(); stream->setDeviceIndex(device->index());
}
void AudioBackend::setCardProfile(const QString &id, int index)
{
    for (auto *card : m_context->cards()) if (card->name() == id && index >= 0 && index < card->profiles().size()
        && card->profiles()[index]->availability() != PulseAudioQt::Profile::Unavailable) {
        clearError(); card->setActiveProfileIndex(uint(index)); return;
    }
    setError(tr("This audio profile is unavailable."));
}
void AudioBackend::setDevicePort(const QString &id, bool input, int port)
{
    auto *device = findAudioDevice(m_context, id, input);
    if (!device || port < 0 || port >= device->ports().size() || device->ports()[port]->availability() == PulseAudioQt::Profile::Unavailable) {
        setError(tr("This audio port is unavailable.")); return;
    }
    clearError(); device->setActivePortIndex(uint(port));
}
void AudioBackend::setChannelVolume(const QString &id, bool input, int channel, int percent)
{
    auto *device = findAudioDevice(m_context, id, input);
    if (!device || channel < 0 || channel >= device->channelVolumes().size() || percent < 0 || percent > 100) { setError(tr("This audio channel is unavailable.")); return; }
    clearError(); device->setChannelVolume(channel, qRound64(PulseAudioQt::normalVolume() * percent / 100.0));
}

QVariantMap AudioBackend::notificationSound() const
{
    for (const auto *stream : m_context->streamRestores()) {
        if (stream->name() == QStringLiteral("sink-input-by-media-role:event"))
            return {{"volume", qRound(100.0 * stream->volume() / PulseAudioQt::normalVolume())},
                {"muted", stream->isMuted()}, {"writable", stream->hasVolume() && stream->isVolumeWritable()}};
    }
    return {};
}
void AudioBackend::setNotificationSound(int volume, bool muted)
{
    if (volume < 0 || volume > 100) { setError(tr("Choose a volume between 0 and 100 percent.")); return; }
    for (auto *stream : m_context->streamRestores()) {
        if (stream->name() == QStringLiteral("sink-input-by-media-role:event") && stream->hasVolume() && stream->isVolumeWritable()) {
            clearError(); stream->setVolume(qRound64(PulseAudioQt::normalVolume() * volume / 100.0)); stream->setMuted(muted); return;
        }
    }
    setError(tr("The audio service does not expose a notification sound volume."));
}
void AudioBackend::stopTestSound()
{
    if (!m_testAudio) return;
    auto *audio = m_testAudio.data(); m_testAudio.clear();
    disconnect(audio, nullptr, this, nullptr); audio->stop(); audio->deleteLater(); Q_EMIT changed();
}
void AudioBackend::testSound()
{
    stopTestSound();
    const auto device = QMediaDevices::defaultAudioOutput();
    if (!available() || device.isNull()) { setError(tr("No default audio output is available.")); return; }
    const auto format = device.preferredFormat();
    if (!format.isValid() || format.sampleRate() > 384000 || format.channelCount() > 32) {
        setError(tr("The audio device does not expose a supported test format.")); return;
    }
    // A short, gently faded tone, at a bounded volume, through Qt's real audio
    // output. It never overrides the user's master volume or mute state.
    const int frames = format.sampleRate();
    QByteArray bytes(format.bytesForFrames(frames), '\0');
    for (int frame = 0; frame < frames; ++frame) {
        const double t = double(frame) / format.sampleRate();
        const double envelope = qMin(1.0, qMin(t / 0.03, (1.0 - t) / 0.05));
        const double sample = 0.15 * qSin(2.0 * M_PI * 440.0 * t) * envelope;
        for (int channel = 0; channel < format.channelCount(); ++channel) {
            char *target = bytes.data() + (frame * format.channelCount() + channel) * format.bytesPerSample();
            switch (format.sampleFormat()) {
            case QAudioFormat::UInt8: { const quint8 value = quint8(qRound(128.0 + sample * 127.0)); std::memcpy(target, &value, sizeof value); break; }
            case QAudioFormat::Int16: { const qint16 value = qint16(qRound(sample * 32767.0)); std::memcpy(target, &value, sizeof value); break; }
            case QAudioFormat::Int32: { const qint32 value = qint32(sample * 2147483647.0); std::memcpy(target, &value, sizeof value); break; }
            case QAudioFormat::Float: { const float value = float(sample); std::memcpy(target, &value, sizeof value); break; }
            default: setError(tr("The audio test sample format is unsupported.")); return;
            }
        }
    }
    clearError(); auto *audio = new QAudioSink(device, format, this); m_testAudio = audio;
    auto *buffer = new QBuffer(audio); buffer->setData(bytes); buffer->open(QIODevice::ReadOnly);
    connect(audio, &QAudioSink::stateChanged, this, [this, audio](QAudio::State state) {
        if (m_testAudio != audio) return;
        if (state == QAudio::IdleState || state == QAudio::StoppedState) {
            if (audio->error() != QAudio::NoError) setError(tr("The audio output could not play the test sound."));
            stopTestSound();
        }
    });
    audio->start(buffer); Q_EMIT changed();
    QTimer::singleShot(2500, audio, [this, audio] { if (m_testAudio == audio) stopTestSound(); });
}
