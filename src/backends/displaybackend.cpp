#include "displaybackend.h"

#include <KScreen/Config>
#include <KScreen/ConfigMonitor>
#include <KScreen/GetConfigOperation>
#include <KScreen/Mode>
#include <KScreen/Output>

#include <QPointer>
#include <QCoreApplication>
#include <QFileInfo>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>
#include <climits>
#include <cerrno>

DisplayBackend::DisplayBackend(QObject *parent)
    : BackendBase(parent)
{
    connect(KScreen::ConfigMonitor::instance(), &KScreen::ConfigMonitor::configurationChanged,
            this, &DisplayBackend::refresh);
    connect(this, &DisplayBackend::changed, this, &DisplayBackend::modeConfirmationChanged);
    m_confirmationTimer.setInterval(1000);
    connect(&m_confirmationTimer, &QTimer::timeout, this, [this] {
        m_confirmationSeconds = qMax(0, m_confirmationSeconds - 1);
        Q_EMIT changed();
        if (!m_confirmationSeconds) revertChanges();
    });
    refresh();
}

QVariantList DisplayBackend::outputs() const
{
    return m_outputs;
}

QString DisplayBackend::summary() const
{
    if (m_outputs.isEmpty()) {
        return available() ? tr("No connected displays") : tr("Display service unavailable");
    }
    return tr("%n connected display(s)", "", m_outputs.size());
}

void DisplayBackend::refresh()
{
    if (busy()) {
        return;
    }
    setBusy(true);
    auto *operation = new KScreen::GetConfigOperation(KScreen::ConfigOperation::NoEDID, this);
    connect(operation, &KScreen::ConfigOperation::finished, this,
            [this, operation](KScreen::ConfigOperation *finished) {
                setBusy(false);
                if (finished->hasError() || !finished->config()) {
                    setAvailable(false);
                    setError(finished->hasError() ? finished->errorString() : tr("No display configuration was returned."));
                    Q_EMIT changed();
                    operation->deleteLater();
                    return;
                }

                QVariantList nextOutputs;
                const auto configuration = finished->config();
                const auto configuredOutputs = configuration->outputs();
                const auto primaryOutput = configuration->primaryOutput();
                nextOutputs.reserve(configuredOutputs.size());
                for (const auto &output : configuredOutputs) {
                    if (!output || !output->isConnected()) {
                        continue;
                    }
                    const auto mode = output->currentMode();
                    const auto modeSize = mode ? mode->size() : QSize{};
                    const QString label = !output->model().isEmpty()
                        ? output->model()
                        : (!output->name().isEmpty() ? output->name() : tr("Display"));
                    QVariantList modes;
                    for (const auto &candidate : output->modes()) {
                        if (!candidate) continue;
                        modes.append(QVariantMap{{"id", candidate->id()}, {"width", candidate->size().width()},
                            {"height", candidate->size().height()}, {"refreshRate", candidate->refreshRate()},
                            {"current", candidate->id() == output->currentModeId()}, {"preferred", output->preferredModes().contains(candidate->id())},
                            {"label", tr("%1 × %2 · %3 Hz").arg(candidate->size().width()).arg(candidate->size().height()).arg(candidate->refreshRate(), 0, 'f', 2)}});
                    }
                    nextOutputs.push_back(QVariantMap{
                        {QStringLiteral("id"), output->id()},
                        {QStringLiteral("modes"), modes},
                        {QStringLiteral("modeId"), output->currentModeId()},
                        {QStringLiteral("currentModeId"), output->currentModeId()},
                        {QStringLiteral("rotation"), int(output->rotation())},
                        {QStringLiteral("x"), output->pos().x()},
                        {QStringLiteral("y"), output->pos().y()},
                        {QStringLiteral("scaleSupported"), configuration->supportedFeatures().testFlag(KScreen::Config::Feature::PerOutputScaling)},
                        {QStringLiteral("name"), label},
                        {QStringLiteral("connector"), output->name()},
                        {QStringLiteral("vendor"), output->vendor()},
                        {QStringLiteral("enabled"), output->isEnabled()},
                        {QStringLiteral("primary"), primaryOutput && output->id() == primaryOutput->id()},
                        {QStringLiteral("width"), modeSize.width()},
                        {QStringLiteral("height"), modeSize.height()},
                        {QStringLiteral("refreshRate"), mode ? mode->refreshRate() : 0.0},
                        {QStringLiteral("scale"), output->scale()},
                    });
                }
                m_outputs = nextOutputs;
                setAvailable(true);
                Q_EMIT changed();
                operation->deleteLater();
            });
}

DisplayBackend::~DisplayBackend()
{
    // EOF requests rollback. The helper is deliberately not a QProcess child:
    // QProcess destruction would kill it before it could restore the snapshot.
    if (m_transactionSocket >= 0) ::close(m_transactionSocket);
}

void DisplayBackend::sendTransaction(const QVariantMap &request)
{
    if (m_transactionSocket < 0) return;
    const auto line = QJsonDocument(QJsonObject::fromVariantMap(request)).toJson(QJsonDocument::Compact) + '\n';
    if (::send(m_transactionSocket, line.constData(), size_t(line.size()), MSG_NOSIGNAL) != line.size()) {
        setError(tr("The display recovery connection was interrupted."));
        finishTransaction();
    }
}

