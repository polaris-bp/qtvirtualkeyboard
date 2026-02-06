# Qt フォントAPI バージョン別差分ガイド

本ドキュメントは、Qt 5.12、5.13、6.x間のフォント関連APIの差分をまとめています。

> **注意**: 本ドキュメントシリーズ（入門編・スタンダード編・応用編）は **Qt 6（主に6.5以降）** を対象として記述されています。Qt 5.12/5.13を使用する場合は、本ドキュメントの差分を参照してください。

## 目次

1. [バージョン別サポート状況](#バージョン別サポート状況)
2. [Qt 5.12 vs Qt 5.13 の差分](#qt-512-vs-qt-513-の差分)
3. [Qt 5.x vs Qt 6.x の差分](#qt-5x-vs-qt-6x-の差分)
4. [バージョン互換コードの書き方](#バージョン互換コードの書き方)
5. [移行ガイド](#移行ガイド)

---

## バージョン別サポート状況

### 主要機能の対応バージョン

| 機能 | Qt 5.12 | Qt 5.13 | Qt 5.15 | Qt 6.0 | Qt 6.5 |
|------|:-------:|:-------:|:-------:|:------:|:------:|
| `FontLoader.name` | ✅ | ✅ | ✅ | ✅ | ✅ |
| `FontLoader.font` | ❌ | ❌ | ❌ | ✅ | ✅ |
| `QFont::setFamilies()` | ❌ | ✅ | ✅ | ✅ | ✅ |
| `QFontDatabase` インスタンスメソッド | ✅ | ✅ | ✅ | ⚠️ 非推奨 | ⚠️ 非推奨 |
| `QFontDatabase` 静的メソッド | ❌ | ❌ | ❌ | ✅ | ✅ |
| `setApplicationFallbackFontFamilies()` | ❌ | ❌ | ❌ | ❌ | ✅ |

---

## Qt 5.12 vs Qt 5.13 の差分

### 1. QFont::setFamilies() / families() の追加

Qt 5.13で複数フォントファミリーを指定するAPIが追加されました。

#### Qt 5.12

```cpp
// 単一のフォントファミリーのみ指定可能
QFont font;
font.setFamily("Noto Sans JP");

// フォールバックはカンマ区切りの文字列で疑似的に指定
// （プラットフォームによって動作が異なる可能性あり）
font.setFamily("Noto Sans JP, Arial, sans-serif");
```

#### Qt 5.13以降

```cpp
// 複数のフォントファミリーをリストで指定可能
QFont font;
font.setFamilies({"Noto Sans JP", "Arial", "sans-serif"});

// 取得も可能
QStringList families = font.families();
```

### 2. QFontコンストラクタの追加

Qt 5.13でペイントデバイスを指定するコンストラクタが追加されました。

```cpp
// Qt 5.13以降
QFont font(existingFont, paintDevice);
```

### 3. フォントマッチングアルゴリズムの変更

| バージョン | マッチング動作 |
|-----------|--------------|
| Qt 5.12 | `setFamily()` → `styleHint()` → "helvetica" → `lastResortFamily()` |
| Qt 5.13 | `setFamilies()` → `setFamily()` → writing system対応フォント |

---

## Qt 5.x vs Qt 6.x の差分

### 1. FontLoader (QML)

#### プロパティの変更

| プロパティ | Qt 5.x | Qt 6.x |
|-----------|--------|--------|
| `name` | 読み書き可能 | **読み取り専用** |
| `font` | ❌ 存在しない | ✅ 追加（推奨） |
| `source` | ✅ | ✅ |
| `status` | ✅ | ✅ |

#### Qt 5.x での使用方法

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

#### Qt 6.x での使用方法

```qml
import QtQuick

Item {
    FontLoader {
        id: customFont
        source: "qrc:/fonts/MyFont.ttf"
    }

    Text {
        text: "サンプル"
        // Qt 6では font プロパティを使用（推奨）
        font.family: customFont.font.family
        // または font 全体を使用
        // font: customFont.font
    }
}
```

#### 既知の問題 (QTBUG-68829)

Qt 5.9.6〜5.15において、FontLoaderの`name`プロパティが「実際のフォントファミリー名」ではなく「優先ファミリー名（preferred family name）」を返す問題がありました。同じ優先名を持つ複数のフォントバリアント（Regular, Bold, Italic等）を読み込むと、最後に読み込んだフォントが適用されてしまう場合があります。

この問題はQt 6.0で修正されています。

### 2. QFontDatabase (C++)

#### API形式の変更

| 操作 | Qt 5.x | Qt 6.x |
|------|--------|--------|
| ファミリー一覧取得 | `QFontDatabase db; db.families();` | `QFontDatabase::families();` |
| フォント追加 | `QFontDatabase::addApplicationFont()` | `QFontDatabase::addApplicationFont()` |
| スタイル取得 | `QFontDatabase db; db.styles(family);` | `QFontDatabase::styles(family);` |

#### Qt 5.x

```cpp
#include <QFontDatabase>

void listFonts()
{
    // インスタンスを作成してメソッドを呼び出す
    QFontDatabase database;

    const QStringList families = database.families();
    for (const QString &family : families) {
        qDebug() << family;

        const QStringList styles = database.styles(family);
        for (const QString &style : styles) {
            qDebug() << "  " << style;
        }
    }
}
```

#### Qt 6.x

```cpp
#include <QFontDatabase>

void listFonts()
{
    // 静的メソッドを直接呼び出す（インスタンス不要）
    const QStringList families = QFontDatabase::families();
    for (const QString &family : families) {
        qDebug() << family;

        const QStringList styles = QFontDatabase::styles(family);
        for (const QString &style : styles) {
            qDebug() << "  " << style;
        }
    }
}
```

### 3. QFont::Weight 列挙値の変更

Qt 6ではOpenType weight値に合わせて数値が変更されました。

| Weight | Qt 5.x 値 | Qt 6.x 値 |
|--------|----------|----------|
| Thin | 0 | 100 |
| ExtraLight | 12 | 200 |
| Light | 25 | 300 |
| Normal | 50 | 400 |
| Medium | 57 | 500 |
| DemiBold | 63 | 600 |
| Bold | 75 | 700 |
| ExtraBold | 81 | 800 |
| Black | 87 | 900 |

#### 移行時の注意

```cpp
// Qt 5.x - 整数値で指定可能（非推奨だが動作する）
font.setWeight(75);  // Bold

// Qt 6.x - 列挙値を使用（整数値はコンパイルエラー）
font.setWeight(QFont::Bold);

// Qt 6.x - 旧整数値を使いたい場合
font.setLegacyWeight(75);  // Qt 5互換
```

### 4. フォールバック設定 (Qt 6.5+)

Qt 6.5でスクリプト別フォールバック設定APIが追加されました。

```cpp
// Qt 6.5以降のみ
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
QFontDatabase::setApplicationFallbackFontFamilies(
    QChar::Script_Han,
    {"Noto Sans CJK JP", "MS Gothic"}
);
#endif
```

### 5. import文の変更

#### Qt 5.x

```qml
import QtQuick 2.12
import QtQuick.Window 2.12
```

#### Qt 6.x

```qml
import QtQuick
import QtQuick.Window
```

---

## バージョン互換コードの書き方

### QML: FontLoaderの互換使用

```qml
FontLoader {
    id: customFont
    source: "qrc:/fonts/MyFont.ttf"
}

Text {
    text: "互換性のあるコード"

    // Qt 5/6 両対応
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

### C++: QFontDatabaseの互換使用

```cpp
#include <QFontDatabase>
#include <QtGlobal>

QStringList getFontFamilies()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    // Qt 6: 静的メソッド
    return QFontDatabase::families();
#else
    // Qt 5: インスタンスメソッド
    QFontDatabase database;
    return database.families();
#endif
}

QStringList getFontStyles(const QString &family)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return QFontDatabase::styles(family);
#else
    QFontDatabase database;
    return database.styles(family);
#endif
}
```

### C++: QFont::Weightの互換使用

```cpp
void setFontBold(QFont &font)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    font.setWeight(QFont::Bold);
#else
    font.setWeight(75);  // Qt 5のBold値
    // または
    font.setWeight(QFont::Bold);  // Qt 5でも列挙値は使用可能
#endif
}
```

---

## 移行ガイド

### Qt 5.12 → Qt 5.13

1. **影響が小さい**: フォント関連の破壊的変更はほぼなし
2. **推奨**: `setFamilies()` を使用してフォールバックを明示的に指定

### Qt 5.x → Qt 6.x

1. **QFontDatabaseの書き換え**
   - インスタンス作成を削除し、静的メソッド呼び出しに変更

2. **FontLoaderの更新**
   - `name` から `font.family` への変更（Qt 6では`name`も動作するが非推奨）

3. **QFont::Weightの確認**
   - 整数値を直接使用している箇所を列挙値に変更

4. **import文の更新**
   - バージョン番号を削除（Qt 6スタイル）

5. **ビルド設定の更新**
   - CMakeLists.txtの更新（`qt_add_*` コマンドの使用）

---

## 参考リンク

- [FontLoader QML Type | Qt 5.15](https://doc.qt.io/archives/qt-5.15/qml-qtquick-fontloader.html)
- [FontLoader QML Type | Qt 6](https://doc.qt.io/qt-6/qml-qtquick-fontloader.html)
- [QFontDatabase Class | Qt 5.15](https://doc.qt.io/archives/qt-5.15/qfontdatabase.html)
- [QFontDatabase Class | Qt 6](https://doc.qt.io/qt-6/qfontdatabase.html)
- [Changes to Qt GUI | Qt 6](https://doc.qt.io/qt-6/gui-changes-qt6.html)
- [QTBUG-68829: FontLoader returns preferred family name](https://bugreports.qt.io/browse/QTBUG-68829)
