#pragma once

#include "CsvCodec.h"
#include <QRectF>

// Positioned text keeps the statement amount separate from numbers in its description.
class PdfStatementReader final
{
public:
    struct Word { QString text; QRectF bounds; };
    using Page = QVector<Word>;
    static CsvCodec::ReadResult read(const QString& path);
    static CsvCodec::ReadResult parsePages(const QVector<Page>& pages);
};
