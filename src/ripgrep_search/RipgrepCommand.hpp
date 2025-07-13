#pragma once

#include <QObject>
#include <QProcess>

class RipgrepCommand : public QProcess
{
    Q_OBJECT
    Q_PROPERTY(bool wholeWord READ wholeWord WRITE setWholeWord NOTIFY searchOptionsChanged)

public:
    struct Result {
        QString fileName;
        int lineNumber;
        QString line;
    };

    explicit RipgrepCommand(QObject *parent);
    ~RipgrepCommand();
    bool wholeWord() const;
    bool caseSensitive() const;

public slots:
    void search(const QString &term);
    void setCaseSensitive(bool newValue);
    void setWholeWord(bool newValue);

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

    bool m_wholeWord = false;
    bool m_caseSensitive = false;
};