#include "RipgrepCommand.hpp"

#include <QException>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

struct SearchOptions {
    bool wholeWord = false;
    bool caseSensitive = false;
    bool useRegex = false;
    QStringList includeFiles;
    QStringList excludeFiles;
};

struct RipgrepCommandPrivate {
    QStringList buildArgs(const QString &term, const QString &dir, const QStringList &files);
    void parseMatch(const QByteArray &match);
    void search(const QString &term, const QString &dir, const QStringList &files);

    RipgrepCommand *q;
    QProcess *process = nullptr;
    SearchOptions options;
};

RipgrepCommand::RipgrepCommand(QObject *parent)
    : QObject(parent)
    , d(new RipgrepCommandPrivate)
{
    d->q = this;
}

RipgrepCommand::~RipgrepCommand() = default;

void RipgrepCommand::setWholeWord(bool newValue)
{
    d->options.wholeWord = newValue;
    emit searchOptionsChanged();
}

void RipgrepCommand::setCaseSensitive(bool newValue)
{
    d->options.caseSensitive = newValue;
    emit searchOptionsChanged();
}

void RipgrepCommand::setUseRegex(bool newValue)
{
    d->options.useRegex = newValue;
    emit searchOptionsChanged();
}

void RipgrepCommand::setIncludeFiles(const QStringList &files)
{
    d->options.includeFiles = files;
}

void RipgrepCommand::setExcludeFiles(const QStringList &files)
{
    d->options.excludeFiles = files;
}

void RipgrepCommandPrivate::search(const QString &term, const QString &dir, const QStringList &files)
{
    QStringList args;
    if (options.wholeWord)
        args << "--word-regexp";
    if (options.caseSensitive)
        args << "--case-sensitive";
    else
        args << "--ignore-case";
    if (!options.useRegex)
        args << "--fixed-strings";
    for (const auto &file : options.includeFiles)
        args << "--glob" << file;
    for (const auto &file : options.excludeFiles)
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

    if (process != nullptr) {
        if (process->state() != QProcess::NotRunning) {
            process->terminate();
            process->waitForFinished();
        }
        process->deleteLater();
    }
    process = new QProcess(q);
    q->connect(process, &QProcess::readyReadStandardOutput, q, [this] {
        while (process->canReadLine()) {
            auto line = process->readLine();
            parseMatch(line.trimmed());
        }
    });
    process->start("rg", args, QIODevice::ReadOnly);
}

void RipgrepCommand::searchInDir(const QString &term, const QString &dir)
{
    d->search(term, dir, {});
}

void RipgrepCommand::searchInFiles(const QString &term, const QStringList &files)
{
    d->search(term, QString(), files);
}

struct JsonResolutionError : public QException {
    QString message;
    JsonResolutionError(const QString &message)
        : message(message)
    {
    }
};

// Ripgrep reports submatch offsets as UTF-8 byte offsets, but KTextEditor
// cursors and ranges use character (UTF-16 code unit) offsets. These diverge
// whenever the line contains multi-byte characters such as CJK text, so we map
// a byte offset back to the corresponding character offset in the line.
static int byteOffsetToCharOffset(const QByteArray &utf8Line, int byteOffset)
{
    byteOffset = qBound(0, byteOffset, utf8Line.size());
    return QString::fromUtf8(utf8Line.constData(), byteOffset).size();
}

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

void RipgrepCommandPrivate::parseMatch(const QByteArray &match)
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
            emit q->matchFoundInFile(file);
        } else if (type == "match") {
            auto file = resolveJson(data, {"path", "text"}).toString();
            auto text = resolveJson(data, {"lines", "text"}).toString();
            auto utf8Line = text.toUtf8();
            auto line = resolveJson(data, {"line_number"}).toInt();
            auto submatches = resolveJson(data, {"submatches"}).toArray();
            for (auto v : submatches) {
                auto obj = v.toObject();
                int start = byteOffsetToCharOffset(utf8Line, resolveJson(obj, {"start"}).toInt());
                int end = byteOffsetToCharOffset(utf8Line, resolveJson(obj, {"end"}).toInt());
                emit q->matchFound(file, text, line, start, end);
            }
        } else if (type == "summary") {
            int found = resolveJson(data, {"stats", "matches"}).toInt();
            qint64 nanos = resolveJson(data, {"elapsed_total", "nanos"}).toInteger();
            emit q->searchFinished(found, nanos);
        }
    } catch (JsonResolutionError &err) {
        qWarning() << "JSON Parse Error:" << err.message;
    }
}
