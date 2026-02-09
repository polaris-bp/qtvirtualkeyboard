# Qt 5.12 / 5.13 フォント管理ガイド

本ドキュメントは、Qt 5.12およびQt 5.13でのフォント管理に関する包括的なガイドです。Qt 6への移行情報も含みます。

> **注意**: 入門編・スタンダード編・応用編（01〜03）は **Qt 6（主に6.5以降）** を対象として記述されています。Qt 5.12/5.13を使用する場合は、本ドキュメントを参照してください。

## 目次

1. [バージョン別機能比較](#バージョン別機能比較)
2. [Qt 5.12 の仕様](#qt-512-の仕様)
3. [Qt 5.13 の仕様](#qt-513-の仕様)
4. [フォントマッチングアルゴリズム](#フォントマッチングアルゴリズム)
5. [フォールバック設定の全手法](#フォールバック設定の全手法)
6. [グリフフォールバック（Font Merging）](#グリフフォールバックfont-merging)
7. [実装例：Qt 5.12](#実装例qt-512)
8. [実装例：Qt 5.13](#実装例qt-513)
9. [Qt 6への移行ガイド](#qt-6への移行ガイド)
10. [制限事項と注意点](#制限事項と注意点)

---

## バージョン別機能比較

### Qt 5.12 vs Qt 5.13 vs Qt 6

| 機能 | Qt 5.12 | Qt 5.13 | Qt 6.0 | Qt 6.5 |
|------|:-------:|:-------:|:------:|:------:|
| `QFont::setFamily()` | ✅ | ✅ | ✅ | ✅ |
| `QFont::setFamilies()` | ❌ | ✅ | ✅ | ✅ |
| `QFont::families()` | ❌ | ✅ | ✅ | ✅ |
| `FontLoader.name` | ✅ (読み書き) | ✅ (読み書き) | ✅ (読み取り専用) | ✅ (読み取り専用) |
| `FontLoader.font` | ❌ | ❌ | ✅ | ✅ |
| `QFontDatabase` インスタンスメソッド | ✅ | ✅ | ⚠️ 非推奨 | ⚠️ 非推奨 |
| `QFontDatabase` 静的メソッド | ❌ | ❌ | ✅ | ✅ |
| `setApplicationFallbackFontFamilies()` | ❌ | ❌ | ❌ | ✅ |
| Font Merging | ✅ | ✅ | ✅ | ✅ |
| Font Substitution | ✅ | ✅ | ✅ | ✅ |

### フォールバック制御の比較

| 項目 | Qt 5.12 | Qt 5.13 | Qt 6.5 |
|------|---------|---------|--------|
| **フォールバック指定** | 手動制御 | `setFamilies()` | `setApplicationFallbackFontFamilies()` |
| **適用範囲** | フォントごと | フォントごと | **アプリ全体** |
| **スクリプト別設定** | ❌ | ❌ | ✅ |
| **実装の複雑さ** | 高 | 中 | 低 |

---

## Qt 5.12 の仕様

### 主要API

#### QFont::setFamily()

```cpp
void QFont::setFamily(const QString &family)
```

単一のフォントファミリーを設定します。ファミリーが見つからない場合、フォントマッチングアルゴリズムにより代替フォントが選択されます。

```cpp
QFont font;
font.setFamily("Noto Sans JP");
```

#### カンマ区切りによる疑似フォールバック

Qt 5.12では`setFamilies()`がないため、カンマ区切りの文字列で複数フォントを指定できます。ただし、この動作は**プラットフォーム依存**です。

```cpp
// プラットフォームによって動作が異なる可能性あり
font.setFamily("Noto Sans JP, Arial, sans-serif");
```

#### QFont::setStyleHint()

フォントが見つからない場合のフォールバックヒントを設定します。

```cpp
void QFont::setStyleHint(StyleHint hint, StyleStrategy strategy = PreferDefault)
```

**StyleHint 列挙値:**

| 値 | 説明 | フォールバック例 |
|-----|------|----------------|
| `Helvetica` / `SansSerif` | サンセリフ体 | Arial, Helvetica |
| `Times` / `Serif` | セリフ体 | Times New Roman |
| `Courier` / `TypeWriter` | 等幅フォント | Courier New |
| `Monospace` | 等幅 | Consolas, Monaco |
| `System` | システムフォント | OS依存 |

```cpp
QFont font("MyCustomFont");
font.setStyleHint(QFont::SansSerif);  // 見つからない場合サンセリフ系を使用
```

#### QFont::setStyleStrategy()

フォントマッチングの戦略を設定します。

```cpp
void QFont::setStyleStrategy(StyleStrategy s)
```

**主要なStyleStrategy:**

| 値 | 説明 |
|-----|------|
| `PreferDefault` | デフォルト動作 |
| `PreferOutline` | アウトラインフォント優先 |
| `PreferQuality` | 品質を優先 |
| `NoFontMerging` | **グリフフォールバック無効** |

```cpp
QFont font("MyFont");
// グリフフォールバックを無効化（厳密なフォント制御）
font.setStyleStrategy(QFont::NoFontMerging);
```

### FontLoader (QML)

Qt 5.12/5.13のFontLoaderプロパティ：

| プロパティ | 型 | 説明 |
|-----------|-----|------|
| `source` | url | フォントファイルのURL |
| `status` | enumeration | Null, Loading, Ready, Error |
| `name` | string | フォントファミリー名（**読み書き可能**） |

```qml
import QtQuick 2.12

Item {
    FontLoader {
        id: customFont
        source: "fonts/MyFont.ttf"
    }

    Text {
        text: "サンプル"
        // Qt 5では name プロパティを使用
        font.family: customFont.name
    }
}
```

---

## Qt 5.13 の仕様

### 新規追加API

#### QFont::setFamilies()

**Qt 5.13で追加**

```cpp
void QFont::setFamilies(const QStringList &families)
```

複数のフォントファミリーをリストで指定します。Qtは先頭から順に検索し、最初に見つかったフォントを使用します。

```cpp
QFont font;
font.setFamilies({"Noto Sans JP", "Yu Gothic", "Arial", "sans-serif"});
font.setPixelSize(16);
```

**動作:**
1. "Noto Sans JP" を検索 → 見つかれば使用、終了
2. 見つからなければ "Yu Gothic" を検索 → 見つかれば使用、終了
3. 見つからなければ "Arial" を検索 → 見つかれば使用、終了
4. 見つからなければ "sans-serif" → システムのサンセリフフォントを使用
5. 全て見つからなければ → システムデフォルトフォント

#### QFont::families()

**Qt 5.13で追加**

```cpp
QStringList QFont::families() const
```

`setFamilies()`で設定されたフォントファミリーリストを取得します。

```cpp
QFont font;
font.setFamilies({"Noto Sans JP", "Arial"});

QStringList families = font.families();
// families = ["Noto Sans JP", "Arial"]
```

---

## フォントマッチングアルゴリズム

### Qt 5.12のマッチングフロー

```
setFamily("RequestedFont")
         │
         ▼
┌─────────────────────────────┐
│ 1. 指定されたファミリーを検索  │
└──────────────┬──────────────┘
               │ 見つからない
               ▼
┌─────────────────────────────┐
│ 2. styleHint に基づく       │
│    代替ファミリーを検索      │
└──────────────┬──────────────┘
               │ 見つからない
               ▼
┌─────────────────────────────┐
│ 3. "helvetica" を検索       │
└──────────────┬──────────────┘
               │ 見つからない
               ▼
┌─────────────────────────────┐
│ 4. lastResortFamily() を    │
│    試行                     │
└──────────────┬──────────────┘
               │ 見つからない
               ▼
┌─────────────────────────────┐
│ 5. lastResortFont() を使用  │
│   （常に何かを返す）         │
└─────────────────────────────┘
```

### Qt 5.13のマッチングフロー

```
setFamilies({"Font1", "Font2", "Font3"})
         │
         ▼
┌─────────────────────────────┐
│ 1. families() リストを      │
│    先頭から順に検索          │
│    → 見つかったら終了       │
└──────────────┬──────────────┘
               │ 全て見つからない
               ▼
┌─────────────────────────────┐
│ 2. setFamily() で設定された │
│    ファミリーを検索          │
└──────────────┬──────────────┘
               │ 見つからない
               ▼
┌─────────────────────────────┐
│ 3. writing system に対応    │
│    した代替フォントを選択    │
└─────────────────────────────┘
```

### マッチング結果の確認

```cpp
QFont requested("MyCustomFont", 14);
QFontInfo actual(requested);

qDebug() << "要求:" << requested.family();
qDebug() << "実際:" << actual.family();
qDebug() << "完全一致:" << actual.exactMatch();
qDebug() << "スタイル:" << actual.styleName();
```

---

## フォールバック設定の全手法

Qt 5.12/5.13では、以下の5つの方法でフォールバックを設定できます。

### 方法1: styleHintによるカテゴリベースのフォールバック

最も基本的な方法です。

```cpp
QFont font("NonExistentFont");
font.setStyleHint(QFont::SansSerif);
// → システムのサンセリフフォントにフォールバック
```

**利点**: シンプル、プラットフォーム間で一貫した動作
**欠点**: 具体的なフォントを指定できない

### 方法2: setFamilies()によるリストベースのフォールバック（Qt 5.13+）

明示的なフォールバックチェーンを指定できます。

```cpp
QFont font;
font.setFamilies({"PrimaryFont", "Fallback1", "Fallback2", "sans-serif"});
```

**利点**: 明確な優先順位、予測可能な動作
**欠点**: Qt 5.13以降でのみ使用可能

### 方法3: Font Substitutionによるグローバル置換

アプリケーション全体でフォント置換ルールを設定できます。

```cpp
// 単一置換
QFont::insertSubstitution("Comic Sans MS", "Arial");

// 複数置換（優先順位順）
QFont::insertSubstitutions("MyBrandFont", {"Noto Sans", "Arial", "sans-serif"});

// 取得
QStringList substitutes = QFont::substitutes("MyBrandFont");

// 削除
QFont::removeSubstitutions("MyBrandFont");
```

**利点**: 一度の設定でアプリ全体に適用
**欠点**: 意図しない置換が発生する可能性

### 方法4: 手動フォント存在確認によるフォールバック

最も確実な方法ですが、実装が複雑です。

```cpp
QString resolveFontFamily(const QStringList &candidates)
{
    QFontDatabase db;
    QStringList available = db.families();

    for (const QString &candidate : candidates) {
        if (available.contains(candidate, Qt::CaseInsensitive)) {
            return candidate;
        }
    }

    return "sans-serif";  // 見つからない場合
}

// 使用例
QString family = resolveFontFamily({
    "Noto Sans JP", "Yu Gothic", "MS Gothic", "Arial"
});
```

### 方法5: QML FontLoaderによる確実なフォールバック

QMLでは`FontLoader`のステータスを確認してフォールバックを実装します。

```qml
import QtQuick 2.12

Item {
    FontLoader { id: font1; source: "qrc:/fonts/Primary.ttf" }
    FontLoader { id: font2; source: "qrc:/fonts/Fallback1.ttf" }
    FontLoader { id: font3; source: "qrc:/fonts/Fallback2.ttf" }

    readonly property string resolvedFont: {
        if (font1.status === FontLoader.Ready) return font1.name
        if (font2.status === FontLoader.Ready) return font2.name
        if (font3.status === FontLoader.Ready) return font3.name
        return "sans-serif"
    }

    Text {
        text: "フォールバック対応テキスト"
        font.family: resolvedFont
        font.pixelSize: 16
    }
}
```

---

## グリフフォールバック（Font Merging）

### 概要

選択したフォントに特定の文字（グリフ）が存在しない場合、Qtは自動的に他のフォントからその文字を取得します。

```
テキスト: "Hello こんにちは"
フォント: Arial

"Hello " → Arial に存在 → Arial で描画
"こんにちは" → Arial に存在しない → システムの日本語フォントで描画
```

### Font Mergingの制御

```cpp
// Font Merging を有効（デフォルト）
QFont font("Arial");
// 日本語文字はシステムの日本語フォントで自動的に描画される

// Font Merging を無効
QFont strictFont("Arial");
strictFont.setStyleStrategy(QFont::NoFontMerging);
// 日本語文字は □ や ? で表示される
```

### グリフ存在確認

```cpp
bool hasGlyph(const QFont &font, QChar ch)
{
    QFontMetrics metrics(font);
    return metrics.inFont(ch);
}

// 使用例
QFont font("Arial");
qDebug() << "A:" << hasGlyph(font, 'A');        // true
qDebug() << "あ:" << hasGlyph(font, u'あ');     // false（通常）
```

---

## 実装例：Qt 5.12

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
    }
}
```

### C++: 手動フォールバック + styleHint

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

public:
    explicit FontManager512(QObject *parent = nullptr) : QObject(parent) {}

    void loadFonts()
    {
        int id = QFontDatabase::addApplicationFont(":/fonts/CustomFont.ttf");
        if (id != -1) {
            QFontDatabase db;
            m_primaryFamily = db.applicationFontFamilies(id).value(0, "");
        }
        emit fontsChanged();
    }

    QString primaryFamily() const { return m_primaryFamily; }

    // Qt 5.12でのフォールバック付きQFont作成
    Q_INVOKABLE QFont createFontWithFallback(const QString &primary,
                                              int pixelSize) const
    {
        QFont font;

        // カンマ区切りでフォールバックを指定
        QString familyChain = primary + ", Arial, sans-serif";
        font.setFamily(familyChain);
        font.setPixelSize(pixelSize);

        // styleHintも設定
        font.setStyleHint(QFont::SansSerif);

        return font;
    }

signals:
    void fontsChanged();

private:
    QString m_primaryFamily = "sans-serif";
};
```

### 多言語対応フォントマネージャー（Qt 5.12）

```cpp
class FontManager512MultiLang
{
public:
    static QFont forLanguage(const QString &lang, int pixelSize)
    {
        QFont font;
        font.setPixelSize(pixelSize);

        if (lang == "ja") {
            font.setFamily(findFirstAvailable({
                "Noto Sans JP", "Yu Gothic", "Hiragino Sans",
                "Meiryo", "MS Gothic"
            }));
        } else if (lang == "zh-CN") {
            font.setFamily(findFirstAvailable({
                "Noto Sans SC", "Microsoft YaHei", "SimHei", "SimSun"
            }));
        } else if (lang == "ko") {
            font.setFamily(findFirstAvailable({
                "Noto Sans KR", "Malgun Gothic", "Gulim"
            }));
        } else {
            font.setFamily(findFirstAvailable({
                "Noto Sans", "Segoe UI", "San Francisco", "Arial"
            }));
        }

        font.setStyleHint(QFont::SansSerif);
        return font;
    }

private:
    static QString findFirstAvailable(const QStringList &candidates)
    {
        QFontDatabase db;
        QStringList available = db.families();

        for (const QString &candidate : candidates) {
            if (available.contains(candidate, Qt::CaseInsensitive)) {
                return candidate;
            }
        }
        return "sans-serif";
    }
};
```

---

## 実装例：Qt 5.13

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
```

### 用途別フォールバック（Qt 5.13）

```cpp
class FontFactory513
{
public:
    static QFont uiFont(int pixelSize)
    {
        QFont font;
        font.setFamilies({"Segoe UI", "San Francisco", "Noto Sans", "Arial"});
        font.setPixelSize(pixelSize);
        return font;
    }

    static QFont japaneseFont(int pixelSize)
    {
        QFont font;
        font.setFamilies({
            "Noto Sans JP",
            "Yu Gothic UI",
            "Hiragino Sans",
            "Meiryo",
            "MS Gothic"
        });
        font.setPixelSize(pixelSize);
        return font;
    }

    static QFont codeFont(int pixelSize)
    {
        QFont font;
        font.setFamilies({
            "Fira Code",
            "Source Code Pro",
            "Consolas",
            "Monaco",
            "monospace"
        });
        font.setPixelSize(pixelSize);
        return font;
    }
};
```

### QMLシングルトン（Qt 5.12/5.13共通）

```qml
// FontConfig.qml
pragma Singleton
import QtQuick 2.12

QtObject {
    // フォントローダー
    property FontLoader _primary: FontLoader {
        source: "qrc:/fonts/NotoSansJP-Regular.ttf"
    }
    property FontLoader _bold: FontLoader {
        source: "qrc:/fonts/NotoSansJP-Bold.ttf"
    }

    // 公開プロパティ
    readonly property string primaryFamily: _primary.status === FontLoader.Ready
        ? _primary.name : "sans-serif"

    readonly property string boldFamily: _bold.status === FontLoader.Ready
        ? _bold.name : primaryFamily

    readonly property bool isReady: _primary.status !== FontLoader.Loading

    // フォント作成関数
    function createFont(size, bold) {
        return {
            family: bold ? boldFamily : primaryFamily,
            pixelSize: size,
            bold: bold || false
        }
    }
}
```

---

## Qt 6への移行ガイド

### FontLoaderの変更

| 項目 | Qt 5.x | Qt 6.x |
|------|--------|--------|
| `name` | 読み書き可能 | **読み取り専用** |
| `font` | ❌ 存在しない | ✅ 追加（推奨） |

#### Qt 5/6 両対応コード

```qml
FontLoader {
    id: customFont
    source: "qrc:/fonts/MyFont.ttf"
}

Text {
    text: "互換性のあるコード"

    font.family: {
        // Qt 6 では font.family を使用、Qt 5 では name を使用
        if (typeof customFont.font !== 'undefined') {
            return customFont.font.family  // Qt 6
        } else {
            return customFont.name  // Qt 5
        }
    }
}
```

### QFontDatabaseの変更

#### Qt 5.x

```cpp
// インスタンスを作成してメソッドを呼び出す
QFontDatabase database;
const QStringList families = database.families();
```

#### Qt 6.x

```cpp
// 静的メソッドを直接呼び出す（インスタンス不要）
const QStringList families = QFontDatabase::families();
```

#### 互換コード

```cpp
QStringList getFontFamilies()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return QFontDatabase::families();
#else
    QFontDatabase database;
    return database.families();
#endif
}
```

### QFont::Weight の変更

Qt 6ではOpenType weight値に合わせて数値が変更されました。

| Weight | Qt 5.x 値 | Qt 6.x 値 |
|--------|----------|------------|
| Thin | 0 | 100 |
| Light | 25 | 300 |
| Normal | 50 | 400 |
| Bold | 75 | 700 |
| Black | 87 | 900 |

```cpp
// Qt 5.x - 整数値で指定可能
font.setWeight(75);  // Bold

// Qt 6.x - 列挙値を使用
font.setWeight(QFont::Bold);

// Qt 6.x - 旧整数値を使いたい場合
font.setLegacyWeight(75);  // Qt 5互換
```

### import文の変更

```qml
// Qt 5.x
import QtQuick 2.12
import QtQuick.Window 2.12

// Qt 6.x
import QtQuick
import QtQuick.Window
```

---

## 制限事項と注意点

### Qt 5.12 の制限

1. **setFamilies()が使用できない**
   - カンマ区切り文字列は非公式でプラットフォーム依存

2. **フォールバック制御が限定的**
   - styleHintはカテゴリ単位でしか指定できない

3. **FontLoader.nameの動作**
   - 一部のフォントで正しいファミリー名が返されない（QTBUG-68829）

### Qt 5.13 の制限

1. **スクリプト別フォールバックは不可**
   - `setApplicationFallbackFontFamilies()`はQt 6.5以降

2. **QTBUG-68829は未修正**
   - FontLoader.nameの問題はQt 6.0まで残存

### 共通の注意点

1. **フォント名の大文字小文字**
   - 大文字小文字を区別しない（case insensitive）

2. **フォントファイルの読み込みタイミング**
   - QMLエンジン起動前にC++でaddApplicationFont()を呼ぶ

3. **ウェイトの互換性**
   - Qt 5のウェイト値とQt 6は異なる（移行時注意）

---

## CMakeLists.txt テンプレート

### Qt 5.12/5.13用

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
        <file>fonts/CustomFont.ttf</file>
        <file>fonts/NotoSansJP-Regular.ttf</file>
    </qresource>
    <qresource prefix="/">
        <file>main.qml</file>
    </qresource>
</RCC>
```

---

## 参考リンク

- [QFont Class | Qt 5.12](https://doc.qt.io/archives/qt-5.12/qfont.html)
- [QFont Class | Qt 5.13](https://doc.qt.io/archives/qt-5.13/qfont.html)
- [QFontDatabase Class | Qt 5.12](https://doc.qt.io/qt-5.12/qfontdatabase.html)
- [FontLoader QML Type | Qt 5.12](https://doc.qt.io/archives/qt-5.12/qml-qtquick-fontloader.html)
- [Changes to Qt GUI | Qt 6](https://doc.qt.io/qt-6/gui-changes-qt6.html)
- [QTBUG-68829: FontLoader returns preferred family name](https://bugreports.qt.io/browse/QTBUG-68829)
