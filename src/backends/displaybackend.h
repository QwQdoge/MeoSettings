#pragma once

#include "../core/backendbase.h"

#include <KScreen/Config>
#include <KScreen/Output>

#include <QTimer>
#include <QVariantList>

#include <functional>

class DisplayBackend final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(QVariantList outputs READ outputs NOTIFY changed)
    Q_PROPERTY(QString summary READ summary NOTIFY changed)
    Q_PROPERTY(bool modeConfirmationPending READ modeConfirmationPending NOTIFY modeConfirmationChanged)
    Q_PROPERTY(int modeConfirmationSecondsRemaining READ modeConfirmationSecondsRemaining NOTIFY modeConfirmationChanged)
    Q_PROPERTY(QString modeConfirmationLabel READ modeConfirmationLabel NOTIFY modeConfirmationChanged)

public:
    explicit DisplayBackend(QObject *parent = nullptr);

    QVariantList outputs() const;
    QString summary() const;
    bool modeConfirmationPending() const;
    int modeConfirmationSecondsRemaining() const;
    QString modeConfirmationLabel() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setScale(int outputId, qreal scale);
    Q_INVOKABLE void setMode(int outputId, const QString &modeId);
    Q_INVOKABLE void setPrimary(int outputId);
    Q_INVOKABLE void confirmModeChange();
    Q_INVOKABLE void revertModeChange();

Q_SIGNALS:
    void changed();
    void modeConfirmationChanged();

private:
    using OutputMutation = std::function<bool(const KScreen::ConfigPtr &,
                                              const KScreen::OutputPtr &,
                                              QString *)>;
    using MutationCompletion = std::function<void(bool)>;

    void mutateOutput(int outputId,
                      const OutputMutation &mutation,
                      const MutationCompletion &completion = {},
                      bool allowDuringModeConfirmation = false);
    void failMutation(const QString &message);
    void beginModeConfirmation(int outputId,
                               const QString &previousModeId,
                               const QString &targetLabel);
    void clearModeConfirmation();

    QVariantList m_outputs;
    QTimer m_modeConfirmationTimer;
    bool m_modeConfirmationPending = false;
    int m_modeConfirmationOutputId = -1;
    int m_modeConfirmationSecondsRemaining = 0;
    QString m_modeConfirmationPreviousModeId;
    QString m_modeConfirmationLabel;
};
