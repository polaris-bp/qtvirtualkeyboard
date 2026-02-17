// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#include "glyphfontinfo.h"
#include <QtGui/QTextLayout>
#include <QtGui/QGlyphRun>
#include <QtGui/QRawFont>

QT_BEGIN_NAMESPACE

GlyphFontInfo::GlyphFontInfo(QObject *parent)
    : QObject(parent)
{
}

QStringList GlyphFontInfo::resolve(const QString &text, const QFont &font) const
{
    if (text.isEmpty())
        return {};

    QTextLayout layout(text, font);
    layout.beginLayout();
    QTextLine line = layout.createLine();
    if (line.isValid())
        line.setLineWidth(1e6);
    layout.endLayout();

    // Map: character index -> font family
    QHash<int, QString> indexToFamily;

    const auto glyphRuns = layout.glyphRuns(-1, QTextLayout::RetrieveAll);
    for (const QGlyphRun &run : glyphRuns) {
        const QRawFont rawFont = run.rawFont();
        const QString family = rawFont.familyName();
        const auto indices = run.stringIndexes();
        for (int idx : indices) {
            if (idx >= 0 && idx < text.length())
                indexToFamily.insert(idx, family);
        }
    }

    QStringList result;
    result.reserve(text.length());
    for (int i = 0; i < text.length(); ++i) {
        const QString family = indexToFamily.value(i, QStringLiteral("(unknown)"));
        result.append(family);
    }

    return result;
}

QString GlyphFontInfo::resolveSummary(const QString &text, const QFont &font) const
{
    if (text.isEmpty())
        return {};

    const QStringList families = resolve(text, font);
    QStringList parts;
    parts.reserve(families.size());
    for (int i = 0; i < text.length() && i < families.size(); ++i) {
        QChar ch = text.at(i);
        parts.append(QStringLiteral("%1[%2]").arg(ch).arg(families.at(i)));
    }
    return parts.join(QStringLiteral(" "));
}

QT_END_NAMESPACE
