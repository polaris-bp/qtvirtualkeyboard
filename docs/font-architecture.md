# Qt Virtual Keyboard フォントアーキテクチャ仕様書

## 目次

1. [概要](#概要)
2. [フォント戦略の全体像](#フォント戦略の全体像)
3. [スタイルシステムとフォント](#スタイルシステムとフォント)
4. [フォントスケーリング機構](#フォントスケーリング機構)
5. [UI要素ごとのフォント仕様](#ui要素ごとのフォント仕様)
6. [フォントプロパティ詳細](#フォントプロパティ詳細)
7. [フォールバックとUnicode対応](#フォールバックとunicode対応)
8. [フルスクリーン入力モードのフォント](#フルスクリーン入力モードのフォント)
9. [カスタムスタイル作成ガイド](#カスタムスタイル作成ガイド)
10. [ビルド構成とスタイル選択](#ビルド構成とスタイル選択)
11. [ファイル構成マップ](#ファイル構成マップ)

---

## 概要

Qt Virtual Keyboard は、フォントファイル（`.ttf`、`.otf` 等）を同梱せず、**システムフォントに完全に依存する**アーキテクチャを採用している。フォントの管理は C++ コードではなく、QML の宣言的スタイルシステムで完結しており、フォントの選択・サイズ計算・ウェイト指定はすべてスタイル定義ファイル (`style.qml`) 内で行われる。

### 設計上の特徴

- **外部フォント非同梱**: カスタムフォントファイルをバンドルしない
- **QML 完結型**: フォント管理に C++ の `QFontDatabase` 等は使用しない
- **スケーラブル**: `scaleHint` による比例スケーリングですべてのデバイスに適応
- **2スタイル提供**: Default（Arial）と Retro（Courier）の2つの組み込みスタイル

---

## フォント戦略の全体像

```
┌─────────────────────────────────────────────────────────┐
│                    アプリケーション                        │
│  (例: font.pixelSize = Qt.application.font.pixelSize*2) │
└──────────────────────┬──────────────────────────────────┘
                       │ テキスト入力
                       ▼
┌─────────────────────────────────────────────────────────┐
│              InputPanel (キーボード本体)                   │
│  ┌───────────────────────────────────────────────────┐  │
│  │         KeyboardStyle (スタイル基底型)               │  │
│  │  ┌─────────────────────────────────────────────┐  │  │
│  │  │  scaleHint = keyboardHeight /               │  │  │
│  │  │              keyboardDesignHeight            │  │  │
│  │  └─────────────────────────────────────────────┘  │  │
│  │                     │                             │  │
│  │          ┌──────────┴──────────┐                  │  │
│  │          ▼                     ▼                  │  │
│  │  ┌──────────────┐    ┌──────────────┐            │  │
│  │  │ Default Style│    │ Retro Style  │            │  │
│  │  │ (Arial)      │    │ (Courier)    │            │  │
│  │  │ Font.Normal  │    │ Font.Bold    │            │  │
│  │  └──────────────┘    └──────────────┘            │  │
│  └───────────────────────────────────────────────────┘  │
│                       │                                  │
│            ┌──────────┴──────────┐                       │
│            ▼                     ▼                       │
│   システムフォント           Qt フォント                    │
│   (Arial/Courier)         フォールバック機構               │
└─────────────────────────────────────────────────────────┘
```

---

## スタイルシステムとフォント

### KeyboardStyle 基底型

`src/styles/KeyboardStyle.qml` がすべてのスタイルの基底型であり、フォントに関連する以下のプロパティを定義する。

| プロパティ | 型 | 説明 |
|---|---|---|
| `keyboardHeight` | `real` | キーボードの実際の高さ |
| `keyboardDesignWidth` | `real` | キーボードのデザイン基準幅 |
| `keyboardDesignHeight` | `real` | キーボードのデザイン基準高さ |
| `scaleHint` | `real` (readonly) | スケーリング係数 (`keyboardHeight / keyboardDesignHeight`) |
| `fullScreenInputFont` | `font` | フルスクリーン入力時のフォント |

> **参照**: `src/styles/KeyboardStyle.qml:38` — `scaleHint` の定義

### 組み込みスタイルの比較

| 項目 | Default スタイル | Retro スタイル |
|---|---|---|
| **ファイル** | `src/styles/builtin/default/style.qml` | `src/styles/builtin/retro/style.qml` |
| **フォントファミリー** | `"Arial"` | `"Courier"` |
| **基本ウェイト** | `Font.Normal` | `Font.Bold` |
| **デザイン幅** | 2560px | 2560px |
| **デザイン高さ** | 800px | 800px |
| **レタースペーシング** | なし | `-5 * scaleHint` (一部キー) |
| **テキストフィットモード** | 使用しない (通常キー) | `Text.Fit` (通常キー) |

---

## フォントスケーリング機構

Qt Virtual Keyboard のフォントサイズは、固定ピクセル値ではなく、`scaleHint` を掛け合わせた**比例値**として定義される。これによりキーボードの実サイズに応じて自動的にスケーリングされる。

### scaleHint の算出

```qml
// src/styles/KeyboardStyle.qml:38
readonly property real scaleHint: keyboardHeight / keyboardDesignHeight
```

**計算例**:
- デザイン基準高さ: **800px**
- 実際のキーボード高さが 400px の場合: `scaleHint = 400 / 800 = 0.5`
- 実際のキーボード高さが 800px の場合: `scaleHint = 800 / 800 = 1.0`
- 実際のキーボード高さが 1200px の場合: `scaleHint = 1200 / 800 = 1.5`

### 適用パターン

すべてのフォントサイズは以下の形式で宣言される:

```qml
font {
    family: fontFamily
    weight: Font.Normal
    pixelSize: <基準サイズ> * scaleHint
}
```

**基準サイズ(px) × scaleHint** で実フォントサイズが決まるため、DPI やデバイスサイズの差異を自然に吸収する。Qt QML エンジン自体の DPI 対応と組み合わさることで、追加のハードコードされた DPI 計算は不要となる。

### 動的フォントサイズ (カーソル連動)

ポップアップリスト（予測変換候補の浮動表示）では、`scaleHint` ではなく入力フィールドのカーソル高さに基づいてフォントサイズが動的に決定される:

```qml
// src/styles/builtin/default/style.qml:1032
pixelSize: Qt.inputMethod.cursorRectangle.height * 0.8
```

これにより、ポップアップのテキストサイズが入力フィールドの見た目と一貫性を保つ。

---

## UI要素ごとのフォント仕様

### Default スタイルのフォントマップ

```
キーボード全体 (fontFamily: "Arial", Font.Normal)
│
├── 通常キー (keyPanel)
│   ├── メインテキスト .......... 60px * scaleHint
│   ├── 小テキスト (右上隅) ..... 60px * scaleHint
│   └── 大文字化 ............... uppercased ? AllUppercase : MixedCase
│
├── エンターキー (enterKeyPanel)
│   └── ラベルテキスト .......... 50px * scaleHint, AllUppercase
│       └── fontSizeMode: Text.HorizontalFit (横幅に合わせて縮小)
│
├── スペースキー (spaceKeyPanel)
│   └── 言語名表示 .............. 60px * scaleHint
│
├── 記号キー (symbolKeyPanel)
│   └── 表示テキスト ............ 60px * scaleHint, AllUppercase
│
├── モードキー (modeKeyPanel)
│   └── 表示テキスト ............ 60px * scaleHint, AllUppercase
│
├── 文字プレビュー (characterPreviewDelegate)
│   ├── メイン文字 .............. 82px * scaleHint
│   │   └── fontSizeMode: Text.VerticalFit
│   └── フリック方向文字 ........ 62px * scaleHint (上下左右)
│       └── opacity: 0.8
│
├── 代替キーリスト (alternateKeysListDelegate)
│   └── 候補文字 ................ 60px * scaleHint
│
├── 選択リスト/予測変換 (selectionListDelegate)
│   └── 候補テキスト ............ 44px * scaleHint
│
├── ポップアップリスト (popupListDelegate)
│   └── 候補テキスト ............ cursorRectangle.height * 0.8 (動的)
│
├── 言語リスト (languageListDelegate)
│   └── 言語名テキスト .......... 44px * scaleHint
│
├── 手書き入力モード表示 (hwrInputModeIndicator)
│   └── モード表示テキスト ...... 44px * scaleHint
│
└── フルスクリーン入力 (fullScreenInputFont)
    └── 入力テキスト ............ 44px * scaleHint
```

### Retro スタイルの差異

| UI 要素 | Default | Retro | 備考 |
|---|---|---|---|
| 通常キー | 60px, Normal | 82px, Bold | Retro は `Text.Fit` でフィッティング |
| 通常キー (ハイライト時) | 変更なし | 74px, `letterSpacing: -5` | 色も変化 (#c5a96f) |
| エンターキー | 50px, Normal | 74px, Bold | Retro は AllUppercase |
| スペースキー | 60px, Normal | 72px, Bold | - |
| 記号キー | 60px, Normal | 74px, DemiBold | `letterSpacing: -5` |
| モードキー | 60px, Normal | 74px, DemiBold | `letterSpacing: -5`, `Text.Fit` |
| 文字プレビュー | 82px, Normal | 85px, Bold | Retro はフリックキー非対応 |
| 代替キーリスト | 60px, Normal | 52px, DemiBold | `letterSpacing: -6` |
| 選択リスト | 44px, Normal | 44px, Bold | Retro は白色テキスト |
| 手書き入力モード表示 | 44px, Normal | 72px, Bold | Retro は大きめ表示 |
| フルスクリーン入力 | 44px | 44px | 同一 |

---

## フォントプロパティ詳細

### font.family

スタイルのルートレベルで `readonly property string fontFamily` として定義され、すべてのテキスト要素がこの値を参照する。

```qml
// Default: src/styles/builtin/default/style.qml:12
readonly property string fontFamily: "Arial"

// Retro: src/styles/builtin/retro/style.qml:11
readonly property string fontFamily: "Courier"
```

### font.weight

QML の `Font` 列挙体を使用。スタイルごとに使い分けが異なる。

| 値 | 数値 | 用途 |
|---|---|---|
| `Font.Normal` | 50 | Default スタイルの全キー |
| `Font.Bold` | 75 | Retro スタイルの主要キー |
| `Font.DemiBold` | 63 | Retro スタイルの記号キー・モードキー・代替キー |

### font.pixelSize

すべて `<基準値> * scaleHint` の形式。使用される基準値の一覧:

| 基準値 (px) | 用途 |
|---|---|
| 82 | Default: 文字プレビュー (メイン)、Retro: 通常キー |
| 85 | Retro: 文字プレビュー |
| 74 | Retro: エンターキー、ハイライト時キー |
| 72 | Retro: スペースキー、手書きモード表示 |
| 62 | Default: フリック方向文字 |
| 60 | Default: 通常キー、代替キー、記号キー |
| 52 | Retro: 代替キーリスト |
| 50 | Default: エンターキー |
| 44 | 両スタイル共通: 選択リスト、言語リスト、フルスクリーン入力 |

### font.capitalization

大文字化の制御は入力状態に連動する:

```qml
capitalization: control.uppercased ? Font.AllUppercase : Font.MixedCase
```

使用される値:

| 値 | 適用場面 |
|---|---|
| `Font.AllUppercase` | 機能キー（記号、エンター、モード）、CapsLock 有効時 |
| `Font.MixedCase` | 通常キーの Shift 押下時 |
| `Font.AllLowercase` | 通常キーの Shift 未押下時、手書きモード表示 |

### font.letterSpacing

Default スタイルでは使用されない。Retro スタイルのみ、特定のキーで負のレタースペーシングを適用:

```qml
// Retro スタイルのみ
letterSpacing: -5 * scaleHint   // 記号キー、モードキー、ハイライト時
letterSpacing: -6 * scaleHint   // 代替キーリスト
```

### fontSizeMode (テキストフィッティング)

テキストが指定領域に収まるよう自動縮小する機能:

| 値 | 使用箇所 |
|---|---|
| `Text.HorizontalFit` | エンターキーのラベル（横幅に合わせて縮小） |
| `Text.VerticalFit` | 文字プレビュー（高さに合わせて縮小） |
| `Text.Fit` | Retro の通常キー（縦横両方向でフィット） |

---

## フォールバックとUnicode対応

### フォールバック戦略

Qt Virtual Keyboard はカスタムフォールバックコードを持たず、**Qt のフォントエンジンに内蔵されたフォント代替機構**に完全に依存する:

1. **第1段階**: スタイルで指定されたフォントファミリー（Arial / Courier）
2. **第2段階**: Qt のフォントマッチング — システムに指定フォントがない場合、Qt が最も近いフォントを自動選択
3. **第3段階**: Unicode カバレッジ不足時、Qt が文字ごとに代替フォントを使用

### 多言語対応

手書き入力モードインジケーターのテキスト表示で、各言語・文字体系に対応した文字列が使用されている:

```
InputMode          表示テキスト         文字体系
─────────────────────────────────────────────
Latin              "Abc"               ラテン文字
Numeric            "123"               ASCII 数字
Numeric (ar/fa)    "٠١٢"               アラビア数字
Greek              "ΑΒΓ"               ギリシャ文字
Cyrillic           "АБВ"               キリル文字
Arabic             "أ‌ب‌ج"              アラビア文字
Arabic (fa)        "ا‌ب‌پ"              ペルシャ文字
Hebrew             "אבג"               ヘブライ文字
ChineseHandwriting "中文"               中国語簡体字
JapaneseHandwriting "日本語"            日本語
KoreanHandwriting  "한국어"             韓国語
Thai               "กขค"               タイ文字
```

> **参照**: `src/styles/builtin/default/style.qml:896-924`

これらの文字を正しく表示するためには、実行環境のシステムに各文字体系をカバーするフォントがインストールされている必要がある。

---

## フルスクリーン入力モードのフォント

フルスクリーン入力モードでは、キーボード上部に専用のテキスト入力エリアが表示される。このエリアのフォントは `KeyboardStyle.fullScreenInputFont` プロパティで管理される。

### 構成

```
ShadowInputControl (src/components/ShadowInputControl.qml)
  └── TextInput
        └── font: keyboard.style.fullScreenInputFont
```

`ShadowInputControl.qml:80` で以下のようにバインドされる:

```qml
font: keyboard.style.fullScreenInputFont
```

各スタイルでの設定:

```qml
// Default: src/styles/builtin/default/style.qml:1175
fullScreenInputFont.pixelSize: 44 * scaleHint

// Retro: 同様に 44 * scaleHint
```

両スタイルとも同一のフォントサイズを使用。フォントファミリーは `fullScreenInputFont` の `family` プロパティに明示的な指定がないため、Qt のデフォルト（通常はシステムのデフォルトフォント）が使用される。

---

## カスタムスタイル作成ガイド

### フォント関連で必ず設定すべきプロパティ

カスタムスタイルを作成する際、以下のフォントプロパティを定義する必要がある:

```qml
KeyboardStyle {
    // 1. フォントファミリーの定義
    readonly property string fontFamily: "YourFontFamily"

    // 2. デザイン基準サイズ（scaleHint の計算に使用）
    keyboardDesignWidth: 2560
    keyboardDesignHeight: 800

    // 3. キーパネルでのフォント使用例
    keyPanel: KeyPanel {
        Text {
            text: control.displayText
            font {
                family: fontFamily
                weight: Font.Normal
                pixelSize: 60 * scaleHint    // 必ず scaleHint を掛ける
                capitalization: control.uppercased
                    ? Font.AllUppercase : Font.MixedCase
            }
        }
    }

    // 4. フルスクリーン入力フォント
    fullScreenInputFont.pixelSize: 44 * scaleHint
}
```

### TextMetrics の活用

言語リストなど、テキストのレイアウト寸法に基づいてUIサイズを決定する場面では `TextMetrics` を使用する:

```qml
// src/styles/builtin/default/style.qml:1094-1102
TextMetrics {
    id: languageNameTextMetrics
    font {
        family: fontFamily
        weight: Font.Normal
        pixelSize: 44 * scaleHint
    }
    text: "X"  // 基準文字で高さ・幅を計測
}
```

この `TextMetrics` の `width` や `height` を使ってリスト項目のサイズを決定する:

```qml
width: languageNameTextMetrics.width * 17   // 文字幅 × 17 で全体幅を算出
height: languageNameTextMetrics.height + ...  // 文字高さ + マージン
```

### 注意点

- **`pixelSize` には必ず `scaleHint` を掛ける**: 固定値を使うとデバイスサイズによって文字が大きすぎたり小さすぎたりする
- **システムフォントへの依存を意識する**: 指定したフォントが環境に存在しない場合、Qt のフォールバック任せになる
- **`fontSizeMode` の活用**: キーサイズが可変の場合、`Text.Fit` 等を使うとテキストが領域に収まるよう自動調整される

---

## ビルド構成とスタイル選択

### CMake によるスタイル選択

`src/virtualkeyboard/configure.cmake` で、使用するスタイルをビルド時に選択できる:

```bash
# Default スタイル (Arial) — デフォルト
cmake -DINPUT_vkb_style=default ..

# Retro スタイル (Courier)
cmake -DINPUT_vkb_style=retro ..

# 組み込みスタイルなし（カスタムスタイル専用）
cmake -DINPUT_vkb_style=none ..
```

### Feature フラグ

| フラグ | 説明 |
|---|---|
| `vkb-default-style` | Default スタイルを有効化（デフォルト ON） |
| `vkb-retro-style` | Retro スタイルをデフォルトとして使用 |
| `vkb-no-builtin-style` | すべての組み込みスタイルを無効化 |

> **参照**: `src/virtualkeyboard/configure.cmake:48-63`

### スタイルリソースの構成

`src/styles/builtin/CMakeLists.txt` でスタイルリソースが QML モジュールとして登録される。フォントファイルは含まれず、QML ファイルと画像リソースのみ:

- **Default**: `style.qml` + 13 画像ファイル（SVG）
- **Retro**: `style.qml` + 39 画像ファイル（PNG, SVG）

---

## ファイル構成マップ

```
src/
├── styles/
│   ├── KeyboardStyle.qml          ← スタイル基底型（scaleHint, fullScreenInputFont 定義）
│   ├── KeyPanel.qml               ← キーパネル基底型（デバッグ用 font.pixelSize: 12）
│   ├── SelectionListItem.qml      ← 選択リスト項目基底型
│   ├── TraceCanvas.qml            ← 手書きトレース描画
│   ├── TraceInputKeyPanel.qml     ← 手書き入力キーパネル
│   └── builtin/
│       ├── CMakeLists.txt         ← リソースバンドル定義
│       ├── default/
│       │   ├── style.qml          ← Default スタイル (Arial, Font.Normal)
│       │   └── images/            ← SVG アイコン (13 ファイル)
│       └── retro/
│           ├── style.qml          ← Retro スタイル (Courier, Font.Bold)
│           └── images/            ← PNG/SVG 画像 (39 ファイル)
├── components/
│   └── ShadowInputControl.qml    ← フルスクリーン入力（font: keyboard.style.fullScreenInputFont）
├── virtualkeyboard/
│   ├── configure.cmake            ← スタイル選択 Feature フラグ
│   ├── qt_cmdline.cmake           ← コマンドラインオプション (-vkb-style)
│   └── doc/
│       └── snippets/
│           └── qtvirtualkeyboard-custom-language-popup.qml ← カスタム言語ポップアップ例
└── settings/
    └── qquickvirtualkeyboardsettings_p.h ← styleName, layoutPath 設定
```

---

## 付録: フォントプロパティ一覧表

### Default スタイル — 全フォント宣言

| UI 要素 | pixelSize (基準) | weight | capitalization | fontSizeMode | 参照行 |
|---|---|---|---|---|---|
| keyPanel (メイン) | 60 | Normal | uppercased制御 | — | default/style.qml:117 |
| keyPanel (小テキスト) | 60 | Normal | uppercased制御 | — | default/style.qml:93 |
| enterKeyPanel | 50 | Normal | AllUppercase | HorizontalFit | default/style.qml:304 |
| spaceKeyPanel | 60 | Normal | — | — | default/style.qml:475 |
| symbolKeyPanel | 60 | Normal | AllUppercase | — | default/style.qml:518 |
| modeKeyPanel | 60 | Normal | AllUppercase | — | default/style.qml:570 |
| characterPreview (メイン) | 82 | Normal | — | VerticalFit | default/style.qml:692 |
| characterPreview (フリック) | 62 | Normal | — | VerticalFit | default/style.qml:710 |
| alternateKeysList | 60 | Normal | — | — | default/style.qml:792 |
| selectionList | 44 | Normal | — | — | default/style.qml:840 |
| popupList | cursor*0.8 | Normal | — | — | default/style.qml:1032 |
| languageList | 44 | Normal | — | — | default/style.qml:1091 |
| hwrInputModeIndicator | 44 | Normal | Shift制御 | — | default/style.qml:934 |
| fullScreenInputFont | 44 | — | — | — | default/style.qml:1175 |

### Retro スタイル — 全フォント宣言

| UI 要素 | pixelSize (基準) | weight | capitalization | fontSizeMode | letterSpacing | 参照行 |
|---|---|---|---|---|---|---|
| keyPanel (通常) | 82 | Bold | uppercased制御 | Fit | — | retro/style.qml:84 |
| keyPanel (ハイライト) | 74 | Bold | — | — | -5 | retro/style.qml:94 |
| enterKeyPanel | 74 | Bold | AllUppercase | HorizontalFit | — | retro/style.qml:290 |
| spaceKeyPanel | 72 | Bold | — | — | — | retro/style.qml:483 |
| symbolKeyPanel | 74 | DemiBold | AllUppercase | — | -5 | retro/style.qml:529 |
| modeKeyPanel | 74 | DemiBold | AllUppercase | Fit | -5 | retro/style.qml:599 |
| characterPreview | 85 | Bold | — | — | -5 (複数文字時) | retro/style.qml:693 |
| alternateKeysList | 52 | DemiBold | — | — | -6 | retro/style.qml:724 |
| selectionList | 44 | Bold | — | — | — | retro/style.qml:795 |
| popupList | cursor*0.8 | Normal | — | — | — | retro/style.qml:986 |
| languageList | 44 | Normal | — | — | — | retro/style.qml:1052 |
| hwrInputModeIndicator | 72 | Bold | Shift制御 | — | — | retro/style.qml:895 |
| fullScreenInputFont | 44 | — | — | — | — | — |
