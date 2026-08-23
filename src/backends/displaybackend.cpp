#include "displaybackend.h"

#include <KScreen/Config>
#include <KScreen/ConfigMonitor>
#include <KScreen/GetConfigOperation>
#include <KScreen/Mode>
#include <KScreen/Output>

#include <QPointer>

DisplayBackend::DisplayBackend(QObject *parent)
    : BackendBase(parent)
{
    connect(KScreen::ConfigMonitor::instance(), &KScreen::ConfigMonitor::configurationChanged,
            this, &DisplayBackend::refresh);
    refresh();
}

QVariantList DisplayBackend::outputs() const
{
    return m_outputs;
}

QString DisplayBackend::summary() const
{
    if (m_outputs.isEmpty()) {
        return available() ? tr("No connected displays") : tr("Display service unavailable");
    }
    return tr("%n connected display(s)", "", m_outputs.size());
}

void DisplayBackend::refresh()
{
    if (busy()) {
        return;
    }
    clearError();
    setBusy(true);
    auto *operation = new KScreen::GetConfigOperation(KScreen::ConfigOperation::NoEDID, this);
    connect(operation, &KScreen::ConfigOperation::finished, this,
            [this, operation](KScreen::ConfigOperation *finished) {
                setBusy(false);
                if (finished->hasError() || !finished->config()) {
                    setAvailable(false);
                    setError(finished->hasError() ? finished->errorString() : tr("No display configuration was returned."));
                    Q_EMIT changed();
                    operation->deleteLater();
                    return;
                }

                QVariantList nextOutputs;
                const auto configuration = finished->config();
                const auto configuredOutputs = configuration->outputs();
                const auto primaryOutput = configuration->primaryOutput();
                nextOutputs.reserve(configuredOutputs.size());
                for (const auto &output : configuredOutputs) {
                    if (!output || !output->isConnected()) {
                        continue;
                    }
                    const auto mode = output->currentMode();
                    const auto modeSize = mode ? mode->size() : QSize{};
                    const QString label = !output->model().isEmpty()
                        ? output->model()
                        : (!output->name().isEmpty() ? output->name() : tr("Display"));
                    nextOutputs.push_back(QVariantMap{
                        {QStringLiteral("id"), output->id()},
                        {QStringLiteral("name"), label},
                        {QStringLiteral("connector"), output->name()},
                        {QStringLiteral("vendor"), output->vendor()},
                        {QStringLiteral("enabled"), output->isEnabled()},
                        {QStringLiteral("primary"), primaryOutput && output->id() == primaryOutput->id()},
                        {QStringLiteral("width"), modeSize.width()},
                        {QStringLiteral("height"), modeSize.height()},
                        {QStringLiteral("refreshRate"), mode ? mode->refreshRate() : 0.0},
                        {QStringLiteral("scale"), output->scale()},
                    });
                }
                m_outputs = nextOutputs;
                setAvailable(true);
                Q_EMIT changed();
                operation->deleteLater();
            });
}
