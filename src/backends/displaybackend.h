#pragma once

#include "../core/backendbase.h"

#include <KScreen/Config>
#include <KScreen/Output>

#include <QVariantList>

#include <functional>

class DisplayBackend final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(QVariantList outputs READ outputs NOTIFY changed)
    Q_PROPERTY(QString summary READ summary NOTIFY changed)

public:
    explicit DisplayBackend(QObject *parent = nullptr);

    QVariantList outputs() const;
    QString summary() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setScale(int outputId, qreal scale);
    Q_INVOKABLE void setMode(int outputId, const QString &modeId);
    Q_INVOKABLE void setPrimary(int outputId);

Q_SIGNALS:
    void changed();

private:
    using OutputMutation = std::function<bool(const KScreen::ConfigPtr &,
                                              const KScreen::OutputPtr &,
                                              QString *)>;

    void mutateOutput(int outputId, const OutputMutation &mutation);
    void failMutation(const QString &message);

    QVariantList m_outputs;
};