void DisplayBackend::finishTransaction()
{
    m_confirmationTimer.stop(); m_confirmationPending = false; m_confirmationSeconds = 0;
    m_confirmationToken.clear();
    if (m_transactionNotifier) { m_transactionNotifier->setEnabled(false); m_transactionNotifier->deleteLater(); m_transactionNotifier = nullptr; }
    if (m_transactionSocket >= 0) ::close(m_transactionSocket);
    m_transactionSocket = -1; setBusy(false); Q_EMIT changed(); refresh();
}

void DisplayBackend::applyOutput(int outputId, const QVariantMap &changes)
{
    if (busy() || m_transactionSocket >= 0) return;
    QVariantMap request = changes;
    bool found = false;
    for (const auto &value : m_outputs) {
        const auto output = value.toMap();
        if (output.value("id").toInt() == outputId) {
            request.insert("outputId", outputId); request.insert("connector", output.value("connector"));
            found = true; break;
        }
    }
    if (!found) { setError(tr("Refresh the display list before changing this display.")); return; }
    QString program = QCoreApplication::applicationDirPath() + QStringLiteral("/meo-settings-display-transaction");
    if (!QFileInfo(program).isExecutable()) program = QString::fromUtf8(MEO_SETTINGS_LIBEXEC_DIR) + QStringLiteral("/meo-settings-display-transaction");
    if (!QFileInfo(program).isExecutable()) { setError(tr("The display recovery helper is not installed.")); return; }
    int sockets[2];
    if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sockets) != 0) {
        setError(tr("The display recovery connection could not start.")); return;
    }
    const QByteArray executable = QFile::encodeName(program);
    // Double fork prevents zombies and leaves the guardian alive after its UI
    // exits. Between fork and exec use only async-signal-safe system calls.
    const pid_t launcher = ::fork();
    if (launcher == 0) {
        ::close(sockets[0]);
        const pid_t guardian = ::fork();
        if (guardian < 0) _exit(127);
        if (guardian > 0) _exit(0);
        ::dup2(sockets[1], STDIN_FILENO); ::dup2(sockets[1], STDOUT_FILENO);
        ::syscall(SYS_close_range, 3u, UINT_MAX, 0u);
        ::execl(executable.constData(), executable.constData(), static_cast<char *>(nullptr));
        _exit(127);
    }
    ::close(sockets[1]);
    if (launcher < 0) { ::close(sockets[0]); setError(tr("The display recovery helper could not start.")); return; }
    int status = 0;
    while (::waitpid(launcher, &status, 0) < 0 && errno == EINTR) {}
    if (!WIFEXITED(status) || WEXITSTATUS(status)) { ::close(sockets[0]); setError(tr("The display recovery helper could not start.")); return; }
    clearError(); setBusy(true); m_transactionOutput.clear(); m_transactionFinished = false;
    m_transactionSocket = sockets[0];
    m_transactionNotifier = new QSocketNotifier(m_transactionSocket, QSocketNotifier::Read, this);
    connect(m_transactionNotifier, &QSocketNotifier::activated, this, [this] {
        char buffer[4096];
        const auto size = ::recv(m_transactionSocket, buffer, sizeof(buffer), MSG_DONTWAIT);
        if (size == 0) {
            if (!m_transactionFinished && error().isEmpty()) setError(tr("The display recovery helper closed without confirming the result."));
            finishTransaction(); return;
        }
        if (size < 0) { if (errno != EAGAIN && errno != EINTR) finishTransaction(); return; }
        m_transactionOutput.append(buffer, size);
        if (m_transactionOutput.size() > 16384) { setError(tr("Invalid display recovery response.")); finishTransaction(); return; }
        while (m_transactionOutput.contains('\n')) {
            const auto index = m_transactionOutput.indexOf('\n');
            const auto reply = QJsonDocument::fromJson(m_transactionOutput.left(index)).object();
            m_transactionOutput.remove(0, index + 1);
            const QString phase = reply.value("phase").toString();
            if (phase == "confirm") {
                m_confirmationToken = reply.value("token").toString();
                m_confirmationPending = true; m_confirmationSeconds = qBound(0, reply.value("remainingSeconds").toInt(15), 15);
                m_confirmationTimer.start(); Q_EMIT changed();
            } else if (phase == "failed" || phase == "rollback-failed") {
                m_transactionFinished = true; setError(reply.value("error").toString());
            } else if (phase == "committed" || phase == "reverted") {
                m_transactionFinished = true;
                if (!reply.value("error").toString().isEmpty()) setError(reply.value("error").toString());
            } else { setError(tr("Invalid display recovery response.")); revertChanges(); }
        }
    });
    sendTransaction(request);
}

void DisplayBackend::keepChanges()
{
    if (!m_confirmationPending) return;
    sendTransaction({{"action", "keep"}, {"token", m_confirmationToken}});
    m_confirmationTimer.stop(); m_confirmationPending = false; Q_EMIT changed();
}

void DisplayBackend::revertChanges()
{
    if (m_transactionSocket < 0) return;
    ::shutdown(m_transactionSocket, SHUT_WR);
    m_confirmationTimer.stop(); m_confirmationPending = false; Q_EMIT changed();
}

void DisplayBackend::setScale(int outputId, qreal scale) { applyOutput(outputId, {{"scale", scale}}); }
void DisplayBackend::setMode(int outputId, const QString &modeId) { applyOutput(outputId, {{"modeId", modeId}}); }
void DisplayBackend::setPrimary(int outputId) { applyOutput(outputId, {{"primary", true}}); }
