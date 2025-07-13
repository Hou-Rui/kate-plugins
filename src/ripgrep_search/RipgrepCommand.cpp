#include "RipgrepCommand.hpp"

#include <QException>
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

bool RipgrepCommand::wholeWord() const
{
    return m_wholeWord;
}

void RipgrepCommand::setWholeWord(bool newValue)
{
    m_wholeWord = newValue;
    emit searchOptionsChanged();
}

bool RipgrepCommand::caseSensitive() const
{
    return m_caseSensitive;
}

void RipgrepCommand::setCaseSensitive(bool newValue)
{
    m_caseSensitive = newValue;
    emit searchOptionsChanged();
}

void RipgrepCommand::ensureStopped()
{
    if (state() != NotRunning)
        kill();
}

void RipgrepCommand::search(const QString &term)
{
    ensureStopped();
    QStringList args;
    if (wholeWord())
        args << "--word-regexp";
    if (caseSensitive())
        args << "--case-sensitive";
    else
        args << "--ignore-case";
    args << "--fixed-strings";
    args << "--json" << "--regexp" << term << workingDirectory();
    start("rg", args, QIODevice::ReadOnly);
}

struct JsonResolutionError : public QException {
    QString message;
    JsonResolutionError(const QString &message)
        : message(message)
    {
    }
};

static QJsonValue resolveJson(const QJsonDocument &json, const QList<QString> &args)
{
    Q_ASSERT(args.size() > 0);
    auto obj = json.object();
    QJsonValue value;
    for (auto it = args.begin(); it != args.end(); it++) {
        value = obj.value(*it);
        if (value == QJsonValue::Undefined)
            throw JsonResolutionError(QString("%1 is undefined").arg(*it));
        if (it + 1 != args.end())
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
        auto type = resolveJson(json, {"type"}).toString();
        if (type == "begin") {
            auto file = resolveJson(json, {"data", "path", "text"}).toString();
            emit matchFoundInFile(file);
        } else if (type == "match") {
            auto file = resolveJson(json, {"data", "path", "text"}).toString();
            auto matchedLine = resolveJson(json, {"data", "lines", "text"}).toString().trimmed();
            auto lineNumber = resolveJson(json, {"data", "line_number"}).toInt();
            emit matchFound({file, lineNumber, matchedLine});
        }
    } catch (JsonResolutionError &err) {
        qFatal() << "JSON Parse Error:" << err.message;
    }
}