#pragma once

#include <QObject>
#include <QProcess>

class RipgrepCommand : public QProcess
{
    Q_OBJECT
    Q_PROPERTY(bool wholeWord READ wholeWord WRITE setWholeWord NOTIFY searchOptionsChanged)

public:
    explicit RipgrepCommand(QObject *parent);
    ~RipgrepCommand();
    void search(const QString &term);

    bool wholeWord() const;
    void setWholeWord(bool newWholeWord);

    struct Result {
        QString fileName;
        int lineNumber;
        QString line;
    };

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
};