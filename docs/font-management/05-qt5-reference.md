# Qt 5.12 / 5.13 フォント管理 詳細リファレンス

本ドキュメントはQt 5.12およびQt 5.13におけるフォント管理の仕様を詳細に解説します。特にフォールバック設定について重点的に説明します。

## 目次

1. [概要](#概要)
2. [Qt 5.12 フォント仕様](#qt-512-フォント仕様)
3. [Qt 5.13 フォント仕様](#qt-513-フォント仕様)
4. [フォントマッチングアルゴリズム](#フォントマッチングアルゴリズム)
5. [フォールバック設定の詳細](#フォールバック設定の詳細)
6. [グリフフォールバック（Font Merging）](#グリフフォールバックfont-merging)
7. [フォント置換（Font Substitution）](#フォント置換font-substitution)
8. [プラットフォーム別の動作](#プラットフォーム別の動作)
9. [実装パターン集](#実装パターン集)
10. [制限事項と注意点](#制限事項と注意点)

---

## 概要

### バージョン別機能比較

| 機能 | Qt 5.12 | Qt 5.13 |
|------|:-------:|:-------:|
| `QFont::setFamily()` | ✅ | ✅ |
| `QFont::setFamilies()` | ❌ | ✅ |
| `QFont::families()` | ❌ | ✅ |
| `QFont::setStyleHint()` | ✅ | ✅ |
| `QFont::setStyleStrategy()` | ✅ | ✅ |
| `QFontDatabase::addApplicationFont()` | ✅ | ✅ |
| Font Merging（グリフフォールバック） | ✅ | ✅ |
| Font Substitution | ✅ | ✅ |
| FontLoader (QML) | ✅ | ✅ |

### QML FontLoaderの仕様（共通）

| プロパティ | 型 | 説明 |
|-----------|-----|------|
| `source` | url | フォントファイルのURL |
| `status` | enumeration | Null, Loading, Ready, Error |
| `name` | string | フォントファミリー名（**読み書き可能**） |

**注意**: Qt 5.xでは`name`プロパティは読み書き可能です。Qt 6.xでは読み取り専用に変更されました。

---

## Qt 5.12 フォント仕様

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
| `OldEnglish` / `Decorative` | 装飾フォント | - |
| `System` | システムフォント | OS依存 |
| `AnyStyle` | 任意 | - |
| `Cursive` | 筆記体 | Comic Sans MS |
| `Monospace` | 等幅 | Consolas, Monaco |
| `Fantasy` | ファンタジー | - |

```cpp
QFont font("MyCustomFont");
font.setStyleHint(QFont::SansSerif);  // 見つからない場合サンセリフ系を使用
```

#### QFont::setStyleStrategy()

フォントマッチングの戦略を設定します。

```cpp
void QFont::setStyleStrategy(StyleStrategy s)
```

**StyleStrategy 列挙値:**

| 値 | 説明 |
|-----|------|
| `PreferDefault` | デフォルト動作 |
| `PreferBitmap` | ビットマップフォント優先 |
| `PreferDevice` | デバイスフォント優先 |
| `PreferOutline` | アウトラインフォント優先 |
| `ForceOutline` | アウトラインを強制 |
| `PreferMatch` | 正確なサイズを優先 |
| `PreferQuality` | 品質を優先 |
| `PreferAntialias` | アンチエイリアス優先 |
| `NoAntialias` | アンチエイリアス無効 |
| `NoSubpixelAntialias` | サブピクセルAA無効 |
| `NoFontMerging` | **グリフフォールバック無効** |

```cpp
QFont font("MyFont");
// グリフフォールバックを無効化（厳密なフォント制御）
font.setStyleStrategy(QFont::NoFontMerging);
```

### Qt 5.12でのフォールバック実装パターン

```cpp
// パターン1: styleHintを使用
QFont createFontWithHint(const QString &family, int pixelSize)
{
    QFont font(family);
    font.setPixelSize(pixelSize);
    font.setStyleHint(QFont::SansSerif);  // フォールバックヒント
    return font;
}

// パターン2: 手動でフォント存在確認
QFont createFontWithManualFallback(const QStringList &families, int pixelSize)
{
    QFontDatabase db;
    QStringList availableFamilies = db.families();

    for (const QString &family : families) {
        if (availableFamilies.contains(family, Qt::CaseInsensitive)) {
            QFont font(family);
            font.setPixelSize(pixelSize);
            return font;
        }
    }

    // 全て見つからない場合
    QFont font;
    font.setStyleHint(QFont::SansSerif);
    font.setPixelSize(pixelSize);
    return font;
}

// 使用例
QFont font = createFontWithManualFallback(
    {"Noto Sans JP", "Yu Gothic", "MS Gothic", "Arial"},
    16
);
```

---

## Qt 5.13 フォント仕様

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
4. 見つからなければ "sans-serif" を検索 → システムのサンセリフフォントを使用
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

### ファウンドリ名の指定

フォントファミリー名にファウンドリ（製造元）を含めることができます。

```cpp
font.setFamilies({
    "Helvetica [Adobe]",      // Adobeのhelveticaを優先
    "Helvetica [Cronyx]",     // なければCronyxのHelvetica
    "Arial"                   // なければArial
});
```

### Qt 5.13でのフォールバック実装パターン

```cpp
// パターン1: 基本的なフォールバックチェーン
QFont createFontWithFallback(int pixelSize)
{
    QFont font;
    font.setFamilies({
        "Noto Sans JP",    // 第1優先
        "Yu Gothic",       // 第2優先
        "Hiragino Sans",   // 第3優先（macOS）
        "MS Gothic",       // 第4優先（Windows）
        "Arial",           // 第5優先
        "sans-serif"       // 最終フォールバック
    });
    font.setPixelSize(pixelSize);
    return font;
}

// パターン2: 用途別フォールバック
class FontFactory
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
qDebug() << "ピクセルサイズ:" << actual.pixelSize();
```

---

## フォールバック設定の詳細

### 方法1: styleHintによるカテゴリベースのフォールバック

最も基本的なフォールバック設定方法です。

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
// "Comic Sans MS" を要求されたら "Arial" を使用
QFont::insertSubstitution("Comic Sans MS", "Arial");

// 複数の置換を設定
QFont::insertSubstitutions("MyFont", {"Arial", "Helvetica", "sans-serif"});
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
        // 完全一致
        if (available.contains(candidate, Qt::CaseInsensitive)) {
            return candidate;
        }

        // 部分一致（"Noto Sans" で "Noto Sans JP" を見つける）
        for (const QString &avail : available) {
            if (avail.startsWith(candidate, Qt::CaseInsensitive)) {
                return avail;
            }
        }
    }

    return QString();  // 見つからない
}
```

### 方法5: QML FontLoaderによる確実なフォールバック

QMLでは`FontLoader`のステータスを確認してフォールバックを実装します。

```qml
// Qt 5.12 / 5.13 共通
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

### 文字列全体のグリフ確認

```cpp
QStringList getMissingGlyphs(const QFont &font, const QString &text)
{
    QFontMetrics metrics(font);
    QStringList missing;

    for (const QChar &ch : text) {
        if (!ch.isSpace() && !metrics.inFont(ch)) {
            missing << QString("%1 (U+%2)")
                .arg(ch)
                .arg(ch.unicode(), 4, 16, QChar('0'));
        }
    }

    return missing;
}
```

---

## フォント置換（Font Substitution）

### 置換の設定

```cpp
// 単一置換
QFont::insertSubstitution("Helvetica", "Arial");

// 複数置換（優先順位順）
QFont::insertSubstitutions("MyBrandFont", {
    "Noto Sans",
    "Arial",
    "sans-serif"
});
```

### 置換の取得

```cpp
// 単一の置換先を取得
QString substitute = QFont::substitute("Helvetica");
// → "Arial"（設定されている場合）

// 全ての置換先を取得
QStringList substitutes = QFont::substitutes("MyBrandFont");
// → ["Noto Sans", "Arial", "sans-serif"]

// 全置換ルールの対象フォントを取得
QStringList families = QFont::substitutions();
```

### 置換の削除

```cpp
// 特定の置換を削除
QFont::removeSubstitutions("MyBrandFont");
```

### 注意事項

- 置換はアプリケーション全体に影響する
- 置換はフォントマッチングの**前**に評価される
- 大文字小文字を区別しない

---

## プラットフォーム別の動作

### Windows (DirectWrite)

```cpp
// Windows固有の考慮事項
#ifdef Q_OS_WIN
// ClearTypeレンダリングがデフォルト
// システムフォールバック: Segoe UI, MS Gothic, SimSun など

// 高DPI環境での注意
// AA_EnableHighDpiScaling の設定確認
#endif
```

### macOS (Core Text)

```cpp
#ifdef Q_OS_MACOS
// システムフォールバック: San Francisco, Hiragino など
// Retinaディスプレイに自動最適化
#endif
```

### Linux (Fontconfig + FreeType)

```cpp
#ifdef Q_OS_LINUX
// fontconfig設定が優先される場合がある
// 環境変数: QT_QPA_FONTDIR でフォントディレクトリ指定可能
// デバッグ: QT_LOGGING_RULES="qt.qpa.fonts=true"
#endif
```

---

## 実装パターン集

### パターン1: 多言語対応フォントマネージャー（Qt 5.12）

```cpp
class FontManager512
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

### パターン2: 多言語対応フォントマネージャー（Qt 5.13）

```cpp
class FontManager513
{
public:
    static QFont forLanguage(const QString &lang, int pixelSize)
    {
        QFont font;
        font.setPixelSize(pixelSize);

        if (lang == "ja") {
            font.setFamilies({
                "Noto Sans JP", "Yu Gothic", "Hiragino Sans",
                "Meiryo", "MS Gothic", "sans-serif"
            });
        } else if (lang == "zh-CN") {
            font.setFamilies({
                "Noto Sans SC", "Microsoft YaHei", "SimHei",
                "SimSun", "sans-serif"
            });
        } else if (lang == "ko") {
            font.setFamilies({
                "Noto Sans KR", "Malgun Gothic", "Gulim", "sans-serif"
            });
        } else {
            font.setFamilies({
                "Noto Sans", "Segoe UI", "San Francisco",
                "Arial", "sans-serif"
            });
        }

        return font;
    }
};
```

### パターン3: QMLシングルトン（Qt 5.12/5.13共通）

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

## 参考リンク

- [QFont Class | Qt 5.12](https://doc.qt.io/archives/qt-5.12/qfont.html)
- [QFont Class | Qt 5.13](https://doc.qt.io/archives/qt-5.13/qfont.html)
- [QFontDatabase Class | Qt 5.12](https://doc.qt.io/qt-5.12/qfontdatabase.html)
- [FontLoader QML Type | Qt 5.12](https://doc.qt.io/archives/qt-5.12/qml-qtquick-fontloader.html)
- [QTBUG-68829: FontLoader returns preferred family name](https://bugreports.qt.io/browse/QTBUG-68829)
