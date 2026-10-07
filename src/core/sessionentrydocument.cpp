#include "sessionentrydocument.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>

namespace
{
using Scope = SessionEntryDocument::Scope;
using Result = SessionEntryDocument::ValidationResult;

Result failure(const QString &path, const QString &message)
{
    return {false, path, message, {}};
}

bool hasExactKeys(const QJsonObject &object,
                  const QStringList &required,
                  const QString &path,
                  Result *result)
{
    const QSet<QString> allowed(required.cbegin(), required.cend());
    for (const QString &key : required) {
        if (!object.contains(key)) {
            *result = failure(path + QLatin1Char('.') + key,
                              QObject::tr("Required setting is missing."));
            return false;
        }
    }
    for (auto it = object.cbegin(); it != object.cend(); ++it) {
        if (!allowed.contains(it.key())) {
            *result = failure(path + QLatin1Char('.') + it.key(),
                              QObject::tr("Unknown settings are not accepted."));
            return false;
        }
    }
    return true;
}

bool requireBoolean(const QJsonObject &object,
                    const QString &key,
                    const QString &path,
                    Result *result)
{
    if (!object.value(key).isBool()) {
        *result = failure(path + QLatin1Char('.') + key,
                          QObject::tr("Expected true or false."));
        return false;
    }
    return true;
}

bool requireString(const QJsonObject &object,
                   const QString &key,
                   const QString &path,
                   QString *value,
                   Result *result)
{
    if (!object.value(key).isString()) {
        *result = failure(path + QLatin1Char('.') + key,
                          QObject::tr("Expected text."));
        return false;
    }
    *value = object.value(key).toString();
    return true;
}

bool requireEnum(const QJsonObject &object,
                 const QString &key,
                 const QString &path,
                 const QSet<QString> &allowed,
                 QString *value,
                 Result *result)
{
    if (!requireString(object, key, path, value, result)) {
        return false;
    }
    if (!allowed.contains(*value)) {
        *result = failure(path + QLatin1Char('.') + key,
                          QObject::tr("Unsupported setting value."));
        return false;
    }
    return true;
}

bool containsControlCharacter(const QString &value)
{
    static const QRegularExpression controls(QStringLiteral("[\\x00-\\x1f\\x7f]"));
    return value.contains(controls);
}

bool isSafeAssetId(const QString &assetId)
{
    static const QRegularExpression syntax(
        QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._/-]{0,255}$"));
    if (!syntax.match(assetId).hasMatch()) {
        return false;
    }

    // Asset IDs are symbolic identifiers, never filesystem paths.  Reject
    // traversal-like or empty path segments even though the base schema's
    // character class would otherwise permit them.
    const QStringList segments = assetId.split(QLatin1Char('/'));
    for (const QString &segment : segments) {
        if (segment.isEmpty() || segment == QLatin1String(".") || segment == QLatin1String("..")) {
            return false;
        }
    }
    return true;
}

bool validateWallpaper(const QJsonObject &wallpaper,
                       Scope scope,
                       const QString &path,
                       Result *result)
{
    if (!hasExactKeys(wallpaper,
                      {QStringLiteral("source"), QStringLiteral("assetId"), QStringLiteral("fillMode")},
                      path,
                      result)) {
        return false;
    }

    QString source;
    QSet<QString> allowedSources{
        QStringLiteral("system-default"),
        QStringLiteral("managed-asset"),
    };
    if (scope == Scope::LockScreen) {
        allowedSources.insert(QStringLiteral("current-user"));
    }
    if (!requireEnum(wallpaper, QStringLiteral("source"), path, allowedSources, &source, result)) {
        return false;
    }

    QString assetId;
    if (!requireString(wallpaper, QStringLiteral("assetId"), path, &assetId, result)) {
        return false;
    }
    if (source == QLatin1String("system-default")) {
        if (!assetId.isEmpty()) {
            *result = failure(path + QStringLiteral(".assetId"),
                              QObject::tr("The system-default wallpaper must not carry an asset ID."));
            return false;
        }
    } else if (!isSafeAssetId(assetId)) {
        *result = failure(path + QStringLiteral(".assetId"),
                          QObject::tr("Wallpaper asset ID is invalid or unsafe."));
        return false;
    }

    QString fillMode;
    return requireEnum(wallpaper,
                       QStringLiteral("fillMode"),
                       path,
                       {QStringLiteral("cover"), QStringLiteral("contain"),
                        QStringLiteral("stretch"), QStringLiteral("center")},
                       &fillMode,
                       result);
}

bool validateAppearance(const QJsonObject &appearance,
                        Scope scope,
                        Result *result)
{
    const QString path = QStringLiteral("$.appearance");
    if (!hasExactKeys(appearance,
                      {QStringLiteral("clockStyle"), QStringLiteral("showDate"),
                       QStringLiteral("backgroundTreatment"), QStringLiteral("wallpaperMode"),
                       QStringLiteral("wallpaper")},
                      path,
                      result)) {
        return false;
    }

    QString value;
    if (!requireEnum(appearance, QStringLiteral("clockStyle"), path,
                     {QStringLiteral("large"), QStringLiteral("compact")}, &value, result)
        || !requireBoolean(appearance, QStringLiteral("showDate"), path, result)
        || !requireEnum(appearance, QStringLiteral("backgroundTreatment"), path,
                        {QStringLiteral("dim"), QStringLiteral("blur"), QStringLiteral("solid")},
                        &value, result)
        || !requireEnum(appearance, QStringLiteral("wallpaperMode"), path,
                        {QStringLiteral("follow-desktop"), QStringLiteral("managed")},
                        &value, result)) {
        return false;
    }

    if (scope == Scope::Login
        && appearance.value(QStringLiteral("wallpaperMode")).toString() != QLatin1String("managed")) {
        *result = failure(path + QStringLiteral(".wallpaperMode"),
                          QObject::tr("Login wallpaper must use the managed mode."));
        return false;
    }

    const QJsonValue wallpaperValue = appearance.value(QStringLiteral("wallpaper"));
    if (!wallpaperValue.isObject()) {
        *result = failure(path + QStringLiteral(".wallpaper"), QObject::tr("Expected an object."));
        return false;
    }
    return validateWallpaper(wallpaperValue.toObject(), scope, path + QStringLiteral(".wallpaper"), result);
}

bool validateModules(const QJsonObject &modules, Scope scope, Result *result)
{
    const QString path = QStringLiteral("$.modules");
    if (!hasExactKeys(modules,
                      {QStringLiteral("media"), QStringLiteral("weather"),
                       QStringLiteral("audio"), QStringLiteral("weatherCity")},
                      path,
                      result)) {
        return false;
    }

    for (const QString &key : {QStringLiteral("media"), QStringLiteral("weather"), QStringLiteral("audio")}) {
        if (!requireBoolean(modules, key, path, result)) {
            return false;
        }
    }

    QString weatherCity;
    if (!requireString(modules, QStringLiteral("weatherCity"), path, &weatherCity, result)) {
        return false;
    }
    if (weatherCity.size() > 96 || containsControlCharacter(weatherCity)) {
        *result = failure(path + QStringLiteral(".weatherCity"),
                          QObject::tr("Weather city must be at most 96 characters and contain no control characters."));
        return false;
    }

    if (scope == Scope::Login
        && (modules.value(QStringLiteral("media")).toBool()
            || modules.value(QStringLiteral("weather")).toBool()
            || modules.value(QStringLiteral("audio")).toBool()
            || !weatherCity.isEmpty())) {
        *result = failure(path,
                          QObject::tr("Login configuration cannot consume user-session media, weather, or audio data."));
        return false;
    }
    return true;
}

bool validatePrivacy(const QJsonObject &privacy, Scope scope, Result *result)
{
    const QString path = QStringLiteral("$.privacy");
    if (!hasExactKeys(privacy,
                      {QStringLiteral("notificationVisibility"),
                       QStringLiteral("showAlbumArtwork"),
                       QStringLiteral("weatherLocation")},
                      path,
                      result)) {
        return false;
    }

    QString visibility;
    QString weatherLocation;
    if (!requireEnum(privacy, QStringLiteral("notificationVisibility"), path,
                     {QStringLiteral("hidden"), QStringLiteral("count"),
                      QStringLiteral("app-name"), QStringLiteral("full-content")},
                     &visibility, result)
        || !requireBoolean(privacy, QStringLiteral("showAlbumArtwork"), path, result)
        || !requireEnum(privacy, QStringLiteral("weatherLocation"), path,
                        {QStringLiteral("hidden"), QStringLiteral("city"), QStringLiteral("precise")},
                        &weatherLocation, result)) {
        return false;
    }

    if (scope == Scope::Login) {
        if (visibility != QLatin1String("hidden")) {
            *result = failure(path + QStringLiteral(".notificationVisibility"),
                              QObject::tr("Login configuration cannot expose notifications."));
            return false;
        }
        if (privacy.value(QStringLiteral("showAlbumArtwork")).toBool()) {
            *result = failure(path + QStringLiteral(".showAlbumArtwork"),
                              QObject::tr("Login configuration cannot expose album artwork."));
            return false;
        }
        if (weatherLocation != QLatin1String("hidden") && weatherLocation != QLatin1String("city")) {
            *result = failure(path + QStringLiteral(".weatherLocation"),
                              QObject::tr("Login weather location is limited to city-level data."));
            return false;
        }
    }
    return true;
}

bool validateLayout(const QJsonObject &layout, Scope scope, Result *result)
{
    const QString path = QStringLiteral("$.layout");
    if (!hasExactKeys(layout,
                      {QStringLiteral("activeAuthenticationScreen"), QStringLiteral("displayOverrides")},
                      path,
                      result)) {
        return false;
    }

    QString screenPolicy;
    if (!requireEnum(layout, QStringLiteral("activeAuthenticationScreen"), path,
                     {QStringLiteral("auto"), QStringLiteral("fixed-primary"),
                      QStringLiteral("follow-interaction")},
                     &screenPolicy, result)) {
        return false;
    }

    const QJsonValue overridesValue = layout.value(QStringLiteral("displayOverrides"));
    if (!overridesValue.isArray()) {
        *result = failure(path + QStringLiteral(".displayOverrides"), QObject::tr("Expected a list."));
        return false;
    }
    const QJsonArray overrides = overridesValue.toArray();
    if (overrides.size() > 16) {
        *result = failure(path + QStringLiteral(".displayOverrides"),
                          QObject::tr("At most 16 display overrides are supported."));
        return false;
    }

    QSet<QString> outputKeys;
    for (qsizetype index = 0; index < overrides.size(); ++index) {
        const QString itemPath = path + QStringLiteral(".displayOverrides[%1]").arg(index);
        if (!overrides.at(index).isObject()) {
            *result = failure(itemPath, QObject::tr("Expected an object."));
            return false;
        }
        const QJsonObject overrideObject = overrides.at(index).toObject();
        if (!hasExactKeys(overrideObject,
                          {QStringLiteral("outputKey"), QStringLiteral("wallpaper")},
                          itemPath,
                          result)) {
            return false;
        }

        QString outputKey;
        if (!requireString(overrideObject, QStringLiteral("outputKey"), itemPath, &outputKey, result)) {
            return false;
        }
        if (outputKey.isEmpty() || outputKey.size() > 256 || containsControlCharacter(outputKey)) {
            *result = failure(itemPath + QStringLiteral(".outputKey"),
                              QObject::tr("Display identity must be 1 to 256 characters with no control characters."));
            return false;
        }
        if (outputKeys.contains(outputKey)) {
            *result = failure(itemPath + QStringLiteral(".outputKey"),
                              QObject::tr("Each display identity may appear only once."));
            return false;
        }
        outputKeys.insert(outputKey);

        const QJsonValue wallpaperValue = overrideObject.value(QStringLiteral("wallpaper"));
        if (!wallpaperValue.isObject()) {
            *result = failure(itemPath + QStringLiteral(".wallpaper"), QObject::tr("Expected an object."));
            return false;
        }
        if (!validateWallpaper(wallpaperValue.toObject(), scope,
                               itemPath + QStringLiteral(".wallpaper"), result)) {
            return false;
        }
    }
    return true;
}

bool validateMotion(const QJsonObject &motion, Result *result)
{
    const QString path = QStringLiteral("$.motion");
    if (!hasExactKeys(motion, {QStringLiteral("reduceMotion")}, path, result)) {
        return false;
    }
    QString value;
    return requireEnum(motion, QStringLiteral("reduceMotion"), path,
                       {QStringLiteral("system"), QStringLiteral("always"), QStringLiteral("never")},
                       &value, result);
}

Result validateObject(const QJsonObject &root, std::optional<Scope> expectedScope)
{
    Result result;
    if (!hasExactKeys(root,
                      {QStringLiteral("schemaVersion"), QStringLiteral("scope"),
                       QStringLiteral("appearance"), QStringLiteral("modules"),
                       QStringLiteral("privacy"), QStringLiteral("layout"),
                       QStringLiteral("motion")},
                      QStringLiteral("$"),
                      &result)) {
        return result;
    }

    const QJsonValue schemaVersion = root.value(QStringLiteral("schemaVersion"));
    if (!schemaVersion.isDouble() || schemaVersion.toDouble() != 1.0) {
        return failure(QStringLiteral("$.schemaVersion"),
                       QObject::tr("Only session-entry schema version 1 is supported."));
    }

    const QJsonValue scopeValue = root.value(QStringLiteral("scope"));
    if (!scopeValue.isString()) {
        return failure(QStringLiteral("$.scope"), QObject::tr("Expected a scope name."));
    }

    Scope scope;
    const QString scopeName = scopeValue.toString();
    if (scopeName == QLatin1String("lockscreen")) {
        scope = Scope::LockScreen;
    } else if (scopeName == QLatin1String("login")) {
        scope = Scope::Login;
    } else {
        return failure(QStringLiteral("$.scope"), QObject::tr("Unsupported session-entry scope."));
    }
    if (expectedScope.has_value() && *expectedScope != scope) {
        return failure(QStringLiteral("$.scope"), QObject::tr("Document scope does not match this writer."));
    }

    for (const QString &key : {QStringLiteral("appearance"), QStringLiteral("modules"),
                               QStringLiteral("privacy"), QStringLiteral("layout"),
                               QStringLiteral("motion")}) {
        if (!root.value(key).isObject()) {
            return failure(QStringLiteral("$.") + key, QObject::tr("Expected an object."));
        }
    }

    if (!validateAppearance(root.value(QStringLiteral("appearance")).toObject(), scope, &result)
        || !validateModules(root.value(QStringLiteral("modules")).toObject(), scope, &result)
        || !validatePrivacy(root.value(QStringLiteral("privacy")).toObject(), scope, &result)
        || !validateLayout(root.value(QStringLiteral("layout")).toObject(), scope, &result)
        || !validateMotion(root.value(QStringLiteral("motion")).toObject(), &result)) {
        return result;
    }

    result.ok = true;
    result.document = root.toVariantMap();
    return result;
}

QVariantMap wallpaperDefaults()
{
    return {
        {QStringLiteral("source"), QStringLiteral("system-default")},
        {QStringLiteral("assetId"), QString()},
        {QStringLiteral("fillMode"), QStringLiteral("cover")},
    };
}
} // namespace

