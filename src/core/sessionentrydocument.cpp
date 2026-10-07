#include "sessionentrydocument.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QObject>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>

namespace
{
using Scope = SessionEntryDocument::Scope;
using Result = SessionEntryDocument::ValidationResult;

class DocumentValidator final
{
public:
    explicit DocumentValidator(std::optional<Scope> expectedScope)
        : m_expectedScope(expectedScope)
    {
    }

    Result validate(const QJsonObject &root)
    {
        if (!exactShape(root,
                        {QStringLiteral("schemaVersion"), QStringLiteral("scope"),
                         QStringLiteral("appearance"), QStringLiteral("modules"),
                         QStringLiteral("privacy"), QStringLiteral("layout"),
                         QStringLiteral("motion")},
                        QStringLiteral("$"))) {
            return m_result;
        }

        const QJsonValue schemaVersion = root.value(QStringLiteral("schemaVersion"));
        if (!schemaVersion.isDouble() || schemaVersion.toDouble() != 1.0) {
            return fail(QStringLiteral("$.schemaVersion"),
                        QObject::tr("Only session-entry schema version 1 is supported."));
        }

        const QJsonValue scopeValue = root.value(QStringLiteral("scope"));
        if (!scopeValue.isString()) {
            return fail(QStringLiteral("$.scope"), QObject::tr("Expected a scope name."));
        }

        Scope scope;
        const QString scopeText = scopeValue.toString();
        if (scopeText == QLatin1String("lockscreen")) {
            scope = Scope::LockScreen;
        } else if (scopeText == QLatin1String("login")) {
            scope = Scope::Login;
        } else {
            return fail(QStringLiteral("$.scope"), QObject::tr("Unsupported session-entry scope."));
        }
        if (m_expectedScope.has_value() && *m_expectedScope != scope) {
            return fail(QStringLiteral("$.scope"),
                        QObject::tr("Document scope does not match this writer."));
        }

        QJsonObject appearance;
        QJsonObject modules;
        QJsonObject privacy;
        QJsonObject layout;
        QJsonObject motion;
        if (!objectValue(root, QStringLiteral("appearance"), QStringLiteral("$"), &appearance)
            || !objectValue(root, QStringLiteral("modules"), QStringLiteral("$"), &modules)
            || !objectValue(root, QStringLiteral("privacy"), QStringLiteral("$"), &privacy)
            || !objectValue(root, QStringLiteral("layout"), QStringLiteral("$"), &layout)
            || !objectValue(root, QStringLiteral("motion"), QStringLiteral("$"), &motion)
            || !validateAppearance(appearance, scope)
            || !validateModules(modules, scope)
            || !validatePrivacy(privacy, scope)
            || !validateLayout(layout, scope)
            || !validateMotion(motion)) {
            return m_result;
        }

        m_result.ok = true;
        m_result.document = root.toVariantMap();
        return m_result;
    }

private:
    Result fail(const QString &path, const QString &message)
    {
        m_result = {false, path, message, {}};
        return m_result;
    }

    bool failBool(const QString &path, const QString &message)
    {
        fail(path, message);
        return false;
    }

    bool exactShape(const QJsonObject &object, const QStringList &keys, const QString &path)
    {
        QSet<QString> allowed;
        for (const QString &key : keys) {
            allowed.insert(key);
            if (!object.contains(key)) {
                return failBool(path + QLatin1Char('.') + key,
                                QObject::tr("Required setting is missing."));
            }
        }
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            if (!allowed.contains(it.key())) {
                return failBool(path + QLatin1Char('.') + it.key(),
                                QObject::tr("Unknown settings are not accepted."));
            }
        }
        return true;
    }

    bool objectValue(const QJsonObject &parent,
                     const QString &key,
                     const QString &parentPath,
                     QJsonObject *value)
    {
        const QJsonValue candidate = parent.value(key);
        if (!candidate.isObject()) {
            return failBool(parentPath + QLatin1Char('.') + key, QObject::tr("Expected an object."));
        }
        *value = candidate.toObject();
        return true;
    }

