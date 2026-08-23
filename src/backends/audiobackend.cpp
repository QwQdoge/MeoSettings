#include "audiobackend.h"

#include <PulseAudioQt/Context>
#include <PulseAudioQt/Server>
#include <PulseAudioQt/Sink>
#include <PulseAudioQt/Source>

#include <QtMath>

AudioBackend::AudioBackend(QObject *parent)
    : BackendBase(parent)
    , m_context(PulseAudioQt::Context::instance())
{
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
        result.push_back(QVariantMap{
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
        result.push_back(QVariantMap{
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
    setAvailable(m_context && m_context->state() == PulseAudioQt::Context::State::Ready && m_sink);
    Q_EMIT changed();
}
