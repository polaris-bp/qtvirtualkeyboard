// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR BSD-3-Clause

#include <QQuickView>
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQmlContext>
#include "fontchecker.h"

int main(int argc, char *argv[])
{
    qputenv("QT_IM_MODULE", QByteArray("qtvirtualkeyboard"));

    QGuiApplication app(argc, argv);

    FontChecker fontChecker;

    QQuickView view;
    view.rootContext()->setContextProperty("fontChecker", &fontChecker);
    view.setSource(QUrl(QString("qrc:/%1").arg(MAIN_QML)));
    if (view.status() == QQuickView::Error)
        return -1;
    view.setResizeMode(QQuickView::SizeRootObjectToView);

    view.show();

    return app.exec();
}
