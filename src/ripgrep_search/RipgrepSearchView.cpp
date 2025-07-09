#include "RipgrepSearchView.hpp"
#include "RipgrepSearchPlugin.hpp"

#include <KTextEditor/Editor>
#include <KTextEditor/MainWindow>
#include <QVBoxLayout>
#include <QVariantMap>
#include <QWidget>
#include <QLineEdit>
#include <QProcess>
#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSizePolicy>
#include <QMessageBox>
#include <qprocess.h>

#define L(literal) QStringLiteral(literal)
#define THEME_ICON(name) QIcon::fromTheme(QStringLiteral(name))

void RipgrepSearchView::showMessage(const QString &msg)
{
    QVariantMap map{{L("category"), L("Ripgrep Search")},
                    {L("categoryIcon"), THEME_ICON("search")},
                    {L("type"), L("Log")},
                    {L("text"), msg}};
    QMetaObject::invokeMethod(m_mainWindow->parent(), "showMessage", Qt::DirectConnection, Q_ARG(QVariantMap, map));
}

RipgrepSearchView::RipgrepSearchView(RipgrepSearchPlugin *plugin, KTextEditor::MainWindow *mainWindow)
    : QObject(plugin)
    , m_plugin(plugin)
    , m_mainWindow(mainWindow)
{
    setupUi();
    connectSignals();
}

void RipgrepSearchView::setupUi()
{
    m_toolView = m_mainWindow->createToolView(m_plugin, "RipgrepSearchPlugin", KTextEditor::MainWindow::Left, THEME_ICON("search"), tr("Ripgrep Search"));
    auto layout = m_toolView->layout();

    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText(tr("Search"));
    layout->addWidget(m_searchEdit);
    
    m_startButton = new QPushButton(THEME_ICON("search"), "Search");
    layout->addWidget(m_startButton);
    
    m_searchResults = new QTreeWidget();
    m_searchResults->setHeaderHidden(true);
    m_searchResults->setSizePolicy(QSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding));
    layout->addWidget(m_searchResults);
}

void RipgrepSearchView::connectSignals()
{
    auto app = KTextEditor::Editor::instance()->application();
    connect(m_startButton, &QPushButton::clicked, [this] {
        m_searchResults->clear();
        launchSearch();
    });
}

static inline QString jsonPath(const QJsonDocument &json)
{
    return json.object()
        .value("data").toObject()
        .value("path").toObject()
        .value("text").toString();
}

static inline QString jsonMatchedLines(const QJsonDocument &json)
{
    return json.object()
        .value("data").toObject()
        .value("lines").toObject()
        .value("text").toString();
}

static inline int jsonLineNumber(const QJsonDocument &json)
{
    return json.object()
        .value("data").toObject()
        .value("line_number").toInt();
}

QString RipgrepSearchView::workingDir()
{
    if (auto projectPlugin = m_mainWindow->pluginView(L("kateprojectplugin"))) {
        return projectPlugin->property("projectBaseDir").toString();
    }
    qWarning() << "No project..?";
    return "";
}

void RipgrepSearchView::launchSearch()
{
    auto editor = KTextEditor::Editor::instance();
    auto cwd = workingDir();
    if (cwd.isEmpty()) {
        return;
    }

    auto rg = new QProcess(this);
    rg->setWorkingDirectory(cwd);

    connect(rg, &QProcess::finished, [this, rg](int exitCode, QProcess::ExitStatus status) {
        if (status != QProcess::NormalExit) {
            QMessageBox::critical(m_toolView, tr("Error"), tr("Ripgrep returned %1").arg(exitCode));
        }
        rg->deleteLater();
    });
    
    connect(rg, &QProcess::started, [this] {
        QMessageBox::information(m_toolView, tr("Message"), tr("Process started"));
    });
    
    connect(rg, &QProcess::readyReadStandardError, [this, rg] {
        auto error = rg->readAllStandardError();
        QMessageBox::critical(m_toolView, tr("Error"), tr("Ripgrep error: %1").arg(error));
    });

    connect(rg, &QProcess::readyReadStandardOutput, [this, rg] {
        auto lines = rg->readAllStandardOutput().split('\n');
        for (const auto &line : lines) {
            auto trimmed = line.trimmed();
            QMessageBox::information(m_toolView, tr("Message"), tr("JSON Line: %1").arg(trimmed));

            if (trimmed.isEmpty())
                continue;
            QJsonParseError err;
            auto json = QJsonDocument::fromJson(trimmed, &err);
            if (err.error != QJsonParseError::NoError) {
                qWarning() << "JSON parse error:" << err.errorString();
                continue;
            }
            auto type = json.object().value("type").toString();
            if (type == "begin") {
                m_currentItem = new QTreeWidgetItem();
                m_searchResults->addTopLevelItem(m_currentItem);
                QString fileName = jsonPath(json);
                m_currentItem->setText(0, fileName);
                m_currentItem->setData(0, FileNameRole, fileName);
                m_currentItem->setData(0, LineNumberRole, 0);
            } else if (type == "match") {
                auto resultItem = new QTreeWidgetItem();
                m_currentItem->addChild(resultItem);
                QString fileName = jsonPath(json);
                QString matched = jsonMatchedLines(json);
                int lineNumber = jsonLineNumber(json);
                resultItem->setText(0, matched);
                resultItem->setData(0, FileNameRole, fileName);
                resultItem->setData(0, LineNumberRole, lineNumber);
            }
        }
    });

    QStringList args;
    args << "-u" << "--json" << "--trim" << "--" << m_searchEdit->text();
    rg->start("/usr/bin/rg", args);
}