QString SessionEntryDocument::scopeName(Scope scope)
{
    return scope == Scope::LockScreen ? QStringLiteral("lockscreen") : QStringLiteral("login");
}

QVariantMap SessionEntryDocument::defaults(Scope scope)
{
    const bool login = scope == Scope::Login;
    return {
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("scope"), scopeName(scope)},
        {QStringLiteral("appearance"), QVariantMap{
             {QStringLiteral("clockStyle"), QStringLiteral("large")},
             {QStringLiteral("showDate"), true},
             {QStringLiteral("backgroundTreatment"), QStringLiteral("dim")},
             {QStringLiteral("wallpaperMode"), login ? QStringLiteral("managed")
                                                       : QStringLiteral("follow-desktop")},
             {QStringLiteral("wallpaper"), wallpaperDefaults()},
         }},
        {QStringLiteral("modules"), QVariantMap{
             {QStringLiteral("media"), !login},
             {QStringLiteral("weather"), !login},
             {QStringLiteral("audio"), !login},
             {QStringLiteral("weatherCity"), QString()},
         }},
        {QStringLiteral("privacy"), QVariantMap{
             // Meo Settings uses the safer count-only default for a user's
             // lock screen.  The login surface is always hidden.
             {QStringLiteral("notificationVisibility"),
              login ? QStringLiteral("hidden") : QStringLiteral("count")},
             {QStringLiteral("showAlbumArtwork"), !login},
             {QStringLiteral("weatherLocation"),
              login ? QStringLiteral("hidden") : QStringLiteral("city")},
         }},
        {QStringLiteral("layout"), QVariantMap{
             {QStringLiteral("activeAuthenticationScreen"), QStringLiteral("auto")},
             {QStringLiteral("displayOverrides"), QVariantList{}},
         }},
        {QStringLiteral("motion"), QVariantMap{
             {QStringLiteral("reduceMotion"), QStringLiteral("system")},
         }},
    };
}

SessionEntryDocument::ValidationResult SessionEntryDocument::validate(
    const QVariantMap &document,
    std::optional<Scope> expectedScope)
{
    return validateObject(QJsonObject::fromVariantMap(document), expectedScope);
}

SessionEntryDocument::ValidationResult SessionEntryDocument::parse(
    const QByteArray &json,
    std::optional<Scope> expectedScope)
{
    if (json.size() > MaximumSerializedBytes) {
        return failure(QStringLiteral("$"), QObject::tr("Session-entry document is too large."));
    }

    QJsonParseError parseError;
    const QJsonDocument parsed = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return failure(QStringLiteral("$"),
                       QObject::tr("Invalid JSON: %1").arg(parseError.errorString()));
    }
    if (!parsed.isObject()) {
        return failure(QStringLiteral("$"), QObject::tr("Session-entry document must be a JSON object."));
    }
    return validateObject(parsed.object(), expectedScope);
}

QByteArray SessionEntryDocument::serialize(const QVariantMap &document)
{
    return QJsonDocument(QJsonObject::fromVariantMap(document)).toJson(QJsonDocument::Indented);
}
