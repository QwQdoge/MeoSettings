#pragma once
#include "../core/backendbase.h"
#include <QProcess>
#include <QTimer>
class AppearanceBackend final : public BackendBase {
    Q_OBJECT
    Q_PROPERTY(bool recoveryAvailable READ recoveryAvailable NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
public:
    explicit AppearanceBackend(QObject *parent = nullptr);
    bool recoveryAvailable() const;
    QString status() const { return m_status; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void applyPreset();
    Q_INVOKABLE void applyMode(const QString &mode);
    Q_INVOKABLE void restore();
Q_SIGNALS:
    void changed();
private:
    bool hasComponent(const QString &path) const;
    void run(const QString &tool, const QStringList &arguments);
    QProcess m_process;
    QTimer m_timeout;
    QString m_status;
};
