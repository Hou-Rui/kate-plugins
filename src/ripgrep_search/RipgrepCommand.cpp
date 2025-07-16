#include "RipgrepCommand.hpp"

#include <QException>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

RipgrepCommand::RipgrepCommand(QObject *parent)
    : QProcess(parent)
{
    connect(this, &RipgrepCommand::readyReadStandardOutput, [this] {
        while (canReadLine()) {
            auto line = readLine();
            parseMatch(line.trimmed());
        }
    });
}

RipgrepCommand::~RipgrepCommand()
{
    ensureStopped();
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

void RipgrepCommand::ensureStopped()
{
    if (state() != NotRunning)
        kill();
}

void RipgrepCommand::search(const QString &term, const QString &dir, const QList<QString> &files)
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
    args << "--json" << "--regexp" << term;

    if (!dir.isEmpty()) {
        args << dir;
    } else if (!files.isEmpty()) {
        for (const auto &file : files)
            args << file;
    } else {
        return;
    }

    ensureStopped();
    start("rg", args, QIODevice::ReadOnly);
}

void RipgrepCommand::searchInDir(const QString &term, const QString &dir)
{
    search(term, dir, {});
}

void RipgrepCommand::searchInFiles(const QString &term, const QList<QString> &files)
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

static QJsonValue resolveJson(const QJsonObject &root, const QList<QString> &args)
{
    Q_ASSERT(args.size() > 0);
    QJsonObject obj = root;
    QJsonValue value;
    for (const auto &key : args) {
        value = obj.value(key);
        if (value == QJsonValue::Undefined)
            throw JsonResolutionError(QString("%1 is undefined").arg(key));
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
        if (type == "begin") {
            auto file = resolveJson(root, {"data", "path", "text"}).toString();
            emit matchFoundInFile(file);
        } else if (type == "match") {
            Result result;
            result.fileName = resolveJson(root, {"data", "path", "text"}).toString();
            result.line = resolveJson(root, {"data", "lines", "text"}).toString();
            result.lineNumber = resolveJson(root, {"data", "line_number"}).toInt();
            auto submatches = resolveJson(root, {"data", "submatches"}).toArray();
            for (auto v : submatches) {
                auto obj = v.toObject();
                int start = resolveJson(obj, {"start"}).toInt();
                int end = resolveJson(obj, {"end"}).toInt();
                result.submatches.append({start, end});
            }
            emit matchFound(result);
        }
    } catch (JsonResolutionError &err) {
        qFatal() << "JSON Parse Error:" << err.message;
    }
}