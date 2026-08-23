#pragma once

#include "../core/backendbase.h"

#include <QVariantList>

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

Q_SIGNALS:
    void changed();

private:
    QVariantList m_outputs;
};
