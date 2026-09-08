#pragma once

#include <QObject>
#include <QVariantMap>

// Privacy boundary for fprintd.  This backend deliberately exposes only the
// product-safe status needed by Settings; it never reads images, templates, or
// matcher implementation details.
class FingerprintBackend final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool devicePresent READ devicePresent NOTIFY changed)
    Q_PROPERTY(QString deviceLabel READ deviceLabel NOTIFY changed)
    Q_PROPERTY(QString supportLevel READ supportLevel NOTIFY changed)
    Q_PROPERTY(QString supportSummary READ supportSummary NOTIFY changed)
    Q_PROPERTY(bool rawBiometricDataExposed READ rawBiometricDataExposed CONSTANT)

public:
    explicit FingerprintBackend(QObject *parent = nullptr);

    bool devicePresent() const;
    QString deviceLabel() const;
    QString supportLevel() const;
    QString supportSummary() const;
    bool rawBiometricDataExposed() const;

    Q_INVOKABLE QVariantMap supportPackage() const;
    Q_INVOKABLE void refresh();

Q_SIGNALS:
    void changed();

private:
    bool m_fpc9800 = false;
};
