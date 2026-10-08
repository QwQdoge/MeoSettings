#pragma once

#include <QByteArray>
#include <QString>
#include <QVariantMap>

#include <optional>

// Pure value-layer contract for Meo session-entry presentation documents.
//
// This class deliberately knows nothing about KScreenLocker, PAM, display
// managers, D-Bus authorization, or filesystem locations.  It validates the
// versioned presentation schema before any platform/backend layer is allowed to
// persist or apply a request.
class SessionEntryDocument final
{
public:
    enum class Scope {
        LockScreen,
        Login,
    };

    struct ValidationResult {
        bool ok = false;
        QString path;
        QString error;
        QVariantMap document;
    };

    static constexpr qsizetype MaximumSerializedBytes = 64 * 1024;

    static QString scopeName(Scope scope);
    static QVariantMap defaults(Scope scope);

    static ValidationResult validate(
        const QVariantMap &document,
        std::optional<Scope> expectedScope = std::nullopt);
    static ValidationResult parse(
        const QByteArray &json,
        std::optional<Scope> expectedScope = std::nullopt);

    // Serialization is intentionally separate from validation so callers must
    // make the trust boundary explicit: validate first, then serialize only the
    // returned normalized document.
    static QByteArray serialize(const QVariantMap &document);
};
