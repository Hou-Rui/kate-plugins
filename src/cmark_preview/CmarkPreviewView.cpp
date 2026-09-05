#include "CmarkPreviewView.hpp"

#include "CmarkCommand.hpp"
#include "CmarkPreviewPlugin.hpp"
#include "CmarkPreviewStyle.hpp"

#include <KTextEditor/Document>
#include <KTextEditor/MainWindow>
#include <KTextEditor/View>

#include <QIcon>
#include <QLabel>
#include <QLayout>
#include <QPointer>
#include <QTextBrowser>
#include <QTimer>
#include <QUrl>

using namespace Qt::Literals::StringLiterals;

class CmarkPreviewViewPrivate : public QObject
{
    Q_OBJECT
public slots:
    void setActiveView(KTextEditor::View *view);
    void scheduleRender();
    void renderDocument();

public:
    void setupUi();
    void connectSignals();
    void showMessage(const QString &message);
    void applyStyle();

    CmarkPreviewView *q = nullptr;
    CmarkPreviewPlugin *plugin = nullptr;
    KTextEditor::MainWindow *mainWindow = nullptr;
    QPointer<KTextEditor::Document> document;
    QMetaObject::Connection documentTextChangedConnection;
    QWidget *toolView = nullptr;
    QTextBrowser *preview = nullptr;
    QLabel *statusLabel = nullptr;
    CmarkCommand *renderer = nullptr;
    QTimer *renderTimer = nullptr;
    CmarkPreviewStyleOptions styleOptions;
};

CmarkPreviewView::CmarkPreviewView(CmarkPreviewPlugin *plugin, KTextEditor::MainWindow *mainWindow)
    : QObject(plugin)
    , d(new CmarkPreviewViewPrivate)
{
    d->q = this;
    d->plugin = plugin;
    d->mainWindow = mainWindow;
    d->renderer = new CmarkCommand(this);
    d->setupUi();
    d->connectSignals();
    d->setActiveView(mainWindow->activeView());
}

CmarkPreviewView::~CmarkPreviewView() = default;

void CmarkPreviewViewPrivate::setupUi()
{
    // clang-format off
    toolView = mainWindow->createToolView(plugin, u"CmarkPreviewPlugin"_s,
                                          KTextEditor::MainWindow::Right,
                                          QIcon::fromTheme(u"document-preview"_s),
                                          tr("Markdown Preview"));
    // clang-format on

    statusLabel = new QLabel(toolView);
    statusLabel->setMargin(6);
    statusLabel->setWordWrap(true);
    toolView->layout()->addWidget(statusLabel);

    preview = new QTextBrowser(toolView);
    preview->setOpenExternalLinks(true);
    preview->setOpenLinks(true);
    preview->setReadOnly(true);
    preview->setPlaceholderText(tr("Open a document to see its Markdown preview."));
    toolView->layout()->addWidget(preview);
    styleOptions = CmarkPreviewStyle::defaultOptions(preview->font());
    CmarkPreviewStyle::prepareDocument(preview->document(), styleOptions);

    renderTimer = new QTimer(this);
    renderTimer->setSingleShot(true);
    renderTimer->setInterval(100);

    if (!renderer->isAvailable()) {
        statusLabel->setText(tr("cmark is not available"));
        showMessage(tr("Install cmark or cmark-gfm and make sure it is available on PATH."));
    } else {
        statusLabel->setText(tr("Ready"));
    }
}

void CmarkPreviewViewPrivate::connectSignals()
{
    connect(mainWindow, &KTextEditor::MainWindow::viewChanged, this, &CmarkPreviewViewPrivate::setActiveView);
    connect(renderTimer, &QTimer::timeout, this, &CmarkPreviewViewPrivate::renderDocument);
    connect(renderer, &CmarkCommand::rendered, this, [this](const QString &html) {
        // A newer edit is waiting for the debounce timer. Do not briefly show
        // the older process result before rendering that newer document state.
        if (!document || renderTimer->isActive())
            return;

        preview->document()->setBaseUrl(document->url());
        CmarkPreviewStyle::prepareDocument(preview->document(), styleOptions);
        preview->setHtml(html);
        CmarkPreviewStyle::applyLineHeight(preview->document(), styleOptions.lineHeight);
        statusLabel->setText(tr("Preview updated: %1").arg(document->documentName()));
    });
    connect(renderer, &CmarkCommand::renderFailed, this, [this](const QString &message) {
        statusLabel->setText(tr("Preview failed"));
        showMessage(message);
    });
}

void CmarkPreviewViewPrivate::setActiveView(KTextEditor::View *view)
{
    if (documentTextChangedConnection)
        disconnect(documentTextChangedConnection);
    document = view ? view->document() : nullptr;

    if (!document) {
        renderTimer->stop();
        statusLabel->setText(tr("No active document"));
        showMessage(tr("Open a document to see its Markdown preview."));
        return;
    }

    documentTextChangedConnection = connect(document, &KTextEditor::Document::textChanged, this, &CmarkPreviewViewPrivate::scheduleRender);
    // Render immediately when switching documents so output from the previous
    // document cannot remain visible while the debounce timer is pending.
    renderTimer->stop();
    statusLabel->setText(tr("Rendering %1...").arg(document->documentName()));
    renderDocument();
}

void CmarkPreviewViewPrivate::scheduleRender()
{
    if (!document)
        return;
    if (!renderer->isAvailable()) {
        statusLabel->setText(tr("cmark is not available"));
        showMessage(tr("Install cmark or cmark-gfm and make sure it is available on PATH."));
        return;
    }

    statusLabel->setText(tr("Rendering %1...").arg(document->documentName()));
    renderTimer->start();
}

void CmarkPreviewViewPrivate::renderDocument()
{
    if (document && renderer->isAvailable())
        renderer->render(document->text());
}

void CmarkPreviewView::setStyleOptions(const CmarkPreviewStyleOptions &options)
{
    d->styleOptions = options;
    d->applyStyle();
}

void CmarkPreviewViewPrivate::applyStyle()
{
    CmarkPreviewStyle::prepareDocument(preview->document(), styleOptions);
    if (document) {
        renderTimer->stop();
        statusLabel->setText(tr("Rendering %1...").arg(document->documentName()));
        renderDocument();
    }
}

void CmarkPreviewViewPrivate::showMessage(const QString &message)
{
    preview->setPlainText(message);
}

#include "CmarkPreviewView.moc"
