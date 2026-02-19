// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#ifndef FONTCHECKER_H
#define FONTCHECKER_H

#include <QObject>
#include <QFont>
#include <QVariantList>
#include <QVariantMap>
#include <QTextLayout>
#include <QGlyphRun>
#include <QRawFont>

class FontChecker : public QObject {
    Q_OBJECT
public:
    explicit FontChecker(QObject *parent = nullptr) : QObject(parent) {}

    Q_INVOKABLE QVariantList checkGlyphFonts(const QString &text, const QFont &font) {
        QVariantList result;
        QTextLayout layout(text, font);
        layout.beginLayout();
        QTextLine line = layout.createLine();
        if (line.isValid())
            line.setLineWidth(100000);
        layout.endLayout();

        for (const QGlyphRun &run : layout.glyphRuns()) {
            QVariantMap entry;
            entry["font"] = run.rawFont().familyName();
            entry["glyphCount"] = run.glyphIndexes().size();
            result.append(entry);
        }
        return result;
    }
};

#endif // FONTCHECKER_H
