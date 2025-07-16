#pragma once

#include <QObject>
#include <QProcess>

class RipgrepCommand : public QProcess
{
    Q_OBJECT
    Q_PROPERTY(bool wholeWord READ wholeWord WRITE setWholeWord NOTIFY searchOptionsChanged)
    Q_PROPERTY(bool caseSensitive READ caseSensitive WRITE setCaseSensitive NOTIFY searchOptionsChanged)
    Q_PROPERTY(bool useRegex READ useRegex WRITE setUseRegex NOTIFY searchOptionsChanged)

public:
    struct Result {
        struct Submatch {
            int start;
            int end;
        };
        QString fileName;
        int lineNumber;
        QString line;
        QList<Submatch> submatches;
    };

    explicit RipgrepCommand(QObject *parent);
    ~RipgrepCommand();
    bool wholeWord() const;
    bool caseSensitive() const;
    bool useRegex() const;

public slots:
    void searchInDir(const QString &term, const QString &dir);
    void searchInFiles(const QString &term, const QList<QString> &files);
    void setCaseSensitive(bool newValue);
    void setWholeWord(bool newValue);
    void setUseRegex(bool newValue);

signals:
    void matchFoundInFile(const QString &path);
    void matchFound(Result result);
    void searchOptionsChanged();

private:
    using QProcess::setArguments;
    using QProcess::setProgram;
    using QProcess::start;
    using QProcess::startCommand;

    void ensureStopped();
    void parseMatch(const QByteArray &match);
    void search(const QString &term, const QString &dir, const QList<QString> &files);

    bool m_wholeWord = false;
    bool m_caseSensitive = false;
    bool m_useRegex = false;
};