# Qt Virtual Keyboard 5.12.10 - レイアウトシステムガイド

**バージョン**: 5.12.10
**最終更新**: 2026-01-10
**対象読者**: 開発者、多言語対応実装者

---

## 目次

1. [レイアウトシステム概要](#1-レイアウトシステム概要)
2. [レイアウトの構造](#2-レイアウトの構造)
3. [カスタムレイアウトの作成](#3-カスタムレイアウトの作成)
4. [多言語対応](#4-多言語対応)
5. [高度な機能](#5-高度な機能)

---

## 1. レイアウトシステム概要

### 1.1 レイアウトとは

**レイアウト**は、キーボード上のキー配置を定義するQMLファイルです。

```
レイアウト = キーの配置 + 動作定義
```

### 1.2 レイアウトの種類

各言語ごとに、以下のレイアウトタイプを定義できます:

| タイプ | ファイル名 | 用途 |
|--------|-----------|------|
| **main** | `main.qml` | 基本的な文字入力 (QWERTY等) |
| **symbols** | `symbols.qml` | 記号・特殊文字 |
| **numbers** | `numbers.qml` | 数値入力（計算機風） |
| **digits** | `digits.qml` | 数字のみ（0-9） |
| **dialpad** | `dialpad.qml` | 電話ダイヤル（1-9, *, #） |
| **handwriting** | `handwriting.qml` | 手書き入力エリア |

### 1.3 レイアウトディレクトリ構造

```
/src/virtualkeyboard/content/layouts/
├── fallback/                    # デフォルトレイアウト
│   ├── main.qml
│   ├── symbols.qml
│   ├── numbers.qml
│   ├── digits.qml
│   └── dialpad.qml
│
├── en_GB/                       # 英語（イギリス）
│   ├── main.qml
│   ├── symbols.qml
│   ├── numbers.qml
│   ├── digits.fallback          # fallbackマーカー
│   └── dialpad.fallback         # fallbackマーカー
│
├── ja_JP/                       # 日本語
│   ├── main.qml
│   ├── symbols.qml
│   ├── numbers.qml
│   ├── digits.fallback
│   ├── dialpad.fallback
│   └── handwriting.qml
│
└── ... (40+言語)
```

**`.fallback`ファイル**:
- 空のマーカーファイル
- その言語固有のレイアウトが不要で、fallbackを使用することを示す

### 1.4 レイアウトの検索順序

```
1. 言語固有のレイアウト
   例: layouts/ja_JP/main.qml

2. .fallbackマーカー確認
   例: layouts/ja_JP/main.fallback が存在する場合

3. Fallbackレイアウト
   例: layouts/fallback/main.qml

4. レイアウトなし
   → mainレイアウトを使用
```

---

## 2. レイアウトの構造

### 2.1 基本テンプレート

```qml
import QtQuick 2.0
import QtQuick.VirtualKeyboard 2.1

KeyboardLayout {
    // 入力モード指定
    inputMode: InputEngine.InputMode.Latin

    // デフォルトキー幅
    keyWeight: 160

    // キー行を定義
    KeyboardRow {
        // キーを配置
        Key {
            key: Qt.Key_Q
            text: "q"
        }
        // ... 他のキー
    }

    KeyboardRow {
        // 2行目
    }

    // ... 他の行
}
```

### 2.2 KeyboardLayoutプロパティ

```qml
KeyboardLayout {
    // 入力モード (必須ではない)
    inputMode: InputEngine.InputMode.Latin

    // 入力メソッド設定
    inputMethod: null  // カスタム入力メソッド

    // デフォルトキー幅（weightの基準値）
    keyWeight: 160

    // 代替キーヒント表示フラグ
    smallTextVisible: true

    // 入力メソッド共有レイアウト
    sharedLayouts: []

    // カスタム入力メソッド生成関数
    function createInputMethod() {
        return null  // オーバーライド可能
    }
}
```

### 2.3 KeyboardRowプロパティ

```qml
KeyboardRow {
    // この行のデフォルトキー幅
    // 省略時はKeyboardLayoutのkeyWeightを継承
    keyWeight: 160

    // 代替キーヒント表示
    // 省略時はKeyboardLayoutのsmallTextVisibleを継承
    smallTextVisible: true

    // ... キー定義
}
```

### 2.4 Weight（重み）システム

**Weightの仕組み**:
- キーの相対的な幅を指定
- 行内のweightの合計で比例配分

**例**:
```qml
KeyboardRow {
    keyWeight: 100  // デフォルト幅

    Key { text: "Q"; weight: 100 }  // 100単位幅
    Key { text: "W"; weight: 100 }  // 100単位幅
    Key { text: "E"; weight: 100 }  // 100単位幅
    BackspaceKey { weight: 200 }    // 200単位幅 (2倍)

    // 合計: 100 + 100 + 100 + 200 = 500単位
    // 画面幅が1000pxの場合:
    //   Q: 1000 * (100/500) = 200px
    //   W: 1000 * (100/500) = 200px
    //   E: 1000 * (100/500) = 200px
    //   Backspace: 1000 * (200/500) = 400px
}
```

### 2.5 キー種別

#### 基本キー

```qml
Key {
    key: Qt.Key_A               // Qtキーコード
    text: "a"                   // 入力テキスト
    displayText: "A"            // 表示テキスト（省略時はtext）
    weight: 160                 // キー幅
    alternativeKeys: "aäåãâàá"  // 長押し代替キー
}
```

#### 機能キー

| キー型 | 説明 | 例 |
|-------|------|-----|
| `BackspaceKey` | バックスペース | `BackspaceKey { weight: 200 }` |
| `EnterKey` | Enter/改行 | `EnterKey { weight: 283 }` |
| `ShiftKey` | Shift/大文字切替 | `ShiftKey {}` |
| `SpaceKey` | スペース | `SpaceKey { weight: 864 }` |
| `SymbolModeKey` | 記号モード切替 | `SymbolModeKey { weight: 217 }` |
| `ChangeLanguageKey` | 言語切替 | `ChangeLanguageKey {}` |
| `HideKeyboardKey` | キーボード非表示 | `HideKeyboardKey { weight: 204 }` |
| `HandwritingModeKey` | 手書きモード | `HandwritingModeKey {}` |
| `FillerKey` | 空白（配置調整用） | `FillerKey { weight: 56 }` |

---

## 3. カスタムレイアウトの作成

### 3.1 新しい言語レイアウトの追加

#### Step 1: ディレクトリ作成

```bash
cd /src/virtualkeyboard/content/layouts/
mkdir my_LOCALE  # 例: my_US
```

#### Step 2: main.qmlを作成

```qml
// my_LOCALE/main.qml
import QtQuick 2.0
import QtQuick.VirtualKeyboard 2.1

KeyboardLayout {
    inputMode: InputEngine.InputMode.Latin
    keyWeight: 160

    // 1行目: QWERTY
    KeyboardRow {
        Key { key: Qt.Key_Q; text: "q" }
        Key { key: Qt.Key_W; text: "w" }
        Key { key: Qt.Key_E; text: "e"; alternativeKeys: "êeëèé" }
        Key { key: Qt.Key_R; text: "r" }
        Key { key: Qt.Key_T; text: "t" }
        Key { key: Qt.Key_Y; text: "y" }
        Key { key: Qt.Key_U; text: "u"; alternativeKeys: "űūũûüuùú" }
        Key { key: Qt.Key_I; text: "i"; alternativeKeys: "îïīĩiìí" }
        Key { key: Qt.Key_O; text: "o"; alternativeKeys: "œøõôöòóo" }
        Key { key: Qt.Key_P; text: "p" }
        BackspaceKey {}
    }

    // 2行目: ASDF...
    KeyboardRow {
        FillerKey { weight: 56 }  // センタリング用
        Key { key: Qt.Key_A; text: "a"; alternativeKeys: "aäåãâàá" }
        Key { key: Qt.Key_S; text: "s" }
        Key { key: Qt.Key_D; text: "d" }
        Key { key: Qt.Key_F; text: "f" }
        Key { key: Qt.Key_G; text: "g" }
        Key { key: Qt.Key_H; text: "h" }
        Key { key: Qt.Key_J; text: "j" }
        Key { key: Qt.Key_K; text: "k" }
        Key { key: Qt.Key_L; text: "l" }
        EnterKey { weight: 283 }
    }

    // 3行目: ZXCV...
    KeyboardRow {
        keyWeight: 156
        ShiftKey {}
        Key { key: Qt.Key_Z; text: "z" }
        Key { key: Qt.Key_X; text: "x" }
        Key { key: Qt.Key_C; text: "c" }
        Key { key: Qt.Key_V; text: "v" }
        Key { key: Qt.Key_B; text: "b" }
        Key { key: Qt.Key_N; text: "n" }
        Key { key: Qt.Key_M; text: "m" }
        Key { key: Qt.Key_Comma; text: "," }
        Key { key: Qt.Key_Period; text: "." }
        ShiftKey { weight: 204 }
    }

    // 4行目: 機能キー
    KeyboardRow {
        keyWeight: 154
        SymbolModeKey { weight: 217 }
        ChangeLanguageKey { weight: 154 }
        SpaceKey { weight: 864 }
        HideKeyboardKey { weight: 204 }
    }
}
```

#### Step 3: symbols.qmlを作成

```qml
// my_LOCALE/symbols.qml
import QtQuick 2.0
import QtQuick.VirtualKeyboard 2.1

KeyboardLayout {
    inputMode: InputEngine.InputMode.Latin
    keyWeight: 160

    KeyboardRow {
        Key { key: Qt.Key_1; text: "1" }
        Key { key: Qt.Key_2; text: "2" }
        Key { key: Qt.Key_3; text: "3" }
        Key { key: Qt.Key_4; text: "4" }
        Key { key: Qt.Key_5; text: "5" }
        Key { key: Qt.Key_6; text: "6" }
        Key { key: Qt.Key_7; text: "7" }
        Key { key: Qt.Key_8; text: "8" }
        Key { key: Qt.Key_9; text: "9" }
        Key { key: Qt.Key_0; text: "0" }
        BackspaceKey {}
    }

    KeyboardRow {
        FillerKey { weight: 56 }
        Key { key: Qt.Key_At; text: "@" }
        Key { key: Qt.Key_NumberSign; text: "#" }
        Key { key: Qt.Key_Dollar; text: "$" }
        Key { key: Qt.Key_Percent; text: "%" }
        Key { key: Qt.Key_Ampersand; text: "&" }
        Key { key: Qt.Key_Asterisk; text: "*" }
        Key { key: Qt.Key_Minus; text: "-" }
        Key { key: Qt.Key_Plus; text: "+" }
        Key { key: Qt.Key_Equal; text: "=" }
        EnterKey { weight: 283 }
    }

    KeyboardRow {
        keyWeight: 156
        ShiftKey {}  // 記号の大文字バリエーション
        Key { key: Qt.Key_Exclam; text: "!" }
        Key { key: Qt.Key_QuoteDbl; text: "\"" }
        Key { key: Qt.Key_ParenLeft; text: "(" }
        Key { key: Qt.Key_ParenRight; text: ")" }
        Key { key: Qt.Key_Slash; text: "/" }
        Key { key: Qt.Key_Colon; text: ":" }
        Key { key: Qt.Key_Semicolon; text: ";" }
        Key { key: Qt.Key_Question; text: "?" }
        ShiftKey { weight: 204 }
    }

    KeyboardRow {
        keyWeight: 154
        SymbolModeKey { weight: 217; mode: true }  // 元に戻る
        ChangeLanguageKey { weight: 154 }
        SpaceKey { weight: 864 }
        HideKeyboardKey { weight: 204 }
    }
}
```

#### Step 4: fallbackマーカー作成

```bash
# numbers, digits, dialpadはfallbackを使用
touch my_LOCALE/numbers.fallback
touch my_LOCALE/digits.fallback
touch my_LOCALE/dialpad.fallback
```

#### Step 5: リソースに追加

```qmake
# virtualkeyboard.pro
RESOURCES += virtualkeyboard_my_locale.qrc
```

```xml
<!-- virtualkeyboard_my_locale.qrc -->
<RCC version="1.0">
<qresource prefix="/QtQuick/VirtualKeyboard/content/layouts">
    <file>my_LOCALE/main.qml</file>
    <file>my_LOCALE/symbols.qml</file>
    <file>my_LOCALE/numbers.fallback</file>
    <file>my_LOCALE/digits.fallback</file>
    <file>my_LOCALE/dialpad.fallback</file>
</qresource>
</RCC>
```

#### Step 6: ビルド設定

```qmake
# virtualkeyboard.pro
contains(CONFIG, lang-my.*): {
    DEFINES += HAVE_MY_LAYOUT
    RESOURCES += virtualkeyboard_my_locale.qrc
}
```

ビルド時:
```bash
qmake CONFIG+=lang-my
make
```

### 3.2 特殊レイアウトの例

#### 数字キーパッド（計算機風）

```qml
// numbers.qml
import QtQuick 2.0
import QtQuick.VirtualKeyboard 2.1

KeyboardLayout {
    inputMode: InputEngine.InputMode.Numeric
    keyWeight: 160

    KeyboardRow {
        Key { key: Qt.Key_7; text: "7" }
        Key { key: Qt.Key_8; text: "8" }
        Key { key: Qt.Key_9; text: "9" }
        BackspaceKey {}
    }

    KeyboardRow {
        Key { key: Qt.Key_4; text: "4" }
        Key { key: Qt.Key_5; text: "5" }
        Key { key: Qt.Key_6; text: "6" }
        Key { key: Qt.Key_Minus; text: "-" }
    }

    KeyboardRow {
        Key { key: Qt.Key_1; text: "1" }
        Key { key: Qt.Key_2; text: "2" }
        Key { key: Qt.Key_3; text: "3" }
        Key { key: Qt.Key_Plus; text: "+" }
    }

    KeyboardRow {
        ChangeLanguageKey {}
        Key { key: Qt.Key_0; text: "0"; weight: 320 }
        Key { key: Qt.Key_Period; text: "." }
        HideKeyboardKey {}
    }
}
```

#### ダイヤルパッド（電話風）

```qml
// dialpad.qml
import QtQuick 2.0
import QtQuick.VirtualKeyboard 2.1

KeyboardLayout {
    inputMode: InputEngine.InputMode.Dialable
    keyWeight: 160

    KeyboardRow {
        Key { key: Qt.Key_1; text: "1" }
        Key { key: Qt.Key_2; text: "2"; alternativeKeys: "2abc" }
        Key { key: Qt.Key_3; text: "3"; alternativeKeys: "3def" }
    }

    KeyboardRow {
        Key { key: Qt.Key_4; text: "4"; alternativeKeys: "4ghi" }
        Key { key: Qt.Key_5; text: "5"; alternativeKeys: "5jkl" }
        Key { key: Qt.Key_6; text: "6"; alternativeKeys: "6mno" }
    }

    KeyboardRow {
        Key { key: Qt.Key_7; text: "7"; alternativeKeys: "7pqrs" }
        Key { key: Qt.Key_8; text: "8"; alternativeKeys: "8tuv" }
        Key { key: Qt.Key_9; text: "9"; alternativeKeys: "9wxyz" }
    }

    KeyboardRow {
        Key { key: Qt.Key_Asterisk; text: "*"; alternativeKeys: "*+" }
        Key { key: Qt.Key_0; text: "0"; alternativeKeys: "0 " }
        Key { key: Qt.Key_NumberSign; text: "#" }
    }

    KeyboardRow {
        BackspaceKey { weight: 240 }
        HideKeyboardKey { weight: 240 }
    }
}
```

---

## 4. 多言語対応

### 4.1 言語検出と切り替え

#### 利用可能言語の設定

```qml
// main.qml
import QtQuick.VirtualKeyboard.Settings 2.2

Component.onCompleted: {
    // アクティブな言語を制限
    VirtualKeyboardSettings.activeLocales = ["en_GB", "ja_JP", "de_DE"]

    // 利用可能な全言語を確認
    console.log("Available:", VirtualKeyboardSettings.availableLocales)
}
```

#### プログラムによる言語切り替え

```qml
import QtQuick.VirtualKeyboard 2.3

Button {
    text: "日本語"
    onClicked: {
        InputContext.locale = "ja_JP"
    }
}
```

### 4.2 言語固有の入力メソッド

```qml
// ja_JP/main.qml
import QtQuick 2.0
import QtQuick.VirtualKeyboard 2.1

KeyboardLayout {
    // 日本語入力モード
    inputMode: InputEngine.InputMode.Hiragana

    // カスタム入力メソッド生成
    function createInputMethod() {
        return Qt.createQmlObject('
            import QtQuick 2.0
            import QtQuick.VirtualKeyboard.Plugins 2.3
            JapaneseInputMethod {}
        ', this)
    }

    // ... キー定義
}
```

### 4.3 RTL (Right-to-Left) 言語対応

```qml
// ar_AR/main.qml (アラビア語)
import QtQuick 2.0
import QtQuick.VirtualKeyboard 2.1

KeyboardLayout {
    inputMode: InputEngine.InputMode.Arabic

    // RTLレイアウト自動適用
    LayoutMirroring.enabled: true
    LayoutMirroring.childrenInherit: true

    KeyboardRow {
        // RTLでは右から左に配置される
        Key { text: "ض" }
        Key { text: "ص" }
        // ...
    }
}
```

---

## 5. 高度な機能

### 5.1 動的キー生成

```qml
KeyboardLayout {
    keyWeight: 160

    KeyboardRow {
        Repeater {
            model: ["Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"]
            Key {
                text: modelData.toLowerCase()
                displayText: modelData
            }
        }
        BackspaceKey {}
    }
}
```

### 5.2 カスタムキー

```qml
KeyboardLayout {
    KeyboardRow {
        // 絵文字キー
        Key {
            key: 0xE000  // カスタムキーコード
            text: "😀"
            displayText: "😀"
            alternativeKeys: ["😀", "😃", "😄", "😁", "😆"]
        }

        // マクロキー（複数文字入力）
        Key {
            key: Qt.Key_unknown
            text: "info@example.com"
            displayText: "Email"
            functionKey: true
        }
    }
}
```

### 5.3 条件付きキー表示

```qml
KeyboardLayout {
    KeyboardRow {
        // URLフィールドの場合のみ表示
        Key {
            key: Qt.Key_Period
            text: "."
            visible: InputContext.inputMethodHints & Qt.ImhUrlCharactersOnly
        }

        // 数値入力フィールドでは非表示
        Key {
            key: Qt.Key_Space
            text: " "
            enabled: !(InputContext.inputMethodHints & Qt.ImhDigitsOnly)
        }
    }
}
```

### 5.4 入力メソッド統合

```qml
KeyboardLayout {
    // カスタム入力メソッド
    function createInputMethod() {
        return Qt.createQmlObject('
            import QtQuick 2.0
            import QtQuick.VirtualKeyboard 2.1

            MultitapInputMethod {
                // T9風入力
            }
        ', this, "multitapInputMethod")
    }

    // 複数レイアウトで入力メソッドを共有
    sharedLayouts: ["symbols"]

    // ... キー定義
}
```

### 5.5 トレースベース入力（手書き）

```qml
// handwriting.qml
import QtQuick 2.0
import QtQuick.VirtualKeyboard 2.1

KeyboardLayout {
    KeyboardRow {
        TraceInputKey {
            objectName: "hwrInputArea"
            patternRecognitionMode: InputEngine.PatternRecognitionMode.Handwriting

            // 手書きエリア全体を使用
            weight: function() {
                return Math.round(keyboard.width)
            }
        }
    }
}
```

---

## まとめ

Qt Virtual Keyboard 5.12.10のレイアウトシステムは、柔軟で拡張性の高い設計です。

**重要なポイント**:
1. **KeyboardLayout**でレイアウトの骨組みを定義
2. **Weight**システムで相対的なキーサイズを指定
3. **代替キー**で1つのキーから複数文字入力
4. **fallback**システムで言語ごとの差分のみ実装
5. **カスタム入力メソッド**で言語固有のロジックを統合

**ベストプラクティス**:
- 既存のレイアウト（fallback/main.qml等）を参考にする
- weightの合計が各行で一致するよう調整
- alternativeKeysで関連文字をグループ化
- .fallbackファイルで重複を避ける

これで、Qt Virtual Keyboard 5.12.10の改造元仕様ドキュメント一式が完成しました。
