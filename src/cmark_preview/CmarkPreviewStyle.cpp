#include "CmarkPreviewStyle.hpp"

#include <QTextBlock>
#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>

namespace
{
QString defaultStyleSheet()
{
    return QStringLiteral(
        "body { margin: 0; }"
        "p { margin-top: 0.55em; margin-bottom: 0.75em; }"
        "h1, h2, h3, h4, h5, h6 { margin-top: 0.9em; margin-bottom: 0.45em; }"
        "ul, ol { margin-top: 0.35em; margin-bottom: 0.65em; }"
        "blockquote { margin-left: 1.25em; margin-top: 0.65em; margin-bottom: 0.65em; }"
        "pre { margin-top: 0.75em; margin-bottom: 0.75em; padding: 0.5em; }"
        "code { font-family: monospace; }");
}
}

CmarkPreviewStyleOptions CmarkPreviewStyle::defaultOptions(const QFont &font)
{
    CmarkPreviewStyleOptions options;
    options.font = font;
    options.styleSheet = defaultStyleSheet();
    return options;
}

void CmarkPreviewStyle::prepareDocument(QTextDocument *document, const CmarkPreviewStyleOptions &options)
{
    if (!document)
        return;

    document->setDocumentMargin(options.documentMargin);
    document->setDefaultFont(options.font);
    document->setDefaultStyleSheet(options.styleSheet);
}

void CmarkPreviewStyle::applyLineHeight(QTextDocument *document, qreal lineHeight)
{
    if (!document || lineHeight <= 0.0)
        return;

    for (auto block = document->begin(); block.isValid(); block = block.next()) {
        auto format = block.blockFormat();
        format.setLineHeight(lineHeight * 100.0, QTextBlockFormat::ProportionalHeight);
        QTextCursor cursor(block);
        cursor.setBlockFormat(format);
    }
}
