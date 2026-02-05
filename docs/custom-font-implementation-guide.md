# Qt Virtual Keyboard カスタムフォント実装ガイド

本ドキュメントはQt Virtual Keyboardに独自フォントを追加するための具体的な実装手順を説明します。

## 目次

1. [現状のフォント構成](#現状のフォント構成)
2. [実装アプローチの選択](#実装アプローチの選択)
3. [実装手順](#実装手順)
4. [スタイル別カスタマイズ](#スタイル別カスタマイズ)
5. [動作確認](#動作確認)

---

## 現状のフォント構成

### 使用中のフォント

| スタイル | フォントファミリー | 設定ファイル |
|---------|-------------------|-------------|
| Default | Arial | `src/styles/builtin/default/style.qml` |
| Retro | Courier | `src/styles/builtin/retro/style.qml` |

### フォント設定箇所

```
src/styles/
├── KeyboardStyle.qml          # 基底スタイル定義
└── builtin/
    ├── default/
    │   └── style.qml          # fontFamily: "Arial"
    └── retro/
        └── style.qml          # fontFamily: "Courier"
```

### スケーリング機構

フォントサイズは `scaleHint` を使用して動的にスケーリングされます：

```qml
// KeyboardStyle.qml で定義
readonly property real scaleHint: keyboardHeight / keyboardDesignHeight

// 使用例
font.pixelSize: 60 * scaleHint
```

---

## 実装アプローチの選択

### アプローチ比較

| アプローチ | 難易度 | 影響範囲 | 推奨用途 |
|-----------|--------|---------|---------|
| A: スタイルファイル直接編集 | 低 | 単一スタイル | 特定スタイルのみ変更 |
| B: 共通フォントローダー | 中 | 全スタイル | 複数スタイルで共通フォント |
| C: C++プラグインでのロード | 高 | アプリ全体 | 起動時の確実なロード |

### 推奨

- **単純な置き換え**: アプローチA
- **新規スタイル作成**: アプローチB
- **商用製品での使用**: アプローチC

---

## 実装手順

### アプローチA: スタイルファイル直接編集

#### 1. フォントファイルの配置

```bash
# フォントディレクトリを作成
mkdir -p src/styles/builtin/default/fonts

# フォントファイルをコピー
cp /path/to/YourFont-Regular.ttf src/styles/builtin/default/fonts/
cp /path/to/YourFont-Bold.ttf src/styles/builtin/default/fonts/
```

#### 2. リソースファイルの作成

**src/styles/builtin/default/fonts.qrc:**

```xml
<!DOCTYPE RCC>
<RCC version="1.0">
    <qresource prefix="/qt-project.org/imports/QtQuick/VirtualKeyboard/Styles/Builtin/default/fonts">
        <file>YourFont-Regular.ttf</file>
        <file>YourFont-Bold.ttf</file>
    </qresource>
</RCC>
```

#### 3. CMakeLists.txtの更新

**src/styles/builtin/default/CMakeLists.txt に追加:**

```cmake
qt_add_resources(qtvkbdefaultstyle "fonts"
    PREFIX "/qt-project.org/imports/QtQuick/VirtualKeyboard/Styles/Builtin/default/fonts"
    FILES
        fonts/YourFont-Regular.ttf
        fonts/YourFont-Bold.ttf
)
```

#### 4. style.qmlの編集

**src/styles/builtin/default/style.qml:**

```qml
// Copyright (C) 2016 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only

import QtQuick
import QtQuick.Layouts
import QtQuick.VirtualKeyboard
import QtQuick.VirtualKeyboard.Styles

KeyboardStyle {
    id: currentStyle

    // カスタムフォントをロード
    FontLoader {
        id: customFontRegular
        source: resourcePrefix + "fonts/YourFont-Regular.ttf"
    }

    FontLoader {
        id: customFontBold
        source: resourcePrefix + "fonts/YourFont-Bold.ttf"
    }

    // フォールバック付きでフォントファミリーを定義
    readonly property string fontFamily: customFontRegular.status === FontLoader.Ready
        ? customFontRegular.font.family
        : "Arial"

    readonly property string fontFamilyBold: customFontBold.status === FontLoader.Ready
        ? customFontBold.font.family
        : fontFamily

    // 既存のプロパティ（変更なし）
    readonly property bool compactSelectionList: [InputEngine.InputMode.Pinyin, InputEngine.InputMode.Cangjie, InputEngine.InputMode.Zhuyin].indexOf(InputContext.inputEngine.inputMode) !== -1
    readonly property real keyBackgroundMargin: Math.round(8 * scaleHint)
    readonly property real keyContentMargin: Math.round(40 * scaleHint)
    readonly property real keyIconScale: scaleHint * 0.8
    readonly property string resourcePrefix: "qrc:/qt-project.org/imports/QtQuick/VirtualKeyboard/Styles/Builtin/default/"

    // ... 以下既存のコード
}
```

---

### アプローチB: 共通フォントローダー

#### 1. FontManager.qmlの作成

**src/styles/FontManager.qml:**

```qml
pragma Singleton
import QtQuick

QtObject {
    id: fontManager

    // フォントローダー
    property FontLoader primaryFont: FontLoader {
        source: "qrc:/qt-project.org/imports/QtQuick/VirtualKeyboard/Styles/fonts/PrimaryFont.ttf"
    }

    property FontLoader primaryFontBold: FontLoader {
        source: "qrc:/qt-project.org/imports/QtQuick/VirtualKeyboard/Styles/fonts/PrimaryFont-Bold.ttf"
    }

    property FontLoader japaneseFont: FontLoader {
        source: "qrc:/qt-project.org/imports/QtQuick/VirtualKeyboard/Styles/fonts/NotoSansJP-Regular.ttf"
    }

    property FontLoader japaneseFontBold: FontLoader {
        source: "qrc:/qt-project.org/imports/QtQuick/VirtualKeyboard/Styles/fonts/NotoSansJP-Bold.ttf"
    }

    // フォントファミリー名（フォールバック付き）
    readonly property string primaryFamily: primaryFont.status === FontLoader.Ready
        ? primaryFont.font.family
        : "Arial"

    readonly property string primaryFamilyBold: primaryFontBold.status === FontLoader.Ready
        ? primaryFontBold.font.family
        : primaryFamily

    readonly property string japaneseFamily: japaneseFont.status === FontLoader.Ready
        ? japaneseFont.font.family
        : "Noto Sans JP"

    readonly property string japaneseFamilyBold: japaneseFontBold.status === FontLoader.Ready
        ? japaneseFontBold.font.family
        : japaneseFamily

    // ロケールに基づくフォント選択
    function getFontFamily(locale) {
        var lang = locale.substring(0, 2)
        switch (lang) {
        case "ja":
            return japaneseFamily
        case "zh":
        case "ko":
            return japaneseFamily  // CJK共通フォントを使用する場合
        default:
            return primaryFamily
        }
    }

    function getFontFamilyBold(locale) {
        var lang = locale.substring(0, 2)
        switch (lang) {
        case "ja":
        case "zh":
        case "ko":
            return japaneseFamilyBold
        default:
            return primaryFamilyBold
        }
    }

    // ステータス確認用
    readonly property bool allFontsReady:
        primaryFont.status === FontLoader.Ready &&
        japaneseFont.status === FontLoader.Ready

    // デバッグ用
    function logFontStatus() {
        console.log("Primary Font:", primaryFamily, "Status:", primaryFont.status)
        console.log("Japanese Font:", japaneseFamily, "Status:", japaneseFont.status)
    }
}
```

#### 2. qmldirの更新

**src/styles/qmldir に追加:**

```
singleton FontManager 1.0 FontManager.qml
```

#### 3. スタイルファイルでの使用

**src/styles/builtin/default/style.qml:**

```qml
import QtQuick
import QtQuick.VirtualKeyboard
import QtQuick.VirtualKeyboard.Styles

KeyboardStyle {
    id: currentStyle

    // FontManagerからフォントを取得
    readonly property string fontFamily: FontManager.getFontFamily(InputContext.locale)

    // ... 既存のコード
}
```

---

### アプローチC: C++プラグインでのロード

#### 1. フォントローダークラスの作成

**src/plugin/fontloader.h:**

```cpp
#ifndef FONTLOADER_H
#define FONTLOADER_H

#include <QObject>
#include <QStringList>

class FontLoader : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList loadedFamilies READ loadedFamilies NOTIFY fontsLoaded)

public:
    explicit FontLoader(QObject *parent = nullptr);

    static FontLoader *instance();

    bool loadFontsFromResources(const QString &resourcePath = ":/fonts");
    QStringList loadedFamilies() const { return m_loadedFamilies; }

    Q_INVOKABLE QString primaryFont() const;
    Q_INVOKABLE QString japaneseFont() const;
    Q_INVOKABLE QString fontForLocale(const QString &locale) const;

signals:
    void fontsLoaded();

private:
    QList<int> m_fontIds;
    QStringList m_loadedFamilies;
    static FontLoader *s_instance;
};

#endif // FONTLOADER_H
```

**src/plugin/fontloader.cpp:**

```cpp
#include "fontloader.h"
#include <QFontDatabase>
#include <QDirIterator>
#include <QDebug>

FontLoader *FontLoader::s_instance = nullptr;

FontLoader::FontLoader(QObject *parent)
    : QObject(parent)
{
    s_instance = this;
}

FontLoader *FontLoader::instance()
{
    if (!s_instance) {
        s_instance = new FontLoader();
    }
    return s_instance;
}

bool FontLoader::loadFontsFromResources(const QString &resourcePath)
{
    QDirIterator it(resourcePath, {"*.ttf", "*.otf", "*.ttc"},
                    QDir::Files, QDirIterator::Subdirectories);

    bool anyLoaded = false;

    while (it.hasNext()) {
        QString fontPath = it.next();
        int id = QFontDatabase::addApplicationFont(fontPath);

        if (id != -1) {
            QStringList families = QFontDatabase::applicationFontFamilies(id);
            m_fontIds.append(id);
            m_loadedFamilies.append(families);
            qDebug() << "Loaded font:" << fontPath << "->" << families;
            anyLoaded = true;
        } else {
            qWarning() << "Failed to load font:" << fontPath;
        }
    }

    if (anyLoaded) {
        emit fontsLoaded();
    }

    return anyLoaded;
}

QString FontLoader::primaryFont() const
{
    // 設定や優先順位に基づいて返す
    for (const QString &family : m_loadedFamilies) {
        if (!family.contains("JP") && !family.contains("CJK")) {
            return family;
        }
    }
    return "Arial";
}

QString FontLoader::japaneseFont() const
{
    for (const QString &family : m_loadedFamilies) {
        if (family.contains("JP") || family.contains("CJK")) {
            return family;
        }
    }
    return "Noto Sans JP";
}

QString FontLoader::fontForLocale(const QString &locale) const
{
    QString lang = locale.left(2);

    if (lang == "ja" || lang == "zh" || lang == "ko") {
        return japaneseFont();
    }

    return primaryFont();
}
```

#### 2. プラグインへの統合

**src/plugin/plugin.cpp に追加:**

```cpp
#include "fontloader.h"

void QVirtualKeyboardPlugin::registerTypes(const char *uri)
{
    // フォントを早期にロード
    FontLoader::instance()->loadFontsFromResources(":/fonts");

    // QMLからアクセス可能にする
    qmlRegisterSingletonType<FontLoader>(uri, 2, 0, "FontLoader",
        [](QQmlEngine *, QJSEngine *) -> QObject * {
            return FontLoader::instance();
        });

    // 既存の型登録
    // ...
}
```

#### 3. QMLでの使用

```qml
import QtQuick.VirtualKeyboard 2.0

KeyboardStyle {
    readonly property string fontFamily: FontLoader.fontForLocale(InputContext.locale)
    // ...
}
```

---

## スタイル別カスタマイズ

### キーテキストのフォント設定

```qml
keyPanel: KeyPanel {
    // ...
    Text {
        id: keyText
        text: control.displayText
        font {
            family: fontFamily
            weight: Font.Normal
            pixelSize: 60 * scaleHint
            capitalization: control.uppercased ? Font.AllUppercase : Font.MixedCase
        }
    }
}
```

### 文字プレビューのフォント設定

```qml
characterPreviewDelegate: Item {
    // ...
    Text {
        id: characterPreviewText
        font {
            family: fontFamily
            weight: Font.Normal
            pixelSize: 82 * scaleHint
        }
    }
}
```

### 候補リストのフォント設定

```qml
selectionListDelegate: SelectionListItem {
    Text {
        id: selectionListLabel
        font {
            family: fontFamily
            weight: Font.Normal
            pixelSize: 44 * scaleHint
        }
    }
}
```

### フルスクリーン入力のフォント設定

```qml
fullScreenInputFont.family: fontFamily
fullScreenInputFont.pixelSize: 44 * scaleHint
```

---

## 動作確認

### 1. ビルド確認

```bash
cd /home/user/qtvirtualkeyboard
mkdir build && cd build
cmake ..
cmake --build .
```

### 2. フォントロード確認

デバッグログを有効にして確認：

```bash
export QT_LOGGING_RULES="qt.qpa.fonts=true"
./examples/virtualkeyboard/basic/basic
```

### 3. 視覚的確認

1. キーボードを表示
2. 各キーのテキストを確認
3. 文字プレビュー（長押し）を確認
4. 候補リストを確認
5. 異なるロケールに切り替えて確認

### 4. 自動テスト

```cpp
// tests/auto/fontloading/tst_fontloading.cpp
void tst_FontLoading::testCustomFontLoaded()
{
    // FontLoaderのステータス確認
    QCOMPARE(customFont.status, FontLoader.Ready);

    // フォントファミリー名の確認
    QVERIFY(!customFont.font.family.isEmpty());

    // グリフの存在確認
    QFont font(customFont.font.family);
    QFontMetrics metrics(font);
    QVERIFY(metrics.inFont(QChar('A')));
    QVERIFY(metrics.inFont(QChar(0x3042)));  // あ
}
```

---

## 注意事項

### フォントライセンス

埋め込むフォントのライセンスを確認してください：

- OFL (SIL Open Font License): 埋め込み可能
- Apache License: 埋め込み可能
- 商用フォント: ライセンス条項を確認

### ファイルサイズ

| フォント種類 | 概算サイズ |
|-------------|-----------|
| ラテン文字のみ | 50-200KB |
| 日本語（基本） | 3-8MB |
| 日本語（フル） | 10-20MB |
| CJK統合 | 15-30MB |

### パフォーマンス

- 大きなフォントファイルは起動時間に影響
- 必要に応じてサブセット化を検討
- 非同期ロードの実装を検討
