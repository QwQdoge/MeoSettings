#pragma once

#include <QString>

class QQmlEngine;

namespace MeoQmlImportPolicy
{
QString systemImportRoot();
void prioritizeMeoUi(QQmlEngine &engine, const QString &developmentImportRoot = {});
}
