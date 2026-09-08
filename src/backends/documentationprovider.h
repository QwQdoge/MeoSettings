#pragma once

#include <QObject>
#include <QVariantMap>

// Documentation is local by default.  Opening an original source is always a
// user action, never an implicit request from a privileged Settings process.
class DocumentationProvider final : public QObject
{
    Q_OBJECT
public:
    explicit DocumentationProvider(QObject *parent = nullptr);

    Q_INVOKABLE QVariantMap guide(const QString &id) const;
    Q_INVOKABLE bool openOriginal(const QString &id) const;
};
