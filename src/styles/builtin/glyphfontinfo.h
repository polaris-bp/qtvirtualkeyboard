// Copyright (C) 2024 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

#ifndef GLYPHFONTINFO_H
#define GLYPHFONTINFO_H

#include <QtCore/QObject>
#include <QtGui/QFont>
#include <QtQml/qqml.h>

QT_BEGIN_NAMESPACE

class GlyphFontInfo : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(GlyphFontInfo)
    QML_ADDED_IN_VERSION(6, 1)

public:
    explicit GlyphFontInfo(QObject *parent = nullptr);

    // Returns a list of "char:fontFamily" for each character in text
    Q_INVOKABLE QStringList resolve(const QString &text, const QFont &font) const;

    // Returns a formatted summary string: "A[Arial] B[Noto Sans]..."
    Q_INVOKABLE QString resolveSummary(const QString &text, const QFont &font) const;
};

QT_END_NAMESPACE

#endif // GLYPHFONTINFO_H
