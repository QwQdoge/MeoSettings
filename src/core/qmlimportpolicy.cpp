#include "qmlimportpolicy.h"

#include <QDir>
#include <QLibraryInfo>
#include <QQmlEngine>

namespace MeoQmlImportPolicy
{
QString systemImportRoot()
{
    return QDir::cleanPath(QLibraryInfo::path(QLibraryInfo::QmlImportsPath));
}

void prioritizeMeoUi(QQmlEngine &engine, const QString &developmentImportRoot)
{
    const QString preferredRoot = developmentImportRoot.isEmpty()
        ? systemImportRoot()
        : QDir::cleanPath(developmentImportRoot);

    if (!preferredRoot.isEmpty() && QDir(preferredRoot).exists()) {
        // QQmlEngine prepends paths added through addImportPath(). Installed
        // builds therefore keep the package-managed MeoUI module ahead of a
        // stale user QML_IMPORT_PATH, while source builds can explicitly pass
        // their sibling MeoUI build root.
        engine.addImportPath(preferredRoot);
    }
}
}