    bool booleanValue(const QJsonObject &object, const QString &key, const QString &path)
    {
        if (!object.value(key).isBool()) {
            return failBool(path + QLatin1Char('.') + key,
                            QObject::tr("Expected true or false."));
        }
        return true;
    }

    bool stringValue(const QJsonObject &object,
                     const QString &key,
                     const QString &path,
                     QString *value)
    {
        if (!object.value(key).isString()) {
            return failBool(path + QLatin1Char('.') + key, QObject::tr("Expected text."));
        }
        *value = object.value(key).toString();
        return true;
    }

    bool enumValue(const QJsonObject &object,
                   const QString &key,
                   const QString &path,
                   const QSet<QString> &allowed,
                   QString *value = nullptr)
    {
        QString candidate;
        if (!stringValue(object, key, path, &candidate)) {
            return false;
        }
        if (!allowed.contains(candidate)) {
            return failBool(path + QLatin1Char('.') + key,
                            QObject::tr("Unsupported setting value."));
        }
        if (value) {
            *value = candidate;
        }
        return true;
    }

    static bool containsControlCharacter(const QString &value)
    {
        static const QRegularExpression controls(QStringLiteral("[\\x00-\\x1f\\x7f]"));
        return value.contains(controls);
    }

    static bool safeAssetId(const QString &assetId)
    {
        static const QRegularExpression syntax(
            QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._/-]{0,255}$"));
        if (!syntax.match(assetId).hasMatch()) {
            return false;
        }
        const QStringList segments = assetId.split(QLatin1Char('/'));
        for (const QString &segment : segments) {
            if (segment.isEmpty() || segment == QLatin1String(".") || segment == QLatin1String("..")) {
                return false;
            }
        }
        return true;
    }

    bool validateWallpaper(const QJsonObject &wallpaper, Scope scope, const QString &path)
    {
        if (!exactShape(wallpaper,
                        {QStringLiteral("source"), QStringLiteral("assetId"), QStringLiteral("fillMode")},
                        path)) {
            return false;
        }

        QSet<QString> allowedSources{
            QStringLiteral("system-default"),
            QStringLiteral("managed-asset"),
        };
        if (scope == Scope::LockScreen) {
            allowedSources.insert(QStringLiteral("current-user"));
        }

        QString source;
        QString assetId;
        if (!enumValue(wallpaper, QStringLiteral("source"), path, allowedSources, &source)
            || !stringValue(wallpaper, QStringLiteral("assetId"), path, &assetId)
            || !enumValue(wallpaper, QStringLiteral("fillMode"), path,
                          {QStringLiteral("cover"), QStringLiteral("contain"),
                           QStringLiteral("stretch"), QStringLiteral("center")})) {
            return false;
        }

        if (source == QLatin1String("system-default")) {
            if (!assetId.isEmpty()) {
                return failBool(path + QStringLiteral(".assetId"),
                                QObject::tr("The system-default wallpaper must not carry an asset ID."));
            }
        } else if (!safeAssetId(assetId)) {
            return failBool(path + QStringLiteral(".assetId"),
                            QObject::tr("Wallpaper asset ID is invalid or unsafe."));
        }
        return true;
    }

    bool validateAppearance(const QJsonObject &appearance, Scope scope)
    {
        const QString path = QStringLiteral("$.appearance");
        if (!exactShape(appearance,
                        {QStringLiteral("clockStyle"), QStringLiteral("showDate"),
                         QStringLiteral("backgroundTreatment"), QStringLiteral("wallpaperMode"),
                         QStringLiteral("wallpaper")},
                        path)
            || !enumValue(appearance, QStringLiteral("clockStyle"), path,
                          {QStringLiteral("large"), QStringLiteral("compact")})
            || !booleanValue(appearance, QStringLiteral("showDate"), path)
            || !enumValue(appearance, QStringLiteral("backgroundTreatment"), path,
                          {QStringLiteral("dim"), QStringLiteral("blur"), QStringLiteral("solid")})
            || !enumValue(appearance, QStringLiteral("wallpaperMode"), path,
                          {QStringLiteral("follow-desktop"), QStringLiteral("managed")})) {
            return false;
        }

        if (scope == Scope::Login
            && appearance.value(QStringLiteral("wallpaperMode")).toString() != QLatin1String("managed")) {
            return failBool(path + QStringLiteral(".wallpaperMode"),
                            QObject::tr("Login wallpaper must use the managed mode."));
        }

        QJsonObject wallpaper;
        return objectValue(appearance, QStringLiteral("wallpaper"), path, &wallpaper)
            && validateWallpaper(wallpaper, scope, path + QStringLiteral(".wallpaper"));
    }

