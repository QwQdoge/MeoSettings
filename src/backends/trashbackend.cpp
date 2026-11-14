#include "trashbackend.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QtConcurrent>

namespace
{
constexpr int kMaximumEntries = 200000;

QStringList emptyCommand()
{
    const QString ktrash = QStandardPaths::findExecutable(QStringLiteral("ktrash6"));
    if (!ktrash.isEmpty())
        return {ktrash, QStringLiteral("--empty")};
    const QString gio = QStandardPaths::findExecutable(QStringLiteral("gio"));
    if (!gio.isEmpty())
        return {gio, QStringLiteral("trash"), QStringLiteral("--empty")};
    return {};
}
} // namespace

TrashBackend::TrashBackend(QObject *parent)
    : BackendBase(parent)
{
    connect(&m_watcher, &QFutureWatcher<QPair<qulonglong, int>>::finished, this, [this] {
        const auto result = m_watcher.result();
        m_bytes = result.first;
        m_itemCount = result.second;
        setBusy(false);
        Q_EMIT changed();
    });
    setAvailable(true);
}

QString TrashBackend::filesDirectory()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/Trash/files");
}

QPair<qulonglong, int> TrashBackend::measure(const QString &directory)
{
    QDir root(directory);
    if (!root.exists())
        return {0, 0};
    const int items = static_cast<int>(root.entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot).size());
    qulonglong total = 0;
    int visited = 0;
    QDirIterator it(directory, QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);
    while (it.hasNext() && visited < kMaximumEntries) {
        it.next();
        const QFileInfo info = it.fileInfo();
        if (!info.isSymLink())
            total += static_cast<qulonglong>(info.size());
        ++visited;
    }
    return {total, items};
}

bool TrashBackend::canEmpty() const
{
    return m_itemCount > 0 && !emptyCommand().isEmpty();
}

void TrashBackend::refresh()
{
    if (m_watcher.isRunning())
        return;
    setBusy(true);
    const QString directory = filesDirectory();
    m_watcher.setFuture(QtConcurrent::run([directory] { return TrashBackend::measure(directory); }));
}

void TrashBackend::empty()
{
    const QStringList command = emptyCommand();
    if (command.isEmpty()) {
        setError(tr("No trash tool is installed."));
        return;
    }
    clearError();
    auto *process = new QProcess(this);
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this, process](int exitCode, QProcess::ExitStatus status) {
                if (status != QProcess::NormalExit || exitCode != 0)
                    setError(tr("The trash could not be emptied."));
                process->deleteLater();
                refresh();
            });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            setError(tr("The trash could not be emptied."));
            process->deleteLater();
        }
    });
    setBusy(true);
    process->start(command.first(), command.mid(1));
}
