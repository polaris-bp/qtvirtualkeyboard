# Qt フォント管理ガイド

本ドキュメントはQtフレームワークにおけるフォント管理の仕組みを解説し、独自フォントを追加するための実践的なガイドを提供します。

## 目次

1. [概要](#概要)
2. [Qtフォントシステムのアーキテクチャ](#qtフォントシステムのアーキテクチャ)
3. [フォント解決メカニズム](#フォント解決メカニズム)
4. [独自フォントの追加方法](#独自フォントの追加方法)
5. [QML FontLoader](#qml-fontloader)
6. [C++ QFontDatabase](#c-qfontdatabase)
7. [フォールバックの設定](#フォールバックの設定)
8. [マルチ言語対応](#マルチ言語対応)
9. [プラットフォーム固有の考慮事項](#プラットフォーム固有の考慮事項)
10. [トラブルシューティング](#トラブルシューティング)
11. [ベストプラクティス](#ベストプラクティス)

---

## 概要

Qtのフォントシステムは、クロスプラットフォームで一貫したテキストレンダリングを実現するための抽象化レイヤーを提供します。主要なコンポーネントは以下の通りです：

| コンポーネント | 役割 |
|---------------|------|
| `QFont` | フォントの指定（ファミリー、サイズ、ウェイト等） |
| `QFontDatabase` | システムフォントの照会、カスタムフォントの登録 |
| `QFontMetrics` | フォントの測定情報（文字幅、高さ等） |
| `FontLoader` (QML) | QMLでのフォント動的ロード |

### サポートされるフォント形式

| 形式 | 拡張子 | 説明 |
|------|--------|------|
| TrueType | `.ttf` | 最も広くサポートされる形式 |
| OpenType | `.otf` | 高度なタイポグラフィ機能をサポート |
| TrueType Collection | `.ttc` | 複数フォントを1ファイルに格納 |

---

## Qtフォントシステムのアーキテクチャ

```
┌─────────────────────────────────────────────────────────────┐
│                      アプリケーション層                       │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────┐   │
│  │   QML Text   │  │   QWidget    │  │  QPainter::      │   │
│  │   font:      │  │   setFont()  │  │  drawText()      │   │
│  └──────┬───────┘  └──────┬───────┘  └────────┬─────────┘   │
└─────────┼─────────────────┼───────────────────┼─────────────┘
          │                 │                   │
          ▼                 ▼                   ▼
┌─────────────────────────────────────────────────────────────┐
│                       Qt Font Engine                         │
│  ┌──────────────────────────────────────────────────────┐   │
│  │                      QFont                            │   │
│  │  - family, pixelSize, weight, style                   │   │
│  │  - styleStrategy (NoFontMerging, PreferQuality, etc.) │   │
│  └──────────────────────┬───────────────────────────────┘   │
│                         ▼                                    │
│  ┌──────────────────────────────────────────────────────┐   │
│  │                  QFontDatabase                        │   │
│  │  - システムフォント一覧                                │   │
│  │  - アプリケーションフォント登録                        │   │
│  │  - フォールバック設定                                  │   │
│  └──────────────────────┬───────────────────────────────┘   │
└─────────────────────────┼───────────────────────────────────┘
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                  プラットフォーム抽象化層                     │
│  ┌────────────┐  ┌────────────┐  ┌────────────────────┐    │
│  │  Windows   │  │   macOS    │  │   Linux/X11        │    │
│  │  DirectWrite│  │  CoreText  │  │   Fontconfig       │    │
│  └────────────┘  └────────────┘  └────────────────────┘    │
└─────────────────────────────────────────────────────────────┘
```

---

## フォント解決メカニズム

### フォント選択の優先順位

Qtは以下の順序でフォントを解決します：

```
1. 明示的に指定されたフォントファミリー
   └─→ font.family: "Noto Sans JP"

2. フォールバックリスト（CSS形式）
   └─→ font.family: "Noto Sans JP, Arial, sans-serif"

3. アプリケーション定義のフォールバック
   └─→ QFontDatabase::setApplicationFallbackFontFamilies()

4. システムのフォールバック
   └─→ OS固有のフォント設定

5. Qtのデフォルトフォールバック
   └─→ 最終手段としてのシステムフォント
```

### フォントマッチングアルゴリズム

QFontは「リクエスト」として機能し、実際のフォントはシステムが持つ最も近いマッチを返します：

```cpp
// リクエスト
QFont requestedFont("MyCustomFont", 14, QFont::Bold);

// 実際に使用されるフォント（システムが決定）
QFontInfo actualFont(requestedFont);
qDebug() << "実際のフォント:" << actualFont.family();
qDebug() << "完全一致:" << actualFont.exactMatch();
```

### グリフフォールバック（Font Merging）

指定フォントに存在しない文字がある場合、Qtは自動的に代替フォントを検索します：

```
"Hello こんにちは"
   │       │
   │       └─→ Arial に日本語グリフがない場合
   │           → システムの日本語フォントから自動取得
   │
   └─→ Arial から取得
```

この機能は `QFont::NoFontMerging` で無効化できます：

```cpp
QFont font("Arial");
font.setStyleStrategy(QFont::NoFontMerging);
// グリフがない文字は □ や ? で表示される
```

---

## 独自フォントの追加方法

### 方法の比較

| 方法 | 使用場面 | 特徴 |
|------|---------|------|
| QML FontLoader | QMLアプリケーション | 宣言的、非同期ロード対応 |
| C++ QFontDatabase | C++アプリケーション、プラグイン | 起動時の一括ロード、細かい制御 |
| Qt Resource System | 両方 | アプリケーションへの埋め込み |

### フォントファイルの配置

推奨ディレクトリ構成：

```
project/
├── src/
├── resources/
│   └── fonts/
│       ├── NotoSansJP-Regular.ttf
│       ├── NotoSansJP-Bold.ttf
│       └── fonts.qrc
└── CMakeLists.txt
```

**fonts.qrc:**

```xml
<!DOCTYPE RCC>
<RCC version="1.0">
    <qresource prefix="/fonts">
        <file>NotoSansJP-Regular.ttf</file>
        <file>NotoSansJP-Bold.ttf</file>
    </qresource>
</RCC>
```

**CMakeLists.txt:**

```cmake
qt_add_resources(${PROJECT_NAME} "fonts"
    PREFIX "/fonts"
    FILES
        resources/fonts/NotoSansJP-Regular.ttf
        resources/fonts/NotoSansJP-Bold.ttf
)
```

---

## QML FontLoader

### 基本的な使い方

```qml
import QtQuick

Item {
    // フォントをロード
    FontLoader {
        id: customFont
        source: "qrc:/fonts/NotoSansJP-Regular.ttf"
    }

    // フォントを使用
    Text {
        text: "カスタムフォントで表示"
        font.family: customFont.font.family
        font.pixelSize: 24
    }
}
```

### FontLoaderのプロパティ

| プロパティ | 型 | 説明 |
|-----------|-----|------|
| `source` | url | フォントファイルのURL |
| `status` | enumeration | ロード状態 (Null, Loading, Ready, Error) |
| `name` | string | フォントファミリー名（自動設定） |
| `font` | font | デフォルトフォントクエリ（Qt 6.0以降） |

### ステータス監視

```qml
FontLoader {
    id: customFont
    source: "qrc:/fonts/MyFont.ttf"

    onStatusChanged: {
        switch (status) {
        case FontLoader.Null:
            console.log("フォント未指定")
            break
        case FontLoader.Loading:
            console.log("読み込み中...")
            break
        case FontLoader.Ready:
            console.log("読み込み完了:", font.family)
            break
        case FontLoader.Error:
            console.error("読み込みエラー")
            break
        }
    }
}
```

### フォールバック付きの実装

```qml
Item {
    FontLoader {
        id: primaryFont
        source: "qrc:/fonts/CustomFont.ttf"
    }

    // フォールバック付きでフォントファミリーを定義
    readonly property string fontFamily:
        primaryFont.status === FontLoader.Ready
            ? primaryFont.font.family
            : "Arial"

    Text {
        text: "フォールバック対応テキスト"
        font.family: fontFamily
    }
}
```

### シングルトンパターンでのグローバル管理

**FontManager.qml:**

```qml
pragma Singleton
import QtQuick

QtObject {
    // プライマリフォント
    property FontLoader primaryFont: FontLoader {
        source: "qrc:/fonts/PrimaryFont.ttf"
    }

    // セカンダリフォント
    property FontLoader secondaryFont: FontLoader {
        source: "qrc:/fonts/SecondaryFont.ttf"
    }

    // 日本語フォント
    property FontLoader japaneseFont: FontLoader {
        source: "qrc:/fonts/NotoSansJP-Regular.ttf"
    }

    // フォントファミリー名を公開（フォールバック付き）
    readonly property string primaryFamily:
        primaryFont.status === FontLoader.Ready
            ? primaryFont.font.family
            : "sans-serif"

    readonly property string secondaryFamily:
        secondaryFont.status === FontLoader.Ready
            ? secondaryFont.font.family
            : "serif"

    readonly property string japaneseFamily:
        japaneseFont.status === FontLoader.Ready
            ? japaneseFont.font.family
            : "Noto Sans CJK JP"
}
```

**qmldir:**

```
singleton FontManager 1.0 FontManager.qml
```

**使用例:**

```qml
import MyApp 1.0

Text {
    text: "グローバルフォント"
    font.family: FontManager.primaryFamily
}
```

---

## C++ QFontDatabase

### 基本的な使い方

```cpp
#include <QFontDatabase>
#include <QGuiApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    // フォントをロード
    int fontId = QFontDatabase::addApplicationFont(":/fonts/CustomFont.ttf");

    if (fontId == -1) {
        qWarning() << "フォントの読み込みに失敗しました";
    } else {
        // ロードされたフォントファミリー名を取得
        QStringList families = QFontDatabase::applicationFontFamilies(fontId);
        qDebug() << "ロードされたフォント:" << families;
    }

    // QML/Widgetで使用可能になる
    // ...

    return app.exec();
}
```

### 主要なAPI

| メソッド | 説明 |
|---------|------|
| `addApplicationFont(path)` | ファイルからフォントを追加 |
| `addApplicationFontFromData(data)` | バイナリデータからフォントを追加 |
| `applicationFontFamilies(id)` | 追加したフォントのファミリー名を取得 |
| `removeApplicationFont(id)` | 追加したフォントを削除 |
| `removeAllApplicationFonts()` | 全てのアプリケーションフォントを削除 |
| `families()` | 利用可能な全フォントファミリー |
| `isPrivateFamily(family)` | プライベートフォントか確認 |

### バイナリデータからのロード

```cpp
// ファイルから読み込み
QFile fontFile(":/fonts/CustomFont.ttf");
if (fontFile.open(QIODevice::ReadOnly)) {
    QByteArray fontData = fontFile.readAll();
    int fontId = QFontDatabase::addApplicationFontFromData(fontData);

    if (fontId != -1) {
        qDebug() << "フォントロード成功";
    }
}
```

### 複数フォントの一括ロード

```cpp
class FontManager
{
public:
    static void loadAllFonts()
    {
        // リソース内の全フォントをスキャン
        QDirIterator it(":/fonts", {"*.ttf", "*.otf"},
                        QDir::Files, QDirIterator::Subdirectories);

        while (it.hasNext()) {
            QString fontPath = it.next();
            int id = QFontDatabase::addApplicationFont(fontPath);

            if (id != -1) {
                QStringList families =
                    QFontDatabase::applicationFontFamilies(id);
                qDebug() << "Loaded:" << fontPath << "->" << families;
                m_fontIds.append(id);
            } else {
                qWarning() << "Failed to load:" << fontPath;
            }
        }
    }

    static void unloadAllFonts()
    {
        for (int id : m_fontIds) {
            QFontDatabase::removeApplicationFont(id);
        }
        m_fontIds.clear();
    }

private:
    static QList<int> m_fontIds;
};
```

---

## フォールバックの設定

### Qt 6.8以降: setApplicationFallbackFontFamilies

```cpp
// スクリプト別にフォールバックを設定
QFontDatabase::setApplicationFallbackFontFamilies(
    QChar::Script_Han,           // 漢字スクリプト
    {"Noto Sans CJK JP", "MS Gothic", "SimSun"}
);

QFontDatabase::setApplicationFallbackFontFamilies(
    QChar::Script_Arabic,
    {"Noto Sans Arabic", "Arial"}
);

// 優先順位: 後から追加したものが優先される
```

### QMLでの複数フォント指定

```qml
Text {
    // CSS形式でフォールバックチェーンを指定
    font.family: "CustomFont, Noto Sans JP, Arial, sans-serif"
    text: "フォールバックチェーン"
}
```

### 言語別フォント切り替え

```qml
Text {
    font.family: {
        switch (Qt.locale().name.substring(0, 2)) {
        case "ja":
            return "Noto Sans JP, sans-serif"
        case "zh":
            return "Noto Sans SC, sans-serif"
        case "ko":
            return "Noto Sans KR, sans-serif"
        case "ar":
            return "Noto Sans Arabic, sans-serif"
        default:
            return "Arial, sans-serif"
        }
    }
}
```

---

## マルチ言語対応

### 推奨フォント構成

| 言語/スクリプト | 推奨フォント |
|----------------|-------------|
| ラテン文字 | Noto Sans, Roboto, Arial |
| 日本語 | Noto Sans JP, IPAゴシック |
| 簡体字中国語 | Noto Sans SC, Source Han Sans CN |
| 繁体字中国語 | Noto Sans TC, Source Han Sans TW |
| 韓国語 | Noto Sans KR, Malgun Gothic |
| アラビア語 | Noto Sans Arabic, Amiri |
| ヘブライ語 | Noto Sans Hebrew |
| タイ語 | Noto Sans Thai |

### 統合フォントの使用

Noto Sans CJKは複数の東アジア言語をサポートする統合フォント：

```cpp
// 単一フォントで日中韓をカバー
QFontDatabase::addApplicationFont(":/fonts/NotoSansCJK-Regular.ttc");
```

### 右から左(RTL)言語の考慮

```qml
Text {
    // アラビア語などRTL言語用
    font.family: "Noto Sans Arabic"
    horizontalAlignment: Text.AlignRight
    LayoutMirroring.enabled: true
}
```

---

## プラットフォーム固有の考慮事項

### Windows

- **DirectWrite**がデフォルトのレンダリングエンジン
- システムフォールバックが最終手段として使用される
- CJKフォントは英語環境ではArialにフォールバックする可能性がある

```cpp
// Windows固有のフォールバック動作を回避
QFontDatabase::setApplicationFallbackFontFamilies(
    QChar::Script_Han,
    {"Microsoft YaHei", "SimSun"}
);
```

### macOS

- **Core Text**がレンダリングエンジン
- システムフォントへのアクセスが良好
- Retinaディスプレイでの高解像度レンダリング

### Linux

- **Fontconfig**がフォント設定を管理
- システムのfontconfig設定が優先される場合がある
- 環境変数でデバッグ可能:

```bash
export QT_LOGGING_RULES="qt.qpa.fonts=true"
./your-application
```

### 組み込みLinux (EGLFS)

- システムフォントが限られる場合がある
- 必要なフォントは全てアプリケーションにバンドル推奨

```cpp
// 最小限のフォント構成例
QFontDatabase::addApplicationFont(":/fonts/DejaVuSans.ttf");
QFontDatabase::addApplicationFont(":/fonts/NotoSansCJK-Regular.ttc");
```

---

## トラブルシューティング

### デバッグログの有効化

```bash
# 環境変数で有効化
export QT_LOGGING_RULES="qt.qpa.fonts=true"

# または C++ で
QLoggingCategory::setFilterRules("qt.qpa.fonts=true");
```

### よくある問題と解決策

#### 1. addApplicationFont が -1 を返す

```cpp
int id = QFontDatabase::addApplicationFont(":/fonts/MyFont.ttf");
if (id == -1) {
    // 原因の確認
    // 1. パスが正しいか確認
    qDebug() << "ファイル存在確認:" << QFile::exists(":/fonts/MyFont.ttf");

    // 2. リソースが正しく登録されているか確認
    QDirIterator it(":/", QDirIterator::Subdirectories);
    while (it.hasNext()) {
        qDebug() << it.next();
    }

    // 3. フォントファイルが破損していないか確認
    // 4. フォント形式がサポートされているか確認（TTF/OTF/TTC）
}
```

#### 2. フォントが適用されない

```qml
FontLoader {
    id: myFont
    source: "qrc:/fonts/MyFont.ttf"
    onStatusChanged: {
        if (status === FontLoader.Ready) {
            // 正しいファミリー名を確認
            console.log("ファミリー名:", font.family)
            console.log("name プロパティ:", name)
        }
    }
}

Text {
    // font プロパティ全体を使用（推奨）
    font: myFont.font
    // または正確なファミリー名を使用
    // font.family: myFont.font.family
}
```

#### 3. 文字化けが発生する

```cpp
// フォントがグリフを持っているか確認
QFont font("MyFont");
QFontMetrics metrics(font);
QString testString = "テスト文字列";
for (const QChar &ch : testString) {
    if (!metrics.inFont(ch)) {
        qWarning() << "グリフなし:" << ch;
    }
}
```

#### 4. フォントサイズが想定と異なる

```qml
Text {
    // pixelSize を使用（デバイス非依存）
    font.pixelSize: 16

    // pointSize はDPIに依存
    // font.pointSize: 12
}
```

---

## ベストプラクティス

### 1. 早期ロード

フォントはアプリケーション起動時にロードする：

```cpp
int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    // QMLエンジン作成前にフォントをロード
    FontManager::loadAllFonts();

    QQmlApplicationEngine engine;
    // ...
}
```

### 2. フォールバックの明示的設定

```qml
readonly property string fontFamily: {
    if (customFont.status === FontLoader.Ready) {
        return customFont.font.family
    }
    // 明示的なフォールバック
    return "Arial, Helvetica, sans-serif"
}
```

### 3. リソースファイルの使用

外部ファイルではなくQtリソースシステムを使用：

```qml
// 推奨: リソースから
source: "qrc:/fonts/MyFont.ttf"

// 非推奨: 外部ファイル（配布時に問題になる可能性）
// source: "file:///path/to/MyFont.ttf"
```

### 4. フォントファイルサイズの最適化

- 必要なグリフのみを含むサブセット化を検討
- 可変フォント（Variable Font）の使用を検討
- 複数ウェイトが必要な場合はTTCの使用を検討

### 5. 非同期ロードの考慮

```qml
FontLoader {
    id: customFont
    source: "qrc:/fonts/LargeFont.ttf"
}

Text {
    text: "Loading..."
    font.family: customFont.status === FontLoader.Ready
        ? customFont.font.family
        : "Arial"

    // ロード完了時にテキストを更新
    Connections {
        target: customFont
        function onStatusChanged() {
            if (customFont.status === FontLoader.Ready) {
                text = "フォントロード完了"
            }
        }
    }
}
```

### 6. テストの実施

```cpp
void testFontLoading()
{
    int id = QFontDatabase::addApplicationFont(":/fonts/TestFont.ttf");
    QVERIFY(id != -1);

    QStringList families = QFontDatabase::applicationFontFamilies(id);
    QVERIFY(!families.isEmpty());

    QFont font(families.first());
    QFontInfo info(font);
    QVERIFY(info.exactMatch());
}
```

---

## 参考リンク

- [QFontDatabase Class | Qt GUI](https://doc.qt.io/qt-6/qfontdatabase.html)
- [FontLoader QML Type | Qt Quick](https://doc.qt.io/qt-6/qml-qtquick-fontloader.html)
- [QFont Class | Qt GUI](https://doc.qt.io/qt-6/qfont.html)
- [Qt Internationalization](https://www.qt.io/blog/2018/02/23/qt-internationalization-create-ui-using-non-latin-characters)
- [Using Custom Fonts | Qt Forum](https://forum.qt.io/topic/57772/using-custom-fonts)
