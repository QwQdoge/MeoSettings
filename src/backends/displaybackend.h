#pragma once

#include "../core/backendbase.h"

#include <QVariantList>
#include <QSocketNotifier>
#include <QTimer>

class DisplayBackend final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(QVariantList outputs READ outputs NOTIFY changed)
    Q_PROPERTY(QString summary READ summary NOTIFY changed)
    Q_PROPERTY(bool confirmationPending READ confirmationPending NOTIFY changed)
    Q_PROPERTY(int confirmationSeconds READ confirmationSeconds NOTIFY changed)
    Q_PROPERTY(bool modeConfirmationPending READ confirmationPending NOTIFY modeConfirmationChanged)
    Q_PROPERTY(int modeConfirmationSecondsRemaining READ confirmationSeconds NOTIFY modeConfirmationChanged)
    Q_PROPERTY(QString modeConfirmationLabel READ summary NOTIFY modeConfirmationChanged)

public:
    explicit DisplayBackend(QObject *parent = nullptr);
    ~DisplayBackend() override;

    QVariantList outputs() const;
    QString summary() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void applyOutput(int outputId, const QVariantMap &changes);
    Q_INVOKABLE void setScale(int outputId, qreal scale);
    Q_INVOKABLE void setMode(int outputId, const QString &modeId);
    Q_INVOKABLE void setPrimary(int outputId);
    Q_INVOKABLE void confirmModeChange() { keepChanges(); }
    Q_INVOKABLE void revertModeChange() { revertChanges(); }
    Q_INVOKABLE void keepChanges();
    Q_INVOKABLE void revertChanges();
    bool confirmationPending() const { return m_confirmationPending; }
    int confirmationSeconds() const { return m_confirmationSeconds; }

Q_SIGNALS:
    void changed();
    void modeConfirmationChanged();

private:
    QVariantList m_outputs;
    int m_transactionSocket = -1;
    QSocketNotifier *m_transactionNotifier = nullptr;
    void finishTransaction();
    void sendTransaction(const QVariantMap &request);
    QByteArray m_transactionOutput;
    QString m_confirmationToken;
    bool m_transactionFinished = false;
    QTimer m_confirmationTimer;
    bool m_confirmationPending = false;
    int m_confirmationSeconds = 0;
};
