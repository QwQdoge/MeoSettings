#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSocketNotifier>
#include <QTimer>
#include <QTextStream>
#include <QUuid>
#include <KScreen/Config>
#include <KScreen/GetConfigOperation>
#include <KScreen/SetConfigOperation>
#include <KScreen/Output>
#include <KScreen/Mode>
#include <unistd.h>
#include <fcntl.h>

// A separate process owns the rollback snapshot. Closing/crashing Settings
// closes its input pipe and reverts an unconfirmed topology independently.
class DisplayTransaction final : public QObject
{
public:
    DisplayTransaction() : input(STDIN_FILENO, QSocketNotifier::Read, this)
    {
        fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
        connect(&input, &QSocketNotifier::activated, this, [this] { readInput(); });
        deadline.setSingleShot(true);
        connect(&deadline, &QTimer::timeout, this, [this] { rollback(); });
        QTimer::singleShot(5000, this, [this] { if (!started) QCoreApplication::exit(1); });
    }

private:
    void reply(const QString &phase, const QString &message = {})
    {
        const QJsonObject response{{"phase", phase}, {"token", token}, {"error", message}, {"remainingSeconds", qMax(0, (deadline.remainingTime() + 999) / 1000)}};
        const QByteArray line = QJsonDocument(response).toJson(QJsonDocument::Compact) + '\n';
        // If the UI disappeared this pipe can be closed. SIGPIPE is ignored
        // in main; rollback continues even when there is no reply recipient.
        ::write(STDOUT_FILENO, line.constData(), size_t(line.size()));
    }
    void readInput()
    {
        char buffer[4096];
        const auto size = ::read(STDIN_FILENO, buffer, sizeof(buffer));
        if (size == 0) { input.setEnabled(false); rollback(); return; }
        if (size < 0) return;
        pending.append(buffer, size);
        if (pending.size() > 16384) { rollback(); return; }
        while (pending.contains('\n')) {
            const auto index = pending.indexOf('\n');
            const QByteArray line = pending.left(index); pending.remove(0, index + 1);
            QJsonParseError error;
            const auto document = QJsonDocument::fromJson(line, &error);
            if (error.error != QJsonParseError::NoError || !document.isObject()) { rollback(); return; }
            const auto request = document.object();
            if (!started) { started = true; apply(request); }
            else if (request.value("token").toString() == token && ready) {
                if (request.value("action").toString() == "keep") {
                    deadline.stop(); snapshot.clear(); reply("committed"); QCoreApplication::quit();
                } else rollback();
            }
        }
    }
    void apply(const QJsonObject &request)
    {
        const QStringList allowed{"outputId", "connector", "modeId", "scale", "rotation", "x", "y", "enabled", "primary"};
        for (auto it = request.begin(); it != request.end(); ++it)
            if (!allowed.contains(it.key())) { fail(QStringLiteral("Unsupported display field.")); return; }
        if (!request.value("outputId").isDouble() || !request.value("connector").isString()) {
            fail(QStringLiteral("A current output identity is required.")); return;
        }
        auto *get = new KScreen::GetConfigOperation(KScreen::ConfigOperation::NoEDID, this);
        connect(get, &KScreen::ConfigOperation::finished, this, [this, request](KScreen::ConfigOperation *operation) {
            if (operation->hasError() || !operation->config()) { fail(operation->errorString()); return; }
            snapshot = operation->config()->clone();
            auto next = snapshot->clone();
            const auto output = next->output(request.value("outputId").toInt(-1));
            if (!output || !output->isConnected() || output->name() != request.value("connector").toString()) {
                fail(QStringLiteral("The selected display changed. Refresh and try again.")); return;
            }
            if (request.contains("modeId")) {
                if (!request.value("modeId").isString() || !output->modes().contains(request.value("modeId").toString())) {
                    fail(QStringLiteral("Unsupported display mode.")); return;
                }
                output->setCurrentModeId(request.value("modeId").toString());
            }
            if (request.contains("scale")) {
                const double scale = request.value("scale").toDouble(-1);
                if (!request.value("scale").isDouble() || scale < 0.5 || scale > 4.0
                    || !next->supportedFeatures().testFlag(KScreen::Config::Feature::PerOutputScaling)) {
                    fail(QStringLiteral("Unsupported display scale.")); return;
                }
                output->setScale(scale);
            }
            if (request.contains("rotation")) {
                const int rotation = request.value("rotation").toInt(-1);
                if (!request.value("rotation").isDouble() || !QList<int>{1, 2, 4, 8}.contains(rotation)) {
                    fail(QStringLiteral("Unsupported display rotation.")); return;
                }
                output->setRotation(static_cast<KScreen::Output::Rotation>(rotation));
            }
            QPoint position = output->pos();
            for (const QString &axis : {QStringLiteral("x"), QStringLiteral("y")}) {
                if (!request.contains(axis)) continue;
                const double value = request.value(axis).toDouble(999999);
                if (!request.value(axis).isDouble() || !qIsFinite(value) || qAbs(value) > 16384 || value != int(value)) {
                    fail(QStringLiteral("Invalid display position.")); return;
                }
                if (axis == "x") position.setX(int(value)); else position.setY(int(value));
            }
            output->setPos(position);
            if (request.contains("enabled")) {
                if (!request.value("enabled").isBool()) { fail(QStringLiteral("Invalid display enablement.")); return; }
                output->setEnabled(request.value("enabled").toBool());
                if (output->isEnabled() && !output->currentMode()) {
                    if (output->preferredModeId().isEmpty()) { fail(QStringLiteral("This display has no usable mode.")); return; }
                    output->setCurrentModeId(output->preferredModeId());
                }
            }
            if (request.contains("primary")) {
                if (!request.value("primary").isBool() || !request.value("primary").toBool() || !output->isEnabled()) {
                    fail(QStringLiteral("Choose an enabled primary display.")); return;
                }
                next->setOutputPriority(output, 1);
            }
            if (!KScreen::Config::canBeApplied(next, KScreen::Config::ValidityFlag::RequireAtLeastOneEnabledScreen)) {
                fail(QStringLiteral("This configuration would leave no usable display.")); return;
            }
            token = QUuid::createUuid().toString(QUuid::WithoutBraces);
            applying = true;
            deadline.start(15000);
            auto *set = new KScreen::SetConfigOperation(next, this);
            connect(set, &KScreen::ConfigOperation::finished, this, [this](KScreen::ConfigOperation *finished) {
                applying = false;
                if (finished->hasError()) { reply("failed", finished->errorString()); rollback(); return; }
                if (rollbackRequested) { rollback(); return; }
                ready = true;
                reply("confirm");
            });
        });
    }
    void fail(const QString &message)
    {
        snapshot.clear(); reply("failed", message.isEmpty() ? QStringLiteral("Display service unavailable.") : message);
        QCoreApplication::exit(1);
    }
    void rollback()
    {
        if (applying) { rollbackRequested = true; return; }
        if (rollingBack) return;
        deadline.stop();
        if (!snapshot) { QCoreApplication::exit(1); return; }
        ready = false; rollingBack = true;
        // Hot-unplug/reconnect changes output identities and modes. Restore
        // matching connectors onto a fresh topology rather than submitting stale devices.
        auto *get = new KScreen::GetConfigOperation(KScreen::ConfigOperation::NoEDID, this);
        connect(get, &KScreen::ConfigOperation::finished, this, [this](KScreen::ConfigOperation *operation) {
            if (operation->hasError() || !operation->config()) {
                reply("rollback-failed", operation->errorString()); QCoreApplication::exit(1); return;
            }
            auto restored = operation->config()->clone();
            for (const auto &current : restored->outputs()) {
                if (!current->isConnected()) continue;
                KScreen::OutputPtr old;
                for (const auto &candidate : snapshot->outputs())
                    if (candidate->isConnected() && candidate->name() == current->name()) old = candidate;
                if (!old) continue;
                QString mode = old->currentModeId();
                if (!current->modes().contains(mode)) {
                    mode.clear();
                    if (old->currentMode()) for (const auto &candidate : current->modes())
                        if (candidate->size() == old->currentMode()->size() && qAbs(candidate->refreshRate() - old->currentMode()->refreshRate()) < 0.1)
                            mode = candidate->id();
                    if (mode.isEmpty()) mode = current->preferredModeId();
                }
                if (old->isEnabled() && mode.isEmpty()) continue;
                current->setCurrentModeId(mode); current->setPos(old->pos()); current->setScale(old->scale());
                current->setRotation(old->rotation()); current->setEnabled(old->isEnabled()); current->setPriority(old->priority());
            }
            restored->adjustPriorities();
            if (!KScreen::Config::canBeApplied(restored, KScreen::Config::ValidityFlag::RequireAtLeastOneEnabledScreen)) {
                reply("rollback-failed", QStringLiteral("The monitor topology changed; the previous configuration is no longer usable."));
                QCoreApplication::exit(1); return;
            }
            auto *set = new KScreen::SetConfigOperation(restored, this);
            connect(set, &KScreen::ConfigOperation::finished, this, [this](KScreen::ConfigOperation *finished) {
                reply(finished->hasError() ? "rollback-failed" : "reverted", finished->errorString());
                QCoreApplication::exit(finished->hasError() ? 1 : 0);
            });
        });
    }
    QSocketNotifier input;
    QTimer deadline;
    QByteArray pending;
    QString token;
    KScreen::ConfigPtr snapshot;
    bool started = false, ready = false, applying = false, rollingBack = false, rollbackRequested = false;
};

#include <csignal>
int main(int argc, char **argv)
{
    std::signal(SIGPIPE, SIG_IGN);
    QCoreApplication app(argc, argv);
    DisplayTransaction transaction;
    return app.exec();
}
