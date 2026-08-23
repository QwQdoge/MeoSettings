#include "backendbase.h"

BackendBase::BackendBase(QObject *parent)
    : QObject(parent)
{
}
bool BackendBase::available() const
{
    return m_available;
}

bool BackendBase::busy() const
{
    return m_busy;
}

QString BackendBase::error() const
{
    return m_error;
}

void BackendBase::clearError()
{
    setError({});
}

void BackendBase::setAvailable(const bool available)
{
    if (m_available == available) {
        return;
    }
    m_available = available;
    Q_EMIT availableChanged();
}

void BackendBase::setBusy(const bool busy)
{
    if (m_busy == busy) {
        return;
    }
    m_busy = busy;
    Q_EMIT busyChanged();
}

void BackendBase::setError(const QString &error)
{
    if (m_error == error) {
        return;
    }
    m_error = error;
    Q_EMIT errorChanged();
}
