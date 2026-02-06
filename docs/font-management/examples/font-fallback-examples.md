# Qt バージョン別フォールバック制御サンプル

各Qtバージョンでのフォールバック制御の実装例です。

## 目次

1. [Qt 5.12 - 基本的なフォールバック](#qt-512---基本的なフォールバック)
2. [Qt 5.13 - setFamilies()を使用](#qt-513---setfamiliesを使用)
3. [Qt 6.5 - アプリ全体のフォールバック設定](#qt-65---アプリ全体のフォールバック設定)
4. [完全なサンプルプロジェクト](#完全なサンプルプロジェクト)

---

## Qt 5.12 - 基本的なフォールバック

Qt 5.12では専用のフォールバックAPIがないため、手動で制御します。

### QML: FontLoaderのステータスによる手動制御

```qml
// Qt512FontFallback.qml
import QtQuick 2.12
import QtQuick.Window 2.12

Window {
    id: root
    width: 600
    height: 400
    visible: true
    title: "Qt 5.12 Font Fallback Example"

    // プライマリフォント
    FontLoader {
        id: primaryFont
        source: "qrc:/fonts/CustomFont.ttf"
    }

    // フォールバックフォント1
    FontLoader {
        id: fallbackFont1
        source: "qrc:/fonts/NotoSansJP-Regular.ttf"
    }

    // フォールバックフォント2
    FontLoader {
        id: fallbackFont2
        source: "qrc:/fonts/Arial.ttf"
    }

    // フォールバックロジック
    QtObject {
        id: fontManager

        // フォールバックチェーンを実装
        readonly property string resolvedFamily: {
            if (primaryFont.status === FontLoader.Ready) {
                return primaryFont.name
            }
            if (fallbackFont1.status === FontLoader.Ready) {
                console.log("Primary font failed, using fallback 1")
                return fallbackFont1.name
            }
            if (fallbackFont2.status === FontLoader.Ready) {
                console.log("Fallback 1 failed, using fallback 2")
                return fallbackFont2.name
            }
            console.log("All custom fonts failed, using system font")
            return "sans-serif"
        }

        // ロード完了を待つ
        readonly property bool isReady:
            primaryFont.status !== FontLoader.Loading &&
            fallbackFont1.status !== FontLoader.Loading &&
            fallbackFont2.status !== FontLoader.Loading
    }

    Column {
        anchors.centerIn: parent
        spacing: 20
        visible: fontManager.isReady

        Text {
            text: "Qt 5.12 フォールバック制御"
            font.family: fontManager.resolvedFamily
            font.pixelSize: 24
        }

        Text {
            text: "使用中のフォント: " + fontManager.resolvedFamily
            font.pixelSize: 14
            color: "gray"
        }

        Text {
            text: "日本語テスト: こんにちは世界"
            font.family: fontManager.resolvedFamily
            font.pixelSize: 20
        }
    }

    // ローディング表示
    Text {
        anchors.centerIn: parent
        text: "Loading fonts..."
        visible: !fontManager.isReady
    }
}
```

### C++: カンマ区切りとstyleHintによる制御

```cpp
// qt512_font_fallback.cpp
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QFontDatabase>
#include <QFont>
#include <QDebug>

class FontManager512 : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString primaryFamily READ primaryFamily NOTIFY fontsChanged)
    Q_PROPERTY(QString japaneseFamily READ japaneseFamily NOTIFY fontsChanged)

public:
    explicit FontManager512(QObject *parent = nullptr) : QObject(parent) {}

    void loadFonts()
    {
        // フォントをロード
        int id1 = QFontDatabase::addApplicationFont(":/fonts/CustomFont.ttf");
        int id2 = QFontDatabase::addApplicationFont(":/fonts/NotoSansJP-Regular.ttf");

        if (id1 != -1) {
            QFontDatabase db;
            m_primaryFamily = db.applicationFontFamilies(id1).value(0, "");
        }
        if (id2 != -1) {
            QFontDatabase db;
            m_japaneseFamily = db.applicationFontFamilies(id2).value(0, "");
        }

        emit fontsChanged();
    }

    QString primaryFamily() const { return m_primaryFamily; }
    QString japaneseFamily() const { return m_japaneseFamily; }

    // Qt 5.12でのフォールバック付きQFont作成
    Q_INVOKABLE QFont createFontWithFallback(const QString &primary,
                                              int pixelSize) const
    {
        QFont font;

        // カンマ区切りでフォールバックを指定
        // （プラットフォーム依存の動作）
        QString familyChain = primary;
        if (!m_japaneseFamily.isEmpty() && primary != m_japaneseFamily) {
            familyChain += ", " + m_japaneseFamily;
        }
        familyChain += ", Arial, sans-serif";

        font.setFamily(familyChain);
        font.setPixelSize(pixelSize);

        // styleHintも設定（追加のフォールバックヒント）
        font.setStyleHint(QFont::SansSerif);

        return font;
    }

signals:
    void fontsChanged();

private:
    QString m_primaryFamily = "sans-serif";
    QString m_japaneseFamily = "sans-serif";
};

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    FontManager512 fontManager;
    fontManager.loadFonts();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("FontManager", &fontManager);
    engine.load(QUrl("qrc:/main.qml"));

    return app.exec();
}

#include "qt512_font_fallback.moc"
```

---

## Qt 5.13 - setFamilies()を使用

Qt 5.13で追加された`setFamilies()`を使用して、より明確にフォールバックを指定します。

### QML: 複数フォントファミリーの指定

```qml
// Qt513FontFallback.qml
import QtQuick 2.13
import QtQuick.Window 2.13

Window {
    id: root
    width: 600
    height: 400
    visible: true
    title: "Qt 5.13 Font Fallback Example"

    // フォントローダー
    FontLoader {
        id: customFont
        source: "qrc:/fonts/CustomFont.ttf"
    }

    FontLoader {
        id: japaneseFont
        source: "qrc:/fonts/NotoSansJP-Regular.ttf"
    }

    // フォールバックチェーンをプロパティで定義
    QtObject {
        id: fontConfig

        // 利用可能なフォントファミリーのリストを構築
        readonly property var fallbackChain: {
            var chain = []
            if (customFont.status === FontLoader.Ready) {
                chain.push(customFont.name)
            }
            if (japaneseFont.status === FontLoader.Ready) {
                chain.push(japaneseFont.name)
            }
            chain.push("Arial")
            chain.push("sans-serif")
            return chain
        }

        // カンマ区切り文字列として提供
        readonly property string familyString: fallbackChain.join(", ")
    }

    Column {
        anchors.centerIn: parent
        spacing: 20

        Text {
            text: "Qt 5.13 setFamilies() サンプル"
            font.family: fontConfig.familyString
            font.pixelSize: 24
        }

        Text {
            text: "フォールバックチェーン:"
            font.pixelSize: 14
            color: "gray"
        }

        Repeater {
            model: fontConfig.fallbackChain
            Text {
                text: (index + 1) + ". " + modelData
                font.pixelSize: 12
                color: "gray"
                leftPadding: 20
            }
        }

        Text {
            text: "日本語テスト: こんにちは世界 🌍"
            font.family: fontConfig.familyString
            font.pixelSize: 20
        }
    }
}
```

### C++: QFont::setFamilies()の使用

```cpp
// qt513_font_fallback.cpp
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QFontDatabase>
#include <QFont>
#include <QDebug>

class FontManager513 : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList fallbackChain READ fallbackChain NOTIFY fontsChanged)

public:
    explicit FontManager513(QObject *parent = nullptr) : QObject(parent) {}

    void loadFonts()
    {
        // フォントをロード
        loadFont(":/fonts/CustomFont.ttf");
        loadFont(":/fonts/NotoSansJP-Regular.ttf");
        loadFont(":/fonts/NotoSansArabic-Regular.ttf");

        // システムフォントをフォールバックに追加
        m_fallbackChain.append("Arial");
        m_fallbackChain.append("sans-serif");

        qDebug() << "Fallback chain:" << m_fallbackChain;
        emit fontsChanged();
    }

    QStringList fallbackChain() const { return m_fallbackChain; }

    // Qt 5.13のsetFamilies()を使用したQFont作成
    Q_INVOKABLE QFont createFont(int pixelSize) const
    {
        QFont font;

        // Qt 5.13の新機能: setFamilies()
        font.setFamilies(m_fallbackChain);
        font.setPixelSize(pixelSize);

        return font;
    }

    // 特定の用途向けフォント（日本語優先）
    Q_INVOKABLE QFont createJapaneseFont(int pixelSize) const
    {
        QFont font;

        // 日本語フォントを先頭に
        QStringList japaneseChain;
        for (const QString &family : m_fallbackChain) {
            if (family.contains("JP") || family.contains("Japanese") ||
                family.contains("Gothic") || family.contains("Mincho")) {
                japaneseChain.prepend(family);
            } else {
                japaneseChain.append(family);
            }
        }

        font.setFamilies(japaneseChain);
        font.setPixelSize(pixelSize);

        return font;
    }

signals:
    void fontsChanged();

private:
    void loadFont(const QString &path)
    {
        int id = QFontDatabase::addApplicationFont(path);
        if (id != -1) {
            QFontDatabase db;
            QStringList families = db.applicationFontFamilies(id);
            for (const QString &family : families) {
                if (!m_fallbackChain.contains(family)) {
                    m_fallbackChain.append(family);
                }
            }
        }
    }

    QStringList m_fallbackChain;
};

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    FontManager513 fontManager;
    fontManager.loadFonts();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("FontManager", &fontManager);
    engine.load(QUrl("qrc:/main.qml"));

    return app.exec();
}

#include "qt513_font_fallback.moc"
```

---

## Qt 6.5 - アプリ全体のフォールバック設定

Qt 6.5の`setApplicationFallbackFontFamilies()`を使用して、スクリプト別にアプリ全体のフォールバックを設定します。

### QML: シンプルな使用（C++で設定済み）

```qml
// Qt65FontFallback.qml
import QtQuick
import QtQuick.Window
import QtQuick.Controls

Window {
    id: root
    width: 800
    height: 600
    visible: true
    title: "Qt 6.5 Application-wide Font Fallback"

    // フォントローダー（オプション、追加フォント用）
    FontLoader {
        id: customFont
        source: "qrc:/fonts/CustomFont.ttf"
    }

    Column {
        anchors.centerIn: parent
        spacing: 30

        // ヘッダー
        Text {
            text: "Qt 6.5 アプリ全体フォールバック"
            font.pixelSize: 28
            font.bold: true
        }

        // 説明
        Text {
            text: "setApplicationFallbackFontFamilies() により\n" +
                  "スクリプト別のフォールバックが自動適用されます"
            font.pixelSize: 14
            color: "gray"
        }

        // 各言語のテスト
        Grid {
            columns: 2
            spacing: 20
            verticalItemAlignment: Grid.AlignVCenter

            Text { text: "英語:"; font.pixelSize: 14; color: "gray" }
            Text {
                text: "Hello, World!"
                font.pixelSize: 20
                // フォールバックは自動適用
            }

            Text { text: "日本語:"; font.pixelSize: 14; color: "gray" }
            Text {
                text: "こんにちは、世界！"
                font.pixelSize: 20
                // Script_Hiragana/Katakana/Han のフォールバックが自動適用
            }

            Text { text: "中国語:"; font.pixelSize: 14; color: "gray" }
            Text {
                text: "你好，世界！"
                font.pixelSize: 20
                // Script_Han のフォールバックが自動適用
            }

            Text { text: "韓国語:"; font.pixelSize: 14; color: "gray" }
            Text {
                text: "안녕하세요, 세계!"
                font.pixelSize: 20
                // Script_Hangul のフォールバックが自動適用
            }

            Text { text: "アラビア語:"; font.pixelSize: 14; color: "gray" }
            Text {
                text: "مرحبا بالعالم"
                font.pixelSize: 20
                horizontalAlignment: Text.AlignRight
                // Script_Arabic のフォールバックが自動適用
            }

            Text { text: "混合テキスト:"; font.pixelSize: 14; color: "gray" }
            Text {
                text: "Hello こんにちは 你好 🌍"
                font.pixelSize: 20
                // 各スクリプトに応じたフォールバックが自動適用
            }
        }

        // フォールバック設定の表示
        Rectangle {
            width: 500
            height: 150
            color: "#f5f5f5"
            radius: 5

            Column {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 5

                Text {
                    text: "現在のフォールバック設定:"
                    font.bold: true
                    font.pixelSize: 14
                }

                Repeater {
                    model: FontManager.fallbackInfo
                    Text {
                        text: modelData
                        font.pixelSize: 12
                        font.family: "monospace"
                    }
                }
            }
        }
    }
}
```

### C++: setApplicationFallbackFontFamilies()の設定

```cpp
// qt65_font_fallback.cpp
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QFontDatabase>
#include <QFont>
#include <QDebug>

class FontManager65 : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList fallbackInfo READ fallbackInfo CONSTANT)

public:
    explicit FontManager65(QObject *parent = nullptr) : QObject(parent) {}

    void initialize()
    {
        // カスタムフォントをロード
        loadFont(":/fonts/Roboto-Regular.ttf");
        loadFont(":/fonts/NotoSansJP-Regular.ttf");
        loadFont(":/fonts/NotoSansSC-Regular.ttf");
        loadFont(":/fonts/NotoSansKR-Regular.ttf");
        loadFont(":/fonts/NotoSansArabic-Regular.ttf");

        // アプリ全体のフォールバックを設定
        setupApplicationFallbacks();
    }

    QStringList fallbackInfo() const { return m_fallbackInfo; }

private:
    void loadFont(const QString &path)
    {
        int id = QFontDatabase::addApplicationFont(path);
        if (id != -1) {
            QStringList families = QFontDatabase::applicationFontFamilies(id);
            qDebug() << "Loaded:" << path << "->" << families;
        }
    }

    void setupApplicationFallbacks()
    {
        // ========================================
        // Qt 6.5+ の新機能: アプリ全体のフォールバック設定
        // ========================================

        // 日本語（ひらがな）
        QFontDatabase::setApplicationFallbackFontFamilies(
            QChar::Script_Hiragana,
            {"Noto Sans JP", "Hiragino Sans", "Yu Gothic", "MS Gothic"}
        );
        m_fallbackInfo << "Hiragana: Noto Sans JP, Hiragino Sans, Yu Gothic";

        // 日本語（カタカナ）
        QFontDatabase::setApplicationFallbackFontFamilies(
            QChar::Script_Katakana,
            {"Noto Sans JP", "Hiragino Sans", "Yu Gothic", "MS Gothic"}
        );
        m_fallbackInfo << "Katakana: Noto Sans JP, Hiragino Sans, Yu Gothic";

        // 漢字（CJK共通）
        QFontDatabase::setApplicationFallbackFontFamilies(
            QChar::Script_Han,
            {"Noto Sans JP", "Noto Sans SC", "Noto Sans TC",
             "Microsoft YaHei", "SimSun", "MS Gothic"}
        );
        m_fallbackInfo << "Han: Noto Sans JP/SC/TC, Microsoft YaHei";

        // 韓国語（ハングル）
        QFontDatabase::setApplicationFallbackFontFamilies(
            QChar::Script_Hangul,
            {"Noto Sans KR", "Malgun Gothic", "Gulim"}
        );
        m_fallbackInfo << "Hangul: Noto Sans KR, Malgun Gothic";

        // アラビア語
        QFontDatabase::setApplicationFallbackFontFamilies(
            QChar::Script_Arabic,
            {"Noto Sans Arabic", "Arabic Typesetting", "Arial"}
        );
        m_fallbackInfo << "Arabic: Noto Sans Arabic, Arabic Typesetting";

        // キリル文字（ロシア語など）
        QFontDatabase::setApplicationFallbackFontFamilies(
            QChar::Script_Cyrillic,
            {"Noto Sans", "Arial", "Segoe UI"}
        );
        m_fallbackInfo << "Cyrillic: Noto Sans, Arial";

        // ギリシャ文字
        QFontDatabase::setApplicationFallbackFontFamilies(
            QChar::Script_Greek,
            {"Noto Sans", "Arial", "Segoe UI"}
        );
        m_fallbackInfo << "Greek: Noto Sans, Arial";

        // タイ文字
        QFontDatabase::setApplicationFallbackFontFamilies(
            QChar::Script_Thai,
            {"Noto Sans Thai", "Leelawadee UI", "Tahoma"}
        );
        m_fallbackInfo << "Thai: Noto Sans Thai, Leelawadee UI";

        // デーヴァナーガリー（ヒンディー語など）
        QFontDatabase::setApplicationFallbackFontFamilies(
            QChar::Script_Devanagari,
            {"Noto Sans Devanagari", "Mangal", "Arial Unicode MS"}
        );
        m_fallbackInfo << "Devanagari: Noto Sans Devanagari, Mangal";

        qDebug() << "Application-wide font fallbacks configured";
    }

    QStringList m_fallbackInfo;
};

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    // フォントマネージャーを初期化（QMLエンジン起動前）
    FontManager65 fontManager;
    fontManager.initialize();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("FontManager", &fontManager);
    engine.load(QUrl("qrc:/main.qml"));

    return app.exec();
}

#include "qt65_font_fallback.moc"
```

---

## 完全なサンプルプロジェクト

### CMakeLists.txt（Qt 6.5用）

```cmake
cmake_minimum_required(VERSION 3.16)
project(FontFallbackExample VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick Gui)

qt_add_executable(fontfallback
    main.cpp
)

qt_add_qml_module(fontfallback
    URI FontFallbackExample
    VERSION 1.0
    QML_FILES
        Main.qml
)

qt_add_resources(fontfallback "fonts"
    PREFIX "/fonts"
    FILES
        fonts/Roboto-Regular.ttf
        fonts/NotoSansJP-Regular.ttf
        fonts/NotoSansSC-Regular.ttf
        fonts/NotoSansKR-Regular.ttf
        fonts/NotoSansArabic-Regular.ttf
)

target_link_libraries(fontfallback PRIVATE
    Qt6::Quick
    Qt6::Gui
)
```

### CMakeLists.txt（Qt 5.12/5.13用）

```cmake
cmake_minimum_required(VERSION 3.5)
project(FontFallbackExample VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 REQUIRED COMPONENTS Quick Gui)

add_executable(fontfallback
    main.cpp
    resources.qrc
)

target_link_libraries(fontfallback PRIVATE
    Qt5::Quick
    Qt5::Gui
)
```

### resources.qrc（Qt 5用）

```xml
<!DOCTYPE RCC>
<RCC version="1.0">
    <qresource prefix="/fonts">
        <file>fonts/Roboto-Regular.ttf</file>
        <file>fonts/NotoSansJP-Regular.ttf</file>
    </qresource>
    <qresource prefix="/">
        <file>main.qml</file>
    </qresource>
</RCC>
```

---

## 比較表

| 項目 | Qt 5.12 | Qt 5.13 | Qt 6.5 |
|------|---------|---------|--------|
| **フォールバック指定** | 手動制御 | `setFamilies()` | `setApplicationFallbackFontFamilies()` |
| **適用範囲** | フォントごと | フォントごと | **アプリ全体** |
| **スクリプト別設定** | ❌ | ❌ | ✅ |
| **実装の複雑さ** | 高 | 中 | 低 |
| **保守性** | 低 | 中 | 高 |
