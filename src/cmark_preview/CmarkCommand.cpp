#include "CmarkCommand.hpp"

#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

struct CmarkCommandPrivate {
    QString resolveExecutablePath() const
    {
        if (!configuredExecutable.isEmpty()) {
            if (QFileInfo(configuredExecutable).isAbsolute())
                return QFileInfo::exists(configuredExecutable) ? configuredExecutable : QString();
            return QStandardPaths::findExecutable(configuredExecutable);
        }

        // Prefer the standard cmark executable, while accepting cmark-gfm as
        // a drop-in implementation when cmark is not installed.
        if (auto executable = QStandardPaths::findExecutable(QStringLiteral("cmark")); !executable.isEmpty())
            return executable;
        return QStandardPaths::findExecutable(QStringLiteral("cmark-gfm"));
    }

    QString configuredExecutable;
    QProcess *process = nullptr;
    QByteArray standardOutput;
    QByteArray standardError;
    quint64 renderSerial = 0;
};

CmarkCommand::CmarkCommand(QObject *parent)
    : QObject(parent)
    , d(new CmarkCommandPrivate)
{
}

CmarkCommand::~CmarkCommand()
{
    if (d->process && d->process->state() != QProcess::NotRunning) {
        d->process->kill();
        d->process->waitForFinished();
    }
}

bool CmarkCommand::isAvailable() const
{
    return !d->resolveExecutablePath().isEmpty();
}

QString CmarkCommand::executablePath() const
{
    return d->configuredExecutable;
}

QString CmarkCommand::resolvedExecutablePath() const
{
    return d->resolveExecutablePath();
}

void CmarkCommand::setExecutablePath(const QString &path)
{
    d->configuredExecutable = path.trimmed();
}

void CmarkCommand::render(const QString &markdown)
{
    if (d->process) {
        d->process->disconnect(this);
        if (d->process->state() != QProcess::NotRunning) {
            d->process->kill();
            d->process->waitForFinished();
        }
        d->process->deleteLater();
        d->process = nullptr;
    }

    const auto executable = d->resolveExecutablePath();
    if (executable.isEmpty()) {
        emit renderFailed(tr("Neither cmark nor cmark-gfm could be found on PATH."));
        return;
    }

    const auto serial = ++d->renderSerial;
    auto process = new QProcess(this);
    d->process = process;
    d->standardOutput.clear();
    d->standardError.clear();

    connect(process, &QProcess::started, this, [process, input = markdown.toUtf8()] {
        process->write(input);
        process->closeWriteChannel();
    });
    connect(process, &QProcess::readyReadStandardOutput, this, [this, process] {
        if (d->process == process)
            d->standardOutput.append(process->readAllStandardOutput());
    });
    connect(process, &QProcess::readyReadStandardError, this, [this, process] {
        if (d->process == process)
            d->standardError.append(process->readAllStandardError());
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (d->process != process || error != QProcess::FailedToStart)
            return;

        emit renderFailed(tr("Failed to start %1.").arg(process->program()));
    });
    connect(process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this, [this, process, serial](int exitCode, QProcess::ExitStatus exitStatus) {
        if (d->process != process || serial != d->renderSerial)
            return;

        d->standardOutput.append(process->readAllStandardOutput());
        d->standardError.append(process->readAllStandardError());
        if (exitStatus == QProcess::NormalExit && exitCode == 0) {
            emit rendered(QString::fromUtf8(d->standardOutput));
        } else {
            auto message = QString::fromUtf8(d->standardError).trimmed();
            if (message.isEmpty())
                message = tr("cmark exited with code %1.").arg(exitCode);
            emit renderFailed(message);
        }

        process->deleteLater();
        d->process = nullptr;
    });

    process->start(executable, {QStringLiteral("--to"), QStringLiteral("html")}, QIODevice::ReadWrite);
}
