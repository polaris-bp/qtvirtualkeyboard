# Qt フォント管理ガイド - 応用編

本ドキュメントでは、マルチ言語対応、プラットフォーム固有の問題、パフォーマンス最適化、トラブルシューティングなど、高度なトピックを解説します。

## 目次

1. [はじめに](#はじめに)
2. [Qtフォントシステムのアーキテクチャ](#qtフォントシステムのアーキテクチャ)
3. [マルチ言語対応](#マルチ言語対応)
4. [プラットフォーム固有の考慮事項](#プラットフォーム固有の考慮事項)
5. [パフォーマンス最適化](#パフォーマンス最適化)
6. [トラブルシューティング](#トラブルシューティング)
7. [Qt Virtual Keyboardでの実装](#qt-virtual-keyboardでの実装)
8. [参考リンク](#参考リンク)

---

## はじめに

### このドキュメントの対象者

- スタンダード編を理解した方
- 多言語アプリケーションを開発する方
- プラットフォーム固有の問題を解決したい方
- パフォーマンスを最適化したい方

### 前提知識

- 入門編・スタンダード編の内容
- 各プラットフォームの基本知識
- デバッグの基本

---

## Qtフォントシステムのアーキテクチャ

### 全体像

```
┌─────────────────────────────────────────────────────────────────┐
│                     アプリケーション層                           │
│  ┌──────────────────┐  ┌──────────────┐  ┌─────────────────┐   │
│  │   QML Text       │  │  QWidget     │  │  QPainter       │   │
│  │   font.family    │  │  setFont()   │  │  drawText()     │   │
│  └────────┬─────────┘  └──────┬───────┘  └────────┬────────┘   │
└───────────┼────────────────────┼──────────────────┼────────────┘
            │                    │                  │
            ▼                    ▼                  ▼
┌─────────────────────────────────────────────────────────────────┐
│                      Qt Font Engine                              │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                        QFont                             │    │
│  │  - family        : フォントファミリー名                   │    │
│  │  - pixelSize     : ピクセルサイズ                        │    │
│  │  - weight        : ウェイト (Light, Normal, Bold...)     │    │
│  │  - style         : スタイル (Normal, Italic, Oblique)    │    │
│  │  - styleStrategy : レンダリング戦略                      │    │
│  └─────────────────────────┬───────────────────────────────┘    │
│                            │                                     │
│                            ▼                                     │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │                   QFontDatabase                          │    │
│  │  - システムフォント一覧                                   │    │
│  │  - アプリケーションフォント登録                           │    │
│  │  - フォールバック設定 (Qt 6.5+)                          │    │
│  │  - フォントマッチング                                    │    │
│  └─────────────────────────┬───────────────────────────────┘    │
└────────────────────────────┼────────────────────────────────────┘
                             │
            ┌────────────────┼────────────────┐
            ▼                ▼                ▼
┌─────────────────────────────────────────────────────────────────┐
│                   プラットフォーム抽象化層                        │
│                                                                  │
│  ┌──────────────┐  ┌──────────────┐  ┌────────────────────┐    │
│  │   Windows    │  │    macOS     │  │    Linux/X11       │    │
│  │              │  │              │  │                    │    │
│  │ DirectWrite  │  │  Core Text   │  │   Fontconfig +     │    │
│  │ GDI (legacy) │  │              │  │   FreeType         │    │
│  └──────────────┘  └──────────────┘  └────────────────────┘    │
│                                                                  │
│  ┌──────────────┐  ┌──────────────┐                             │
│  │   Android    │  │  iOS/iPadOS  │                             │
│  │              │  │              │                             │
│  │  Skia +      │  │  Core Text   │                             │
│  │  FreeType    │  │              │                             │
│  └──────────────┘  └──────────────┘                             │
└─────────────────────────────────────────────────────────────────┘
```

### QFont::StyleStrategy の詳細

フォントレンダリングの戦略を制御します：

| 戦略 | 説明 | 使用場面 |
|------|------|---------|
| `PreferDefault` | システムのデフォルト設定 | 通常のテキスト |
| `PreferBitmap` | ビットマップフォントを優先 | 小サイズテキスト |
| `PreferDevice` | デバイスフォントを優先 | プリンタ出力 |
| `PreferOutline` | アウトラインフォントを優先 | 大サイズテキスト |
| `ForceOutline` | アウトラインを強制 | スケーリング重視 |
| `PreferAntialias` | アンチエイリアスを優先 | 滑らかな表示 |
| `NoAntialias` | アンチエイリアスなし | ピクセルパーフェクト |
| `NoSubpixelAntialias` | サブピクセルAAなし | LCD以外のディスプレイ |
| `NoFontMerging` | グリフフォールバック無効 | 厳密なフォント制御 |

```cpp
QFont font("Arial", 12);
font.setStyleStrategy(QFont::PreferAntialias);
// または複数の戦略を組み合わせ
font.setStyleStrategy(
    QFont::StyleStrategy(QFont::PreferAntialias | QFont::PreferQuality)
);
```

---

## マルチ言語対応

### 言語別推奨フォント

| 言語/スクリプト | 推奨フォント | 備考 |
|----------------|-------------|------|
| **ラテン文字** | Noto Sans, Roboto, Arial | 西欧言語共通 |
| **日本語** | Noto Sans JP, 源ノ角ゴシック | ひらがな・カタカナ・漢字 |
| **簡体字中国語** | Noto Sans SC, 思源黑体 CN | 中国本土 |
| **繁体字中国語** | Noto Sans TC, 思源黑体 TW | 台湾・香港 |
| **韓国語** | Noto Sans KR, Malgun Gothic | ハングル |
| **アラビア語** | Noto Sans Arabic, Amiri | RTL対応必須 |
| **ヘブライ語** | Noto Sans Hebrew | RTL対応必須 |
| **タイ語** | Noto Sans Thai | 複雑な文字組み |
| **デーヴァナーガリー** | Noto Sans Devanagari | ヒンディー語など |

### CJK統合フォントの使用

Noto Sans CJK は日本語・中国語・韓国語を1つのフォントでカバーします：

```cpp
// 1つのフォントで3言語に対応
QFontDatabase::addApplicationFont(
    ":/fonts/NotoSansCJK-Regular.ttc"
);
```

**注意**: CJK統合フォントは同じ漢字でも言語によって字形が異なる場合があります（例: 「直」の字形）。

### 言語別フォント切り替え

```qml
QtObject {
    id: fontSelector

    function getFontFamily(locale) {
        var lang = locale.substring(0, 2)
        switch (lang) {
        case "ja":
            return "Noto Sans JP, sans-serif"
        case "zh":
            // 簡体字と繁体字を区別
            if (locale.indexOf("TW") !== -1 ||
                locale.indexOf("HK") !== -1) {
                return "Noto Sans TC, sans-serif"
            }
            return "Noto Sans SC, sans-serif"
        case "ko":
            return "Noto Sans KR, sans-serif"
        case "ar":
            return "Noto Sans Arabic, sans-serif"
        case "he":
            return "Noto Sans Hebrew, sans-serif"
        case "th":
            return "Noto Sans Thai, sans-serif"
        default:
            return "Noto Sans, Arial, sans-serif"
        }
    }
}

Text {
    text: "多言語テキスト"
    font.family: fontSelector.getFontFamily(Qt.locale().name)
}
```

### RTL（右から左）言語への対応

アラビア語やヘブライ語などのRTL言語では、レイアウト全体を反転させる必要があります：

```qml
import QtQuick
import QtQuick.Layouts

ApplicationWindow {
    // アプリ全体のミラーリング設定
    LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    Text {
        text: "مرحبا بالعالم"  // アラビア語
        font.family: "Noto Sans Arabic"
        horizontalAlignment: Text.AlignRight
    }
}
```

```cpp
// C++でのRTL検出
bool isRtlLocale(const QString &locale) {
    static const QStringList rtlLanguages = {
        "ar", "he", "fa", "ur", "yi"
    };
    return rtlLanguages.contains(locale.left(2));
}
```

---

## プラットフォーム固有の考慮事項

### Windows

**レンダリングエンジン**: DirectWrite（Windows 7以降）

**特徴**:
- ClearType サブピクセルレンダリング
- システムフォントキャッシュの活用
- GDI互換モードも利用可能

**注意点**:
- 英語環境ではCJKフォントのフォールバックが不安定
- フォント名がローカライズされている場合がある

```cpp
// Windows固有のフォールバック設定
#ifdef Q_OS_WIN
QFontDatabase::setApplicationFallbackFontFamilies(
    QChar::Script_Han,
    {"Microsoft YaHei", "SimSun", "MS Gothic"}
);
#endif
```

**CJKフォントぼやけ問題の対処**:

```cpp
// 高DPI環境でのCJKフォントぼやけ対策
#ifdef Q_OS_WIN
// Windows 10 1703以降
qputenv("QT_ENABLE_HIGHDPI_SCALING", "0");
QApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
#endif
```

### macOS

**レンダリングエンジン**: Core Text

**特徴**:
- 高品質なフォントレンダリング
- Retinaディスプレイへの最適化
- システムフォントへの良好なアクセス

**注意点**:
- San Francisco フォントはシステム専用
- App Sandboxでのフォントアクセス制限

```cpp
#ifdef Q_OS_MACOS
// macOSではシステムフォントを優先
QFontDatabase::setApplicationFallbackFontFamilies(
    QChar::Script_Han,
    {"Hiragino Sans", "PingFang SC"}
);
#endif
```

### Linux

**レンダリングエンジン**: FreeType + Fontconfig

**特徴**:
- 高度にカスタマイズ可能
- ディストリビューションごとにフォント構成が異なる

**注意点**:
- fontconfig設定がQtの動作に影響
- フォントパッケージのインストール状況が異なる

```bash
# デバッグログを有効化
export QT_LOGGING_RULES="qt.qpa.fonts=true"
./your-application

# fontconfigのキャッシュ更新
fc-cache -fv
```

```cpp
#ifdef Q_OS_LINUX
// Linux向けのフォールバック
QFontDatabase::setApplicationFallbackFontFamilies(
    QChar::Script_Han,
    {"Noto Sans CJK JP", "Droid Sans Japanese", "TakaoGothic"}
);
#endif
```

### 組み込みLinux (EGLFS)

**特徴**:
- システムフォントが限定的または存在しない
- 全てのフォントをアプリケーションにバンドル必要

```cpp
// 組み込み向け: 必要なフォントを全てバンドル
void loadEmbeddedFonts() {
    // ラテン文字
    QFontDatabase::addApplicationFont(":/fonts/DejaVuSans.ttf");

    // CJK
    QFontDatabase::addApplicationFont(":/fonts/NotoSansCJK-Regular.ttc");

    // 絵文字（必要な場合）
    QFontDatabase::addApplicationFont(":/fonts/NotoColorEmoji.ttf");
}
```

### プラットフォーム検出コード

```cpp
QString getPlatformDefaultFont() {
#if defined(Q_OS_WIN)
    return "Segoe UI";
#elif defined(Q_OS_MACOS)
    return ".AppleSystemUIFont";
#elif defined(Q_OS_LINUX)
    return "Noto Sans";
#elif defined(Q_OS_ANDROID)
    return "Roboto";
#elif defined(Q_OS_IOS)
    return ".AppleSystemUIFont";
#else
    return "sans-serif";
#endif
}
```

---

## パフォーマンス最適化

### フォントファイルサイズの目安

| フォント種類 | 概算サイズ | 含まれる文字数 |
|-------------|-----------|---------------|
| ラテン文字のみ | 50-200KB | 数百文字 |
| 日本語（基本） | 3-8MB | JIS第1・第2水準 |
| 日本語（フル） | 10-20MB | JIS第3・第4水準含む |
| CJK統合 | 15-30MB | 日中韓共通 |
| 絵文字 | 10-25MB | カラー絵文字 |

### フォントサブセット化

必要な文字だけを含むフォントを作成することでサイズを削減できます：

```bash
# pyftsubset (fonttools) を使用
pip install fonttools

# 必要な文字のみを抽出
pyftsubset NotoSansJP-Regular.ttf \
    --text-file=used_characters.txt \
    --output-file=NotoSansJP-Subset.ttf
```

### 遅延ロード

大きなフォントは必要になるまでロードを遅延させます：

```cpp
class LazyFontLoader : public QObject
{
    Q_OBJECT
public:
    void ensureJapaneseFont() {
        if (m_japaneseFontId == -1) {
            m_japaneseFontId = QFontDatabase::addApplicationFont(
                ":/fonts/NotoSansJP-Regular.ttf"
            );
            emit japaneseFontLoaded();
        }
    }

signals:
    void japaneseFontLoaded();

private:
    int m_japaneseFontId = -1;
};
```

```qml
// QMLでの遅延ロード
FontLoader {
    id: japaneseFont
    // 最初は空、必要になったら設定
    source: ""
}

function loadJapaneseFont() {
    if (japaneseFont.source === "") {
        japaneseFont.source = "qrc:/fonts/NotoSansJP-Regular.ttf"
    }
}
```

### メモリ使用量の監視

```cpp
void logFontMemoryUsage() {
    // 登録されたフォントの数
    qDebug() << "Registered fonts:";
    for (const QString &family : QFontDatabase::families()) {
        if (QFontDatabase::isPrivateFamily(family)) {
            qDebug() << "  [App]" << family;
        }
    }
}
```

---

## トラブルシューティング

### デバッグログの有効化

```bash
# 環境変数で有効化
export QT_LOGGING_RULES="qt.qpa.fonts=true"

# または起動時に設定
QT_LOGGING_RULES="qt.qpa.fonts=true" ./your-application
```

```cpp
// C++で動的に有効化
#include <QLoggingCategory>
QLoggingCategory::setFilterRules("qt.qpa.fonts=true");
```

### 問題1: addApplicationFont が -1 を返す

**原因と対処**:

```cpp
int id = QFontDatabase::addApplicationFont(":/fonts/MyFont.ttf");
if (id == -1) {
    // 1. パスを確認
    qDebug() << "ファイル存在:" << QFile::exists(":/fonts/MyFont.ttf");

    // 2. リソースの内容を確認
    QDirIterator it(":/", QDirIterator::Subdirectories);
    while (it.hasNext()) {
        qDebug() << it.next();
    }

    // 3. ファイルサイズを確認（0バイトでないか）
    QFile f(":/fonts/MyFont.ttf");
    if (f.open(QIODevice::ReadOnly)) {
        qDebug() << "ファイルサイズ:" << f.size();
    }
}
```

**よくある原因**:
- リソースファイル(.qrc)に登録されていない
- CMakeLists.txtでリソースが追加されていない
- パスのタイプミス
- フォントファイルが破損している

### 問題2: フォントが適用されない

**確認手順**:

```qml
FontLoader {
    id: myFont
    source: "qrc:/fonts/MyFont.ttf"
    onStatusChanged: {
        console.log("Status:", status)
        if (status === FontLoader.Ready) {
            // ファミリー名を確認
            console.log("Family:", font.family)
            console.log("Name:", name)  // Qt 5互換
        }
    }
}

Text {
    text: "テスト"
    // font.family と font の違いを確認
    font: myFont.font  // Qt 6推奨
    // font.family: myFont.font.family  // 代替方法
}
```

**よくある原因**:
- フォントファミリー名がファイル名と異なる
- フォントに必要なグリフが含まれていない
- 複数のFontLoaderが競合している

### 問題3: 文字化けが発生する

```cpp
// グリフの存在確認
void checkGlyphAvailability(const QString &fontFamily,
                            const QString &text) {
    QFont font(fontFamily);
    QFontMetrics metrics(font);

    for (const QChar &ch : text) {
        if (!metrics.inFont(ch)) {
            qWarning() << "グリフなし:"
                       << ch << QString("(U+%1)")
                          .arg(ch.unicode(), 4, 16, QChar('0'));
        }
    }
}

// 使用例
checkGlyphAvailability("Arial", "Hello こんにちは");
// 出力: グリフなし: こ (U+3053)
//       グリフなし: ん (U+3093)
//       ...
```

### 問題4: プラットフォーム間でフォントが異なる

```cpp
// フォントマッチング結果を確認
void debugFontMatching(const QString &requestedFamily) {
    QFont requested(requestedFamily);
    QFontInfo actual(requested);

    qDebug() << "=== Font Matching Debug ===";
    qDebug() << "Requested:" << requestedFamily;
    qDebug() << "Actual family:" << actual.family();
    qDebug() << "Actual style:" << actual.styleName();
    qDebug() << "Exact match:" << actual.exactMatch();
    qDebug() << "===========================";
}
```

### 問題5: フォントサイズが想定と異なる

```qml
// pixelSize と pointSize の違い
Text {
    // pixelSize: 画面ピクセル数（DPI非依存）
    font.pixelSize: 16  // 常に16ピクセル

    // pointSize: 論理サイズ（DPI依存）
    // font.pointSize: 12  // DPIにより実サイズが変わる
}
```

```cpp
// 現在のDPIを確認
QScreen *screen = QGuiApplication::primaryScreen();
qDebug() << "DPI:" << screen->logicalDotsPerInch();
qDebug() << "Device pixel ratio:" << screen->devicePixelRatio();
```

---

## Qt Virtual Keyboardでの実装

### 現在のフォント構成

Qt Virtual Keyboardでは、スタイルファイルでフォントを定義しています：

| スタイル | フォント | 設定ファイル |
|---------|---------|-------------|
| Default | Arial | `src/styles/builtin/default/style.qml` |
| Retro | Courier | `src/styles/builtin/retro/style.qml` |

### カスタムフォントの追加方法

#### 方法1: スタイルファイル直接編集

```qml
// src/styles/builtin/default/style.qml

KeyboardStyle {
    id: currentStyle

    // カスタムフォントをロード
    FontLoader {
        id: customFontLoader
        source: resourcePrefix + "fonts/CustomFont.ttf"
    }

    // フォールバック付きで定義
    readonly property string fontFamily:
        customFontLoader.status === FontLoader.Ready
            ? customFontLoader.font.family
            : "Arial"

    // ... 以下既存のコード
}
```

#### 方法2: 言語別フォント切り替え

```qml
// src/styles/builtin/default/style.qml

KeyboardStyle {
    id: currentStyle

    // 言語別フォントローダー
    FontLoader {
        id: latinFont
        source: resourcePrefix + "fonts/Roboto-Regular.ttf"
    }

    FontLoader {
        id: japaneseFont
        source: resourcePrefix + "fonts/NotoSansJP-Regular.ttf"
    }

    // 言語に応じてフォントを選択
    readonly property string fontFamily: {
        var lang = InputContext.locale.substring(0, 2)
        switch (lang) {
        case "ja":
        case "zh":
        case "ko":
            return japaneseFont.status === FontLoader.Ready
                ? japaneseFont.font.family
                : "sans-serif"
        default:
            return latinFont.status === FontLoader.Ready
                ? latinFont.font.family
                : "Arial"
        }
    }
}
```

### フォントサイズのスケーリング

Qt Virtual Keyboardでは `scaleHint` を使用して動的にスケーリングします：

```qml
// scaleHint = keyboardHeight / keyboardDesignHeight
readonly property real scaleHint: keyboardHeight / keyboardDesignHeight

// 使用例
Text {
    font.pixelSize: 60 * scaleHint  // キーボードサイズに応じてスケール
}
```

---

## 参考リンク

### 公式ドキュメント

- [QFontDatabase Class | Qt GUI](https://doc.qt.io/qt-6/qfontdatabase.html)
- [FontLoader QML Type | Qt Quick](https://doc.qt.io/qt-6/qml-qtquick-fontloader.html)
- [QFont Class | Qt GUI](https://doc.qt.io/qt-6/qfont.html)
- [Qt Virtual Keyboard | Qt](https://doc.qt.io/qt-6/qtvirtualkeyboard-index.html)

### 関連記事

- [Qt Internationalization](https://www.qt.io/blog/2018/02/23/qt-internationalization-create-ui-using-non-latin-characters)
- [Qt Font Fallback | Qt Forum](https://forum.qt.io/topic/98849/qt-font-fallback-when-glyph-not-found)

### フォントリソース

- [Google Fonts](https://fonts.google.com/)
- [Google Noto Fonts](https://fonts.google.com/noto)
- [Adobe Source Han Sans](https://github.com/adobe-fonts/source-han-sans)