    bool validateModules(const QJsonObject &modules, Scope scope)
    {
        const QString path = QStringLiteral("$.modules");
        if (!exactShape(modules,
                        {QStringLiteral("media"), QStringLiteral("weather"),
                         QStringLiteral("audio"), QStringLiteral("weatherCity")},
                        path)
            || !booleanValue(modules, QStringLiteral("media"), path)
            || !booleanValue(modules, QStringLiteral("weather"), path)
            || !booleanValue(modules, QStringLiteral("audio"), path)) {
            return false;
        }

        QString city;
        if (!stringValue(modules, QStringLiteral("weatherCity"), path, &city)) {
            return false;
        }
        if (city.size() > 96 || containsControlCharacter(city)) {
            return failBool(path + QStringLiteral(".weatherCity"),
                            QObject::tr("Weather city must be at most 96 characters and contain no control characters."));
        }
        if (scope == Scope::Login
            && (modules.value(QStringLiteral("media")).toBool()
                || modules.value(QStringLiteral("weather")).toBool()
                || modules.value(QStringLiteral("audio")).toBool()
                || !city.isEmpty())) {
            return failBool(path,
                            QObject::tr("Login configuration cannot consume user-session media, weather, or audio data."));
        }
        return true;
    }

    bool validatePrivacy(const QJsonObject &privacy, Scope scope)
    {
        const QString path = QStringLiteral("$.privacy");
        if (!exactShape(privacy,
                        {QStringLiteral("notificationVisibility"), QStringLiteral("showAlbumArtwork"),
                         QStringLiteral("weatherLocation")},
                        path)) {
            return false;
        }

        QString visibility;
        QString weatherLocation;
        if (!enumValue(privacy, QStringLiteral("notificationVisibility"), path,
                       {QStringLiteral("hidden"), QStringLiteral("count"),
                        QStringLiteral("app-name"), QStringLiteral("full-content")},
                       &visibility)
            || !booleanValue(privacy, QStringLiteral("showAlbumArtwork"), path)
            || !enumValue(privacy, QStringLiteral("weatherLocation"), path,
                          {QStringLiteral("hidden"), QStringLiteral("city"), QStringLiteral("precise")},
                          &weatherLocation)) {
            return false;
        }

        if (scope == Scope::Login) {
            if (visibility != QLatin1String("hidden")) {
                return failBool(path + QStringLiteral(".notificationVisibility"),
                                QObject::tr("Login configuration cannot expose notifications."));
            }
            if (privacy.value(QStringLiteral("showAlbumArtwork")).toBool()) {
                return failBool(path + QStringLiteral(".showAlbumArtwork"),
                                QObject::tr("Login configuration cannot expose album artwork."));
            }
            if (weatherLocation != QLatin1String("hidden") && weatherLocation != QLatin1String("city")) {
                return failBool(path + QStringLiteral(".weatherLocation"),
                                QObject::tr("Login weather location is limited to city-level data."));
            }
        }
        return true;
    }

