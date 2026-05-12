#include "RipgrepCommand.hpp"

#include <QException>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

RipgrepCommand::RipgrepCommand(QObject *parent)
    : QObject(parent)
{
}

void RipgrepCommand::setWholeWord(bool newValue)
{
    m_options.wholeWord = newValue;
    emit searchOptionsChanged();
}

void RipgrepCommand::setCaseSensitive(bool newValue)
{
    m_options.caseSensitive = newValue;
    emit searchOptionsChanged();
}

void RipgrepCommand::setUseRegex(bool newValue)
{
    m_options.useRegex = newValue;
    emit searchOptionsChanged();
}

void RipgrepCommand::setIncludeFiles(const QStringList &files)
{
    m_options.includeFiles = files;
}

void RipgrepCommand::setExcludeFiles(const QStringList &files)
{
    m_options.excludeFiles = files;
}

void RipgrepCommand::processReadOutput()
{
    while (m_process->canReadLine()) {
        auto line = m_process->readLine();
        parseMatch(line.trimmed());
    }
}

void RipgrepCommand::search(const QString &term, const QString &dir, const QStringList &files)
{
    QStringList args;
    if (m_options.wholeWord)
        args << "--word-regexp";
    if (m_options.caseSensitive)
        args << "--case-sensitive";
    else
        args << "--ignore-case";
    if (!m_options.useRegex)
        args << "--fixed-strings";
    for (const auto &file : m_options.includeFiles)
        args << "--glob" << file;
    for (const auto &file : m_options.excludeFiles)
        args << "--glob" << QString("!%1").arg(file);
    args << "--json" << "--regexp" << term;

    if (!dir.isEmpty()) {
        qInfo() << "[ripgrep] Searching in directory:" << dir;
        args << dir;
    } else if (!files.isEmpty()) {
        qInfo() << "[ripgrep] Searching in file:" << files;
        for (const auto &file : files)
            args << file;
    } else {
        qInfo() << "[ripgrep] Nothing to search; abort searching";
        return;
    }

    if (m_process != nullptr) {
        if (m_process->state() != QProcess::NotRunning) {
            m_process->terminate();
            m_process->waitForFinished();
        }
        m_process->deleteLater();
    }
    m_process = new QProcess(this);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &RipgrepCommand::processReadOutput);
    m_process->start("rg", args, QIODevice::ReadOnly);
}

void RipgrepCommand::searchInDir(const QString &term, const QString &dir)
{
    search(term, dir, {});
}

void RipgrepCommand::searchInFiles(const QString &term, const QStringList &files)
{
    search(term, QString(), files);
}

struct JsonResolutionError : public QException {
    QString message;
    JsonResolutionError(const QString &message)
        : message(message)
    {
    }
};

static QJsonValue resolveJson(const QJsonObject &root, const QStringList &args)
{
    Q_ASSERT(args.size() > 0);
    QJsonObject obj = root;
    QJsonValue value;
    for (const auto &key : args) {
        value = obj.value(key);
        if (value == QJsonValue::Undefined)
            throw JsonResolutionError(QStringLiteral("%1 is undefined").arg(key));
        if (&key != &args.back())
            obj = value.toObject();
    }
    return value;
}

void RipgrepCommand::parseMatch(const QByteArray &match)
{
    if (match.isEmpty())
        return;
    try {
        QJsonParseError err;
        auto json = QJsonDocument::fromJson(match, &err);
        if (err.error != QJsonParseError::NoError)
            throw JsonResolutionError(err.errorString());
        auto root = json.object();
        auto type = resolveJson(root, {"type"}).toString();
        auto data = resolveJson(root, {"data"}).toObject();
        if (type == "begin") {
            auto file = resolveJson(data, {"path", "text"}).toString();
            emit matchFoundInFile(file);
        } else if (type == "match") {
            auto file = resolveJson(data, {"path", "text"}).toString();
            auto text = resolveJson(data, {"lines", "text"}).toString();
            auto line = resolveJson(data, {"line_number"}).toInt();
            auto submatches = resolveJson(data, {"submatches"}).toArray();
            for (auto v : submatches) {
                auto obj = v.toObject();
                int start = resolveJson(obj, {"start"}).toInt();
                int end = resolveJson(obj, {"end"}).toInt();
                emit matchFound(file, text, line, start, end);
            }
        } else if (type == "summary") {
            int found = resolveJson(data, {"stats", "matches"}).toInt();
            qint64 nanos = resolveJson(data, {"elapsed_total", "nanos"}).toInteger();
            emit searchFinished(found, nanos);
        }
    } catch (JsonResolutionError &err) {
        qWarning() << "JSON Parse Error:" << err.message;
    }
}
