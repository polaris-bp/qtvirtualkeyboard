# Qt フォント管理ガイド - スタンダード編

本ドキュメントでは、実際のプロジェクトで独自フォントを管理するための実践的な手法を解説します。

## 目次

1. [はじめに](#はじめに)
2. [Qtリソースシステムの活用](#qtリソースシステムの活用)
3. [フォント解決メカニズム](#フォント解決メカニズム)
4. [C++でのフォント管理（QFontDatabase）](#cでのフォント管理qfontdatabase)
5. [フォールバックの設定](#フォールバックの設定)
6. [フォントの一元管理パターン](#フォントの一元管理パターン)
7. [ベストプラクティス](#ベストプラクティス)
8. [次のステップ](#次のステップ)

---

## はじめに

### このドキュメントの対象者

- 入門編を理解した方
- 実際のプロジェクトでフォントを管理したい方
- QMLとC++の両方でフォントを扱いたい方

### 前提知識

- 入門編の内容
- CMakeの基本
- C++の基本（C++でのフォント管理を行う場合）

---

## Qtリソースシステムの活用

### なぜリソースシステムを使うのか

入門編ではファイルパスを直接指定しましたが、アプリケーションを配布する際には問題が生じます：

| 方法 | 開発時 | 配布時 |
|------|--------|--------|
| ファイルパス直接指定 | ✅ 動作する | ❌ パスが変わると動かない |
| リソースシステム | ✅ 動作する | ✅ 実行ファイルに埋め込まれる |

リソースシステムを使うと、フォントファイルが実行ファイルに埋め込まれ、配布が容易になります。

### リソースファイル（.qrc）の作成

#### プロジェクト構成

```
my-project/
├── src/
│   └── main.cpp
├── qml/
│   └── main.qml
├── resources/
│   └── fonts/
│       ├── NotoSansJP-Regular.ttf
│       ├── NotoSansJP-Bold.ttf
│       └── fonts.qrc          ← リソースファイル
└── CMakeLists.txt
```

#### fonts.qrc の内容

```xml
<!DOCTYPE RCC>
<RCC version="1.0">
    <qresource prefix="/fonts">
        <file>NotoSansJP-Regular.ttf</file>
        <file>NotoSansJP-Bold.ttf</file>
    </qresource>
</RCC>
```

#### CMakeLists.txt への追加

```cmake
# 方法1: qt_add_resources を使用
qt_add_resources(${PROJECT_NAME} "fonts"
    PREFIX "/fonts"
    BASE "resources/fonts"
    FILES
        resources/fonts/NotoSansJP-Regular.ttf
        resources/fonts/NotoSansJP-Bold.ttf
)

# 方法2: .qrc ファイルを使用
qt_add_resources(${PROJECT_NAME} "fonts"
    resources/fonts/fonts.qrc
)
```

#### QMLからの使用

```qml
FontLoader {
    id: notoSansJP
    // "qrc:" プレフィックスでリソースにアクセス
    source: "qrc:/fonts/NotoSansJP-Regular.ttf"
}

Text {
    text: "リソースから読み込んだフォント"
    font.family: notoSansJP.font.family
}
```

---

## フォント解決メカニズム

### フォントマッチングの仕組み

Qtでフォントを指定すると、以下の順序で解決されます：

```
指定: font.family: "Noto Sans JP"
         │
         ▼
┌─────────────────────────────────────┐
│ 1. 完全一致するフォントを検索        │
│    → 見つかれば使用                 │
└─────────────────┬───────────────────┘
                  │ 見つからない
                  ▼
┌─────────────────────────────────────┐
│ 2. 類似フォントを検索               │
│    （ファミリー名の一部一致など）     │
└─────────────────┬───────────────────┘
                  │ 見つからない
                  ▼
┌─────────────────────────────────────┐
│ 3. システムのデフォルトフォントを使用 │
└─────────────────────────────────────┘
```

### フォント指定が「リクエスト」である理由

QFontは「このフォントが欲しい」というリクエストを表します。実際に使用されるフォントは、システムが持つフォントの中から最も近いものが選ばれます。

```cpp
// C++ での確認方法
QFont requestedFont("Noto Sans JP", 14);
QFontInfo actualFont(requestedFont);

qDebug() << "リクエスト:" << requestedFont.family();
qDebug() << "実際に使用:" << actualFont.family();
qDebug() << "完全一致:" << actualFont.exactMatch();
```

### グリフフォールバック（Font Merging）

1つのフォントに含まれない文字がある場合、Qtは自動的に別のフォントから文字を取得します：

```
テキスト: "Hello こんにちは"

Arial フォント:
  "Hello " → ✅ Arialで描画
  "こんにちは" → ❌ グリフなし → 日本語フォントから取得

結果: 2つのフォントが混在して描画される
```

この動作は便利ですが、意図しないフォントが混在する可能性があります。後述のフォールバック設定で制御できます。

---

## C++でのフォント管理（QFontDatabase）

### 基本的な使い方

```cpp
#include <QFontDatabase>
#include <QGuiApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    // リソースからフォントを追加
    int fontId = QFontDatabase::addApplicationFont(
        ":/fonts/NotoSansJP-Regular.ttf"
    );

    if (fontId == -1) {
        qWarning() << "フォントの読み込みに失敗";
        return 1;
    }

    // 追加されたフォントのファミリー名を取得
    QStringList families =
        QFontDatabase::applicationFontFamilies(fontId);

    qDebug() << "ロードされたフォント:" << families;
    // 出力例: ("Noto Sans JP")

    // QMLエンジンの起動など...
    return app.exec();
}
```

### QFontDatabase の主要メソッド

| メソッド | 説明 | 戻り値 |
|---------|------|--------|
| `addApplicationFont(path)` | フォントファイルを追加 | フォントID（失敗時は-1） |
| `addApplicationFontFromData(data)` | バイナリデータから追加 | フォントID |
| `applicationFontFamilies(id)` | 追加したフォントのファミリー名 | QStringList |
| `removeApplicationFont(id)` | フォントを削除 | bool |
| `families()` | 利用可能な全フォント | QStringList |

### C++でフォントを追加するタイミング

**重要**: QMLエンジンを起動する前にフォントを追加してください。

```cpp
int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    // ① フォントを追加（QMLエンジン起動前）
    QFontDatabase::addApplicationFont(":/fonts/MyFont.ttf");

    // ② QMLエンジンを起動
    QQmlApplicationEngine engine;
    engine.load(QUrl("qrc:/main.qml"));

    return app.exec();
}
```

### 複数フォントの一括ロード

```cpp
#include <QDirIterator>

void loadAllFonts()
{
    // リソース内の全フォントを検索してロード
    QDirIterator it(":/fonts",
                    {"*.ttf", "*.otf", "*.ttc"},
                    QDir::Files,
                    QDirIterator::Subdirectories);

    while (it.hasNext()) {
        QString path = it.next();
        int id = QFontDatabase::addApplicationFont(path);

        if (id != -1) {
            auto families =
                QFontDatabase::applicationFontFamilies(id);
            qDebug() << "Loaded:" << path << "->" << families;
        } else {
            qWarning() << "Failed:" << path;
        }
    }
}
```

---

## フォールバックの設定

### QMLでの複数フォント指定

CSS形式で複数のフォントを指定できます。左から順に試行され、最初に見つかったフォントが使用されます：

```qml
Text {
    // カンマ区切りでフォールバックチェーンを指定
    font.family: "Noto Sans JP, Arial, sans-serif"
    text: "フォールバックチェーン"
}
```

**解決順序**:
1. "Noto Sans JP" を検索 → あれば使用
2. なければ "Arial" を検索 → あれば使用
3. なければ "sans-serif"（汎用サンセリフ）を使用

### 条件付きフォールバック

FontLoaderのステータスに応じてフォールバックする：

```qml
Item {
    FontLoader {
        id: customFont
        source: "qrc:/fonts/CustomFont.ttf"
    }

    // フォールバック付きプロパティ
    readonly property string fontFamily:
        customFont.status === FontLoader.Ready
            ? customFont.font.family
            : "Arial, sans-serif"

    Text {
        text: "条件付きフォールバック"
        font.family: fontFamily
    }
}
```

### C++でのフォールバック設定（Qt 6.5以降）

スクリプト（文字体系）ごとにフォールバックを設定できます：

```cpp
#include <QFontDatabase>

void setupFontFallbacks()
{
    // 漢字（日本語・中国語・韓国語共通）
    QFontDatabase::setApplicationFallbackFontFamilies(
        QChar::Script_Han,
        {"Noto Sans CJK JP", "MS Gothic", "SimSun"}
    );

    // アラビア文字
    QFontDatabase::setApplicationFallbackFontFamilies(
        QChar::Script_Arabic,
        {"Noto Sans Arabic", "Arial"}
    );

    // ギリシャ文字
    QFontDatabase::setApplicationFallbackFontFamilies(
        QChar::Script_Greek,
        {"Noto Sans", "Arial"}
    );
}
```

**主要なスクリプト定数**:

| 定数 | 対象 |
|------|------|
| `QChar::Script_Latin` | ラテン文字（英語など） |
| `QChar::Script_Han` | 漢字（CJK共通） |
| `QChar::Script_Hiragana` | ひらがな |
| `QChar::Script_Katakana` | カタカナ |
| `QChar::Script_Arabic` | アラビア文字 |
| `QChar::Script_Cyrillic` | キリル文字（ロシア語など） |

---

## フォントの一元管理パターン

### QMLシングルトンパターン

アプリ全体で統一したフォントを使うため、フォント管理を一箇所にまとめます。

#### FontManager.qml

```qml
pragma Singleton
import QtQuick

QtObject {
    id: fontManager

    // フォントローダー
    property FontLoader _primaryFont: FontLoader {
        source: "qrc:/fonts/NotoSansJP-Regular.ttf"
    }

    property FontLoader _primaryFontBold: FontLoader {
        source: "qrc:/fonts/NotoSansJP-Bold.ttf"
    }

    // 公開プロパティ（フォールバック付き）
    readonly property string primary:
        _primaryFont.status === FontLoader.Ready
            ? _primaryFont.font.family
            : "sans-serif"

    readonly property string primaryBold:
        _primaryFontBold.status === FontLoader.Ready
            ? _primaryFontBold.font.family
            : primary

    // ステータス確認
    readonly property bool isReady:
        _primaryFont.status === FontLoader.Ready

    // デバッグ用
    function logStatus() {
        console.log("Primary font:", primary)
        console.log("Status:", _primaryFont.status)
    }
}
```

#### qmldir

```
module MyApp
singleton FontManager 1.0 FontManager.qml
```

#### 使用例

```qml
import QtQuick
import MyApp 1.0

Text {
    text: "一元管理されたフォント"
    font.family: FontManager.primary
    font.pixelSize: 16
}

Text {
    text: "太字バージョン"
    font.family: FontManager.primaryBold
    font.pixelSize: 16
    font.bold: true
}
```

### C++シングルトンパターン

#### FontManager.h

```cpp
#ifndef FONTMANAGER_H
#define FONTMANAGER_H

#include <QObject>
#include <QString>
#include <QList>

class FontManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString primary READ primary NOTIFY fontsLoaded)
    Q_PROPERTY(QString primaryBold READ primaryBold NOTIFY fontsLoaded)
    Q_PROPERTY(bool isReady READ isReady NOTIFY fontsLoaded)

public:
    static FontManager* instance();

    void loadFonts();

    QString primary() const { return m_primary; }
    QString primaryBold() const { return m_primaryBold; }
    bool isReady() const { return m_isReady; }

signals:
    void fontsLoaded();

private:
    explicit FontManager(QObject *parent = nullptr);

    QString m_primary = "sans-serif";
    QString m_primaryBold = "sans-serif";
    bool m_isReady = false;
    QList<int> m_fontIds;

    static FontManager* s_instance;
};

#endif // FONTMANAGER_H
```

#### FontManager.cpp

```cpp
#include "FontManager.h"
#include <QFontDatabase>
#include <QDebug>

FontManager* FontManager::s_instance = nullptr;

FontManager* FontManager::instance()
{
    if (!s_instance) {
        s_instance = new FontManager();
    }
    return s_instance;
}

FontManager::FontManager(QObject *parent)
    : QObject(parent)
{
}

void FontManager::loadFonts()
{
    // Regular
    int regularId = QFontDatabase::addApplicationFont(
        ":/fonts/NotoSansJP-Regular.ttf");
    if (regularId != -1) {
        auto families =
            QFontDatabase::applicationFontFamilies(regularId);
        if (!families.isEmpty()) {
            m_primary = families.first();
            m_fontIds.append(regularId);
        }
    }

    // Bold
    int boldId = QFontDatabase::addApplicationFont(
        ":/fonts/NotoSansJP-Bold.ttf");
    if (boldId != -1) {
        auto families =
            QFontDatabase::applicationFontFamilies(boldId);
        if (!families.isEmpty()) {
            m_primaryBold = families.first();
            m_fontIds.append(boldId);
        }
    }

    m_isReady = !m_fontIds.isEmpty();
    emit fontsLoaded();

    qDebug() << "FontManager loaded:"
             << m_primary << m_primaryBold;
}
```

#### main.cpp での登録

```cpp
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "FontManager.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    // フォントをロード
    FontManager::instance()->loadFonts();

    QQmlApplicationEngine engine;

    // QMLからアクセス可能にする
    engine.rootContext()->setContextProperty(
        "FontManager",
        FontManager::instance()
    );

    engine.load(QUrl("qrc:/main.qml"));
    return app.exec();
}
```

---

## ベストプラクティス

### 1. リソースシステムを使用する

```qml
// ✅ 推奨: リソースパス
source: "qrc:/fonts/MyFont.ttf"

// ❌ 非推奨: ファイルシステムパス
source: "file:///path/to/MyFont.ttf"
```

### 2. フォントは起動時にロードする

```cpp
int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    // ✅ QMLエンジン起動前にロード
    FontManager::instance()->loadFonts();

    QQmlApplicationEngine engine;
    // ...
}
```

### 3. フォールバックを必ず設定する

```qml
// ✅ フォールバック付き
readonly property string fontFamily:
    customFont.status === FontLoader.Ready
        ? customFont.font.family
        : "Arial, sans-serif"

// ❌ フォールバックなし（ロード失敗時に問題）
readonly property string fontFamily: customFont.font.family
```

### 4. フォントファミリー名は動的に取得する

```qml
// ✅ FontLoaderから取得
font.family: myFont.font.family

// ❌ ハードコード（フォント名が異なる場合がある）
font.family: "NotoSansJP-Regular"
```

### 5. pixelSize を使用する

```qml
// ✅ UIには pixelSize
font.pixelSize: 16

// △ pointSize はDPIに依存するため挙動が予測しにくい
font.pointSize: 12
```

---

## 次のステップ

スタンダード編の内容を理解したら、**応用編** に進んでください。

応用編では以下を学べます：
- マルチ言語対応とCJKフォント
- プラットフォーム固有の考慮事項
- パフォーマンス最適化
- 詳細なトラブルシューティング
- Qt Virtual Keyboardでの実装例

---

## 参考リンク

- [QFontDatabase Class | Qt GUI](https://doc.qt.io/qt-6/qfontdatabase.html)
- [The Qt Resource System | Qt Core](https://doc.qt.io/qt-6/resources.html)
- [QFont Class | Qt GUI](https://doc.qt.io/qt-6/qfont.html)
