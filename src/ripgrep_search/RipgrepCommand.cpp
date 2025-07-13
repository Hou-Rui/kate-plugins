#include "RipgrepCommand.hpp"

#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>

#include <initializer_list>
#include <type_traits>

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

template <typename T>
static std::optional<T> parseJson(const QJsonDocument &json, const std::initializer_list<QString> &args)
{
    Q_ASSERT(args.size() > 0);
    auto obj = json.object();
    for (auto it = args.begin(); it + 1 != args.end(); it++) {
        auto value = obj.value(*it);
        if (value == QJsonValue::Undefined)
            return {};
        obj = value.toObject();
    }
    auto last = obj.value(args.end()[-1]);
    if constexpr (std::is_integral_v<T>) {
        return last.toInt();
    } else if constexpr (std::is_assignable_v<T, QString>) {
        return last.toString().trimmed();
    } else {
        static_assert(false, "Unsupported JSON type");
    }
}

void RipgrepCommand::parseMatch(const QByteArray &match)
{
    if (match.isEmpty())
        return;

    QJsonParseError err;
    auto json = QJsonDocument::fromJson(match, &err);
    if (err.error != QJsonParseError::NoError) {
        qFatal() << "JSON parse error:" << err.errorString();
        return;
    }

    auto type = parseJson<QString>(json, {"type"});
    auto file = parseJson<QString>(json, {"data", "path", "text"});
    if (*type == "begin") {
        Q_ASSERT(file);
        emit matchFoundInFile(*file);
    } else if (*type == "match") {
        auto matchedLine = parseJson<QString>(json, {"data", "lines", "text"});
        auto lineNumber = parseJson<int>(json, {"data", "line_number"});
        Q_ASSERT(matchedLine && lineNumber);
        emit matchFound({*file, *lineNumber, *matchedLine});
    }
}