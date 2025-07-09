#pragma once

#include <QList>
#include <QVariant>

#include <KTextEditor/MainWindow>
#include <KTextEditor/Plugin>

class RipgrepSearchPlugin : public KTextEditor::Plugin
{
    Q_OBJECT
public:
    explicit RipgrepSearchPlugin(QObject *parent = nullptr, const QList<QVariant> & = QList<QVariant>());
    QObject *createView(KTextEditor::MainWindow *mainWindow) override;
};
