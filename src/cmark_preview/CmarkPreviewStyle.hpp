#pragma once

#include <QFont>
#include <QString>

class QTextDocument;

struct CmarkPreviewStyleOptions {
    qreal documentMargin = 18.0;
    qreal lineHeight = 1.45;
    QFont font;
    QString styleSheet;
};

namespace CmarkPreviewStyle
{
CmarkPreviewStyleOptions defaultOptions(const QFont &font);
void prepareDocument(QTextDocument *document, const CmarkPreviewStyleOptions &options);
void applyLineHeight(QTextDocument *document, qreal lineHeight);
}
