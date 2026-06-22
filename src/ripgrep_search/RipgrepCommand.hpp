#pragma once
#include <QObject>
#include <QProcess>

class RipgrepCommandPrivate;

class RipgrepCommand : public QObject
{
    Q_OBJECT
public:
    explicit RipgrepCommand(QObject *parent);
    ~RipgrepCommand();

public slots:
    void searchInDir(const QString &term, const QString &dir);
    void searchInFiles(const QString &term, const QStringList &files);

    void setWholeWord(bool newValue);
    void setCaseSensitive(bool newValue);
    void setUseRegex(bool newValue);
    void setIncludeFiles(const QStringList &files);
    void setExcludeFiles(const QStringList &files);

signals:
    void matchFoundInFile(const QString &file);
    // line/start/end describe ripgrep's view of the match (used only for the
    // result row's text and tooltip). byteStart/byteEnd are absolute UTF-8 byte
    // offsets into the file; they are the source of truth for navigation, since
    // ripgrep and Kate disagree on line boundaries when a lone '\r' is present.
    void matchFound(const QString &file, const QString &text, int line, int start, int end, qint64 byteStart, qint64 byteEnd);
    void searchFinished(int found, qint64 nanos);
    void searchOptionsChanged();

private:
    const QScopedPointer<RipgrepCommandPrivate> d;
};
