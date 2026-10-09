#pragma once
#include "../core/backendbase.h"
#include <QVariantList>
#include <QUrl>
#include <QProcess>
#include <QTimer>

class WallpaperBackend final : public BackendBase
{
    Q_OBJECT
    Q_PROPERTY(QVariantList wallpapers READ wallpapers NOTIFY changed)
    Q_PROPERTY(QVariantList images READ images NOTIFY changed)
public:
    explicit WallpaperBackend(QObject *parent = nullptr);
    QVariantList wallpapers() const { return m_wallpapers; }
    QVariantList images() const { return m_images; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void applyImage(const QUrl &image, const QString &fillMode, int screen = -1);
    Q_INVOKABLE QUrl importImage(const QUrl &image);
    Q_INVOKABLE void removeImported(const QUrl &image);
Q_SIGNALS:
    void changed();
    void imageApplied();
private:
    QString validatedImage(const QUrl &image);
    QVariantList m_wallpapers, m_images;
    QString m_tool;
    QProcess m_process;
    QTimer m_timeout;
};
