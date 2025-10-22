#pragma once

#include <QObject>
#include <QProcess>

class RipgrepCommand : public QProcess
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
    void matchFound(const QString &file, const QString &text, int line, int start, int end);
    void searchFinished(int found, int nanos);
    void searchOptionsChanged();

private:
    using QProcess::setArguments;
    using QProcess::setProgram;
    using QProcess::start;
    using QProcess::startCommand;

    void ensureStopped();
    void parseMatch(const QByteArray &match);
    void search(const QString &term, const QString &dir, const QStringList &files);

    struct SearchOptions {
        bool wholeWord = false;
        bool caseSensitive = false;
        bool useRegex = false;
        QStringList includeFiles;
        QStringList excludeFiles;
    } m_options;
};
