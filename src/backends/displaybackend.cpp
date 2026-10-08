#include "displaybackend.h"

#include <KScreen/ConfigMonitor>
#include <KScreen/GetConfigOperation>
#include <KScreen/Mode>
#include <KScreen/SetConfigOperation>

#include <QtMath>

#include <memory>

namespace {

QString displayModeLabel(const KScreen::ModePtr &mode)
{
    if (!mode) {
        return {};
    }

    const QSize size = mode->size();
    return DisplayBackend::tr("%1 × %2 · %3 Hz")
        .arg(size.width())
        .arg(size.height())
        .arg(QString::number(mode->refreshRate(), 'f', 1));
}

} // namespace

DisplayBackend::DisplayBackend(QObject *parent)
    : BackendBase(parent)
{
    connect(KScreen::ConfigMonitor::instance(), &KScreen::ConfigMonitor::configurationChanged,
            this, &DisplayBackend::refresh);

    m_modeConfirmationTimer.setInterval(1000);
    connect(&m_modeConfirmationTimer, &QTimer::timeout, this, [this] {
        if (!m_modeConfirmationPending || busy()) {
            return;
        }

        if (m_modeConfirmationSecondsRemaining > 1) {
            --m_modeConfirmationSecondsRemaining;
            Q_EMIT modeConfirmationChanged();
            return;
        }

        m_modeConfirmationSecondsRemaining = 0;
        Q_EMIT modeConfirmationChanged();
        revertModeChange();
    });

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

bool DisplayBackend::modeConfirmationPending() const
{
    return m_modeConfirmationPending;
}

int DisplayBackend::modeConfirmationSecondsRemaining() const
{
    return m_modeConfirmationSecondsRemaining;
}

QString DisplayBackend::modeConfirmationLabel() const
{
    return m_modeConfirmationLabel;
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
    if (m_modeConfirmationPending) {
        setError(tr("Keep or revert the current display mode before changing another display setting."));
        return;
    }

    auto previousModeId = std::make_shared<QString>();
    auto targetLabel = std::make_shared<QString>();

    mutateOutput(
        outputId,
        [modeId, previousModeId, targetLabel](const KScreen::ConfigPtr &,
                                              const KScreen::OutputPtr &output,
                                              QString *error) {
            const auto mode = output->mode(modeId);
            if (!mode) {
                if (error) {
                    *error = DisplayBackend::tr("The selected display mode is no longer available.");
                }
                return false;
            }

            if (output->currentModeId().isEmpty()) {
                if (error) {
                    *error = DisplayBackend::tr("The current display mode is unknown, so a safe rollback cannot be prepared.");
                }
                return false;
            }

            if (output->currentModeId() == mode->id()) {
                if (error) {
                    *error = DisplayBackend::tr("That display mode is already active.");
                }
                return false;
            }

            *previousModeId = output->currentModeId();
            *targetLabel = displayModeLabel(mode);
            output->setCurrentModeId(mode->id());
            return true;
        },
        [this, outputId, previousModeId, targetLabel](const bool success) {
            if (success) {
                beginModeConfirmation(outputId, *previousModeId, *targetLabel);
            }
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

void DisplayBackend::confirmModeChange()
{
    if (!m_modeConfirmationPending || busy()) {
        return;
    }

    clearModeConfirmation();
}

void DisplayBackend::revertModeChange()
{
    if (!m_modeConfirmationPending) {
        return;
    }
    if (busy()) {
        setError(tr("The display service is still refreshing. Try reverting again in a moment."));
        return;
    }

    const int outputId = m_modeConfirmationOutputId;
    const QString previousModeId = m_modeConfirmationPreviousModeId;

    mutateOutput(
        outputId,
        [previousModeId](const KScreen::ConfigPtr &,
                         const KScreen::OutputPtr &output,
                         QString *error) {
            const auto previousMode = output->mode(previousModeId);
            if (!previousMode) {
                if (error) {
                    *error = DisplayBackend::tr("The previous display mode is no longer available for automatic rollback.");
                }
                return false;
            }
            output->setCurrentModeId(previousMode->id());
            return true;
        },
        [this](const bool success) {
            if (success) {
                clearModeConfirmation();
                return;
            }

            // Do not loop an automatic rollback forever if the output was
            // disconnected or its old mode disappeared. Keep the confirmation
            // state visible so the user can retry or explicitly keep the mode.
            m_modeConfirmationTimer.stop();
            m_modeConfirmationSecondsRemaining = 0;
            Q_EMIT modeConfirmationChanged();
        },
        true);
}

void DisplayBackend::mutateOutput(const int outputId,
                                  const OutputMutation &mutation,
                                  const MutationCompletion &completion,
                                  const bool allowDuringModeConfirmation)
{
    if (m_modeConfirmationPending && !allowDuringModeConfirmation) {
        setError(tr("Keep or revert the current display mode before changing another display setting."));
        if (completion) {
            completion(false);
        }
        return;
    }
    if (busy()) {
        setError(tr("Another display change is still being applied."));
        if (completion) {
            completion(false);
        }
        return;
    }

    clearError();
    setBusy(true);

    auto *getOperation = new KScreen::GetConfigOperation(KScreen::ConfigOperation::NoEDID, this);
    connect(getOperation, &KScreen::ConfigOperation::finished, this,
            [this, getOperation, outputId, mutation, completion](KScreen::ConfigOperation *finished) {
                if (finished->hasError() || !finished->config()) {
                    failMutation(finished->hasError()
                                     ? finished->errorString()
                                     : tr("No display configuration was returned."));
                    if (completion) {
                        completion(false);
                    }
                    getOperation->deleteLater();
                    return;
                }

                const auto config = finished->config();
                const auto output = config->output(outputId);
                if (!output || !output->isConnected()) {
                    failMutation(tr("The selected display is no longer connected."));
                    if (completion) {
                        completion(false);
                    }
                    getOperation->deleteLater();
                    return;
                }

                QString mutationError;
                if (!mutation(config, output, &mutationError)) {
                    failMutation(mutationError.isEmpty()
                                     ? tr("The requested display change is not available.")
                                     : mutationError);
                    if (completion) {
                        completion(false);
                    }
                    getOperation->deleteLater();
                    return;
                }

                if (!KScreen::Config::canBeApplied(
                        config,
                        KScreen::Config::ValidityFlag::RequireAtLeastOneEnabledScreen)) {
                    failMutation(tr("This display configuration cannot be applied safely."));
                    if (completion) {
                        completion(false);
                    }
                    getOperation->deleteLater();
                    return;
                }

                auto *setOperation = new KScreen::SetConfigOperation(config, this);
                connect(setOperation, &KScreen::ConfigOperation::finished, this,
                        [this, setOperation, completion](KScreen::ConfigOperation *applied) {
                            setBusy(false);
                            if (applied->hasError()) {
                                setError(applied->errorString());
                                Q_EMIT changed();
                                if (completion) {
                                    completion(false);
                                }
                            } else {
                                if (completion) {
                                    completion(true);
                                }
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

void DisplayBackend::beginModeConfirmation(const int outputId,
                                           const QString &previousModeId,
                                           const QString &targetLabel)
{
    m_modeConfirmationPending = true;
    m_modeConfirmationOutputId = outputId;
    m_modeConfirmationPreviousModeId = previousModeId;
    m_modeConfirmationLabel = targetLabel;
    m_modeConfirmationSecondsRemaining = 15;
    m_modeConfirmationTimer.start();
    Q_EMIT modeConfirmationChanged();
}

void DisplayBackend::clearModeConfirmation()
{
    if (!m_modeConfirmationPending
        && m_modeConfirmationOutputId < 0
        && m_modeConfirmationPreviousModeId.isEmpty()
        && m_modeConfirmationLabel.isEmpty()) {
        return;
    }

    m_modeConfirmationTimer.stop();
    m_modeConfirmationPending = false;
    m_modeConfirmationOutputId = -1;
    m_modeConfirmationSecondsRemaining = 0;
    m_modeConfirmationPreviousModeId.clear();
    m_modeConfirmationLabel.clear();
    Q_EMIT modeConfirmationChanged();
}
