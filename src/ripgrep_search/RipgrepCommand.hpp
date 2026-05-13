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
    void matchFound(const QString &file, const QString &text, int line, int start, int end);
    void searchFinished(int found, qint64 nanos);
    void searchOptionsChanged();
    
private:
    const QScopedPointer<RipgrepCommandPrivate> d;
};
