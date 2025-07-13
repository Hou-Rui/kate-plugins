#include "RipgrepCommand.hpp"

#include <QException>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

#include <initializer_list>

RipgrepCommand::RipgrepCommand(QObject *parent)
    : QProcess(parent)
{
    connect(this, &RipgrepCommand::readyReadStandardOutput, [this] {
        auto lines = readAllStandardOutput().split('\n');
        for (const auto &line : lines) {
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
    args << "--json" << "-e" << term;
    if (wholeWord())
        args << "-x";
    args << workingDirectory();
    start("rg", args, QIODevice::ReadOnly);
}

struct JsonResolutionError : public QException {
    QString message;
    JsonResolutionError(const QString &message)
        : message(message)
    {
    }
};

static QJsonValue resolveJson(const QJsonDocument &json, const std::initializer_list<QString> &args)
{
    Q_ASSERT(args.size() > 0);
    auto obj = json.object();
    for (auto it = args.begin(); it + 1 != args.end(); it++) {
        auto value = obj.value(*it);
        if (value == QJsonValue::Undefined) {
            throw JsonResolutionError("is undefined");
        }
        obj = value.toObject();
    }
    return obj.value(args.end()[-1]);
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