#pragma once

#include <QObject>
#include <QProcess>

class RipgrepCommand : public QObject
{
    Q_OBJECT
public:
    explicit RipgrepCommand(QObject *parent);

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
    void searchFinished(int found, qint64 nanos);
    void searchOptionsChanged();

private slots:
    void processReadOutput();

private:
    QStringList buildArgs(const QString &term, const QString &dir, const QStringList &files);
    void parseMatch(const QByteArray &match);
    void search(const QString &term, const QString &dir, const QStringList &files);

    QProcess *m_process = nullptr;
    struct SearchOptions {
        bool wholeWord = false;
        bool caseSensitive = false;
        bool useRegex = false;
        QStringList includeFiles;
        QStringList excludeFiles;
    } m_options;
};
