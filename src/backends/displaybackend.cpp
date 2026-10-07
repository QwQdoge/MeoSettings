#include "displaybackend.h"

#include <KScreen/ConfigMonitor>
#include <KScreen/GetConfigOperation>
#include <KScreen/Mode>
#include <KScreen/SetConfigOperation>

#include <QtMath>

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

                    QVariantList modes;
                    const auto availableModes = output->modes();
                    modes.reserve(availableModes.size());
                    for (const auto &availableMode : availableModes) {
                        if (!availableMode) {
                            continue;
                        }
                        const auto size = availableMode->size();
                        modes.push_back(QVariantMap{
                            {QStringLiteral("id"), availableMode->id()},
                            {QStringLiteral("width"), size.width()},
                            {QStringLiteral("height"), size.height()},
                            {QStringLiteral("refreshRate"), availableMode->refreshRate()},
                            {QStringLiteral("current"), availableMode->id() == output->currentModeId()},
                            {QStringLiteral("preferred"), output->preferredModes().contains(availableMode->id())},
                        });
                    }

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
                        {QStringLiteral("currentModeId"), output->currentModeId()},
                        {QStringLiteral("modes"), modes},
                    });
                }
                m_outputs = nextOutputs;
                setAvailable(true);
                Q_EMIT changed();
                operation->deleteLater();
            });
}

void DisplayBackend::setScale(const int outputId, const qreal scale)
{
    if (!qIsFinite(scale) || scale < 0.5 || scale > 4.0) {
        setError(tr("Display scale must be between 0.5× and 4.0×."));
        return;
    }

    const qreal normalized = qRound(scale * 20.0) / 20.0;
    mutateOutput(outputId,
                 [normalized](const KScreen::ConfigPtr &, const KScreen::OutputPtr &output, QString *) {
                     output->setScale(normalized);
                     return true;
                 });
}

void DisplayBackend::setMode(const int outputId, const QString &modeId)
{
    if (modeId.trimmed().isEmpty()) {
        setError(tr("Choose a valid display mode."));
        return;
    }

    mutateOutput(outputId,
                 [modeId](const KScreen::ConfigPtr &, const KScreen::OutputPtr &output, QString *error) {
                     const auto mode = output->mode(modeId);
                     if (!mode) {
                         if (error) {
                             *error = DisplayBackend::tr("The selected display mode is no longer available.");
                         }
                         return false;
                     }
                     output->setCurrentModeId(mode->id());
                     return true;
                 });
}

void DisplayBackend::setPrimary(const int outputId)
{
    mutateOutput(outputId,
                 [](const KScreen::ConfigPtr &config, const KScreen::OutputPtr &output, QString *error) {
                     if (!output->isEnabled()) {
                         if (error) {
                             *error = DisplayBackend::tr("Enable this display before making it primary.");
                         }
                         return false;
                     }
                     config->setOutputPriority(output, 1);
                     return true;
                 });
}

void DisplayBackend::mutateOutput(const int outputId, const OutputMutation &mutation)
{
    if (busy()) {
        setError(tr("Another display change is still being applied."));
        return;
    }

    clearError();
    setBusy(true);

    auto *getOperation = new KScreen::GetConfigOperation(KScreen::ConfigOperation::NoEDID, this);
    connect(getOperation, &KScreen::ConfigOperation::finished, this,
            [this, getOperation, outputId, mutation](KScreen::ConfigOperation *finished) {
                if (finished->hasError() || !finished->config()) {
                    failMutation(finished->hasError()
                                     ? finished->errorString()
                                     : tr("No display configuration was returned."));
                    getOperation->deleteLater();
                    return;
                }

                const auto config = finished->config();
                const auto output = config->output(outputId);
                if (!output || !output->isConnected()) {
                    failMutation(tr("The selected display is no longer connected."));
                    getOperation->deleteLater();
                    return;
                }

                QString mutationError;
                if (!mutation(config, output, &mutationError)) {
                    failMutation(mutationError.isEmpty()
                                     ? tr("The requested display change is not available.")
                                     : mutationError);
                    getOperation->deleteLater();
                    return;
                }

                if (!KScreen::Config::canBeApplied(
                        config,
                        KScreen::Config::ValidityFlag::RequireAtLeastOneEnabledScreen)) {
                    failMutation(tr("This display configuration cannot be applied safely."));
                    getOperation->deleteLater();
                    return;
                }

                auto *setOperation = new KScreen::SetConfigOperation(config, this);
                connect(setOperation, &KScreen::ConfigOperation::finished, this,
                        [this, setOperation](KScreen::ConfigOperation *applied) {
                            setBusy(false);
                            if (applied->hasError()) {
                                setError(applied->errorString());
                                Q_EMIT changed();
                            } else {
                                refresh();
                            }
                            setOperation->deleteLater();
                        });
                getOperation->deleteLater();
            });
}

void DisplayBackend::failMutation(const QString &message)
{
    setBusy(false);
    setError(message);
    Q_EMIT changed();
}