    bool validateLayout(const QJsonObject &layout, Scope scope)
    {
        const QString path = QStringLiteral("$.layout");
        if (!exactShape(layout,
                        {QStringLiteral("activeAuthenticationScreen"), QStringLiteral("displayOverrides")},
                        path)
            || !enumValue(layout, QStringLiteral("activeAuthenticationScreen"), path,
                          {QStringLiteral("auto"), QStringLiteral("fixed-primary"),
                           QStringLiteral("follow-interaction")})) {
            return false;
        }

        const QJsonValue overridesValue = layout.value(QStringLiteral("displayOverrides"));
        if (!overridesValue.isArray()) {
            return failBool(path + QStringLiteral(".displayOverrides"), QObject::tr("Expected a list."));
        }
        const QJsonArray overrides = overridesValue.toArray();
        if (overrides.size() > 16) {
            return failBool(path + QStringLiteral(".displayOverrides"),
                            QObject::tr("At most 16 display overrides are supported."));
        }

        QSet<QString> outputKeys;
        for (qsizetype index = 0; index < overrides.size(); ++index) {
            const QString itemPath = path + QStringLiteral(".displayOverrides[%1]").arg(index);
            const QJsonValue itemValue = overrides.at(index);
            if (!itemValue.isObject()) {
                return failBool(itemPath, QObject::tr("Expected an object."));
            }
            const QJsonObject item = itemValue.toObject();
            if (!exactShape(item,
                            {QStringLiteral("outputKey"), QStringLiteral("wallpaper")},
                            itemPath)) {
                return false;
            }

            QString outputKey;
            if (!stringValue(item, QStringLiteral("outputKey"), itemPath, &outputKey)) {
                return false;
            }
            if (outputKey.isEmpty() || outputKey.size() > 256 || containsControlCharacter(outputKey)) {
                return failBool(itemPath + QStringLiteral(".outputKey"),
                                QObject::tr("Display identity must be 1 to 256 characters with no control characters."));
            }
            if (outputKeys.contains(outputKey)) {
                return failBool(itemPath + QStringLiteral(".outputKey"),
                                QObject::tr("Each display identity may appear only once."));
            }
            outputKeys.insert(outputKey);

            QJsonObject wallpaper;
            if (!objectValue(item, QStringLiteral("wallpaper"), itemPath, &wallpaper)
                || !validateWallpaper(wallpaper, scope, itemPath + QStringLiteral(".wallpaper"))) {
                return false;
            }
        }
        return true;
    }

    bool validateMotion(const QJsonObject &motion)
    {
        const QString path = QStringLiteral("$.motion");
        return exactShape(motion, {QStringLiteral("reduceMotion")}, path)
            && enumValue(motion, QStringLiteral("reduceMotion"), path,
                         {QStringLiteral("system"), QStringLiteral("always"), QStringLiteral("never")});
    }

    std::optional<Scope> m_expectedScope;
    Result m_result;
};

QVariantMap wallpaperDefaults()
{
    return {
        {QStringLiteral("source"), QStringLiteral("system-default")},
        {QStringLiteral("assetId"), QString()},
        {QStringLiteral("fillMode"), QStringLiteral("cover")},
    };
}

Result parseFailure(const QString &message)
{
    return {false, QStringLiteral("$"), message, {}};
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
             {QStringLiteral("wallpaperMode"),
              login ? QStringLiteral("managed") : QStringLiteral("follow-desktop")},
             {QStringLiteral("wallpaper"), wallpaperDefaults()},
         }},
        {QStringLiteral("modules"), QVariantMap{
             {QStringLiteral("media"), !login},
             {QStringLiteral("weather"), !login},
             {QStringLiteral("audio"), !login},
             {QStringLiteral("weatherCity"), QString()},
         }},
        {QStringLiteral("privacy"), QVariantMap{
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
    DocumentValidator validator(expectedScope);
    return validator.validate(QJsonObject::fromVariantMap(document));
}

SessionEntryDocument::ValidationResult SessionEntryDocument::parse(
    const QByteArray &json,
    std::optional<Scope> expectedScope)
{
    if (json.size() > MaximumSerializedBytes) {
        return parseFailure(QObject::tr("Session-entry document is too large."));
    }

    QJsonParseError parseError;
    const QJsonDocument parsed = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return parseFailure(QObject::tr("Invalid JSON: %1").arg(parseError.errorString()));
    }
    if (!parsed.isObject()) {
        return parseFailure(QObject::tr("Session-entry document must be a JSON object."));
    }

    DocumentValidator validator(expectedScope);
    return validator.validate(parsed.object());
}

QByteArray SessionEntryDocument::serialize(const QVariantMap &document)
{
    return QJsonDocument(QJsonObject::fromVariantMap(document)).toJson(QJsonDocument::Indented);
}
