#pragma once

#include <QObject>

#include <QScopedPointer>
#include <QString>

class CmarkCommandPrivate;

class CmarkCommand : public QObject
{
    Q_OBJECT
public:
    explicit CmarkCommand(QObject *parent = nullptr);
    ~CmarkCommand() override;

    bool isAvailable() const;

    // This is intentionally exposed before a configuration page exists, so a
    // future config page can choose a command without changing the renderer API.
    QString executablePath() const;
    QString resolvedExecutablePath() const;

public slots:
    void render(const QString &markdown);
    void setExecutablePath(const QString &path);

signals:
    void rendered(const QString &html);
    void renderFailed(const QString &message);

private:
    const QScopedPointer<CmarkCommandPrivate> d;
};
