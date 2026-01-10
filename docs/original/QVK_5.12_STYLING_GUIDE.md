# Qt Virtual Keyboard 5.12.10 - スタイリングシステムガイド

**バージョン**: 5.12.10
**最終更新**: 2026-01-10
**対象読者**: UI/UXデザイナー、フロントエンド開発者

---

## 目次

1. [スタイリングシステム概要](#1-スタイリングシステム概要)
2. [KeyboardStyleの構造](#2-keyboardstyleの構造)
3. [カスタムスタイルの作成](#3-カスタムスタイルの作成)
4. [実装例](#4-実装例)
5. [ベストプラクティス](#5-ベストプラクティス)

---

## 1. スタイリングシステム概要

### 1.1 設計思想

Qt Virtual Keyboardのスタイリングシステムは、**Component Delegateパターン**を採用しています。

```
KeyboardStyle (基底クラス)
    ↓
  プロパティでComponentを定義
    ↓
  Keyboardが実行時にロード
    ↓
  カスタマイズ可能なUI
```

**利点**:
- **完全なカスタマイズ**: すべてのUI要素を置き換え可能
- **スケーラブル**: scaleHintで自動スケーリング
- **再利用性**: スタイルを複数のキーボードで共有
- **非侵襲的**: コアロジックを変更せずにUI変更

### 1.2 スタイルの種類

Qt Virtual Keyboard 5.12.10には2つの組み込みスタイルがあります：

| スタイル | 説明 | ファイルパス |
|---------|------|------------|
| **default** | 標準スタイル (ダーク系) | `/src/virtualkeyboard/content/styles/default/style.qml` |
| **retro** | レトロスタイル (ライト系) | `/src/virtualkeyboard/content/styles/retro/style.qml` |

### 1.3 スタイルの適用方法

#### 方法1: QMLから設定

```qml
import QtQuick 2.0
import QtQuick.VirtualKeyboard.Settings 2.2

Component.onCompleted: {
    VirtualKeyboardSettings.style = "qrc:/custom/mystyle.qml"
}
```

#### 方法2: C++から設定

```cpp
#include <QtVirtualKeyboard/qvirtualkeyboardsettings_p.h>

QVirtualKeyboardSettings::instance()->setStyle("qrc:/custom/mystyle.qml");
```

#### 方法3: 環境変数

```bash
export QT_VIRTUALKEYBOARD_STYLE=retro
./myapp
```

#### 方法4: ビルド時デフォルト指定

```qmake
# virtualkeyboard.pro
retro-style {
    DEFINES += QT_VIRTUALKEYBOARD_DEFAULT_STYLE=\"retro\"
}
```

---

## 2. KeyboardStyleの構造

### 2.1 基本プロパティ

#### 設計サイズ

```qml
KeyboardStyle {
    // 設計時の基準サイズ
    keyboardDesignWidth: 2560   // 横幅（ピクセル）
    keyboardDesignHeight: 800   // 高さ（ピクセル）

    // 実際の高さ（実行時に設定される）
    keyboardHeight: 300  // 例: 実際の高さ

    // スケールヒント（自動計算）
    readonly property real scaleHint: keyboardHeight / keyboardDesignHeight
    // 例: 300 / 800 = 0.375
}
```

**scaleHint**の使い方:
```qml
Text {
    // 設計時: 52px
    // 実行時: 52 * 0.375 = 19.5px (自動スケーリング)
    font.pixelSize: 52 * scaleHint
}
```

#### マージン設定

```qml
KeyboardStyle {
    // 相対マージン（0.0 - 1.0）
    keyboardRelativeLeftMargin: 0.0445    // 4.45%
    keyboardRelativeRightMargin: 0.0445   // 4.45%
    keyboardRelativeTopMargin: 0.01625    // 1.625%
    keyboardRelativeBottomMargin: 0.1075  // 10.75%
}
```

### 2.2 Component Delegateプロパティ

#### キーパネルDelegate

各キー種別に対応するUIコンポーネント:

```qml
KeyboardStyle {
    // 背景
    property Component keyboardBackground

    // 通常キー
    property Component keyPanel

    // 特殊キー
    property Component backspaceKeyPanel
    property Component languageKeyPanel
    property Component enterKeyPanel
    property Component hideKeyPanel
    property Component shiftKeyPanel
    property Component spaceKeyPanel
    property Component symbolKeyPanel
    property Component modeKeyPanel
    property Component handwritingKeyPanel
}
```

**使用例**:
```qml
keyPanel: KeyPanel {
    // KeyPanel.controlでキー情報にアクセス
    Rectangle {
        color: control.pressed ? "#555" : "#333"
        Text {
            text: control.displayText
            color: "white"
            font.pixelSize: 48 * scaleHint
        }
    }
}
```

#### プレビュー・候補リストDelegate

```qml
KeyboardStyle {
    // 文字プレビューバブル
    property real characterPreviewMargin: 0
    property Component characterPreviewDelegate

    // 代替キーリスト
    property real alternateKeysListItemWidth: 111
    property real alternateKeysListItemHeight: 154
    property Component alternateKeysListDelegate
    property Component alternateKeysListHighlight
    property Component alternateKeysListBackground

    // 単語候補リスト
    property real selectionListHeight: 88
    property Component selectionListDelegate
    property Component selectionListHighlight
    property Component selectionListBackground
    property Transition selectionListAdd
    property Transition selectionListRemove
}
```

#### フルスクリーンモード

```qml
KeyboardStyle {
    // フルスクリーン入力エリア
    property Component fullScreenInputContainerBackground
    property Component fullScreenInputBackground
    property real fullScreenInputMargins: 15
    property real fullScreenInputPadding: 30
    property Component fullScreenInputCursor
    property font fullScreenInputFont
    property color fullScreenInputColor: "#000"
    property color fullScreenInputSelectionColor: "#80c342"
    property color fullScreenInputSelectedTextColor: "#fff"
    property string fullScreenInputPasswordCharacter: "\u2022"
}
```

### 2.3 制御オブジェクト (control)

各Delegateでは、`control`オブジェクトを通じてキー情報にアクセスできます:

```qml
keyPanel: KeyPanel {
    Rectangle {
        // controlプロパティ
        Text {
            text: control.displayText        // 表示テキスト
            visible: !control.smallTextVisible  // 小テキスト表示フラグ
        }

        opacity: control.pressed ? 0.5 : 1.0  // 押下状態

        // その他のプロパティ
        // control.uppercased: 大文字表示フラグ
        // control.active: アクティブ状態
        // control.enabled: 有効/無効
    }
}
```

---

## 3. カスタムスタイルの作成

### 3.1 基本構造

```qml
// mystyle.qml
import QtQuick 2.0
import QtQuick.VirtualKeyboard.Styles 2.1

KeyboardStyle {
    // 1. 設計サイズ設定
    keyboardDesignWidth: 2560
    keyboardDesignHeight: 800

    // 2. マージン設定
    keyboardRelativeLeftMargin: 114 / keyboardDesignWidth
    keyboardRelativeRightMargin: 114 / keyboardDesignWidth
    keyboardRelativeTopMargin: 13 / keyboardDesignHeight
    keyboardRelativeBottomMargin: 86 / keyboardDesignHeight

    // 3. 背景
    keyboardBackground: Rectangle {
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#1a1a1a" }
            GradientStop { position: 1.0; color: "#0d0d0d" }
        }
    }

    // 4. キーパネル
    keyPanel: KeyPanel {
        // カスタムUI実装
        Rectangle {
            id: keyBackground
            anchors.fill: parent
            anchors.margins: 5 * scaleHint
            radius: 5
            color: "#2c2c2c"

            Text {
                anchors.centerIn: parent
                text: control.displayText
                font.pixelSize: 48 * scaleHint
                color: "#ffffff"
            }

            states: [
                State {
                    name: "pressed"
                    when: control.pressed
                    PropertyChanges {
                        target: keyBackground
                        color: "#4a90e2"
                    }
                }
            ]
        }
    }
}
```

### 3.2 ステップバイステップガイド

#### Step 1: テンプレートファイル作成

```bash
# プロジェクトディレクトリに作成
mkdir -p resources/styles
cd resources/styles
touch customstyle.qml
```

#### Step 2: KeyboardStyleを継承

```qml
import QtQuick 2.0
import QtQuick.VirtualKeyboard.Styles 2.1

KeyboardStyle {
    id: currentStyle

    // ここにカスタマイズを記述
}
```

#### Step 3: 設計サイズとマージンを設定

```qml
KeyboardStyle {
    // 16:9のアスペクト比
    keyboardDesignWidth: 1920
    keyboardDesignHeight: 1080

    // 画面端からの余白（5%）
    keyboardRelativeLeftMargin: 0.05
    keyboardRelativeRightMargin: 0.05
    keyboardRelativeTopMargin: 0.05
    keyboardRelativeBottomMargin: 0.05
}
```

#### Step 4: キーパネルをカスタマイズ

```qml
KeyboardStyle {
    // ... 設計サイズ設定 ...

    keyPanel: KeyPanel {
        Rectangle {
            id: keyBg
            anchors.fill: parent
            anchors.margins: 8 * scaleHint
            radius: 10

            // グラデーション
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#4a90e2" }
                GradientStop { position: 1.0; color: "#357abd" }
            }

            // テキスト
            Text {
                id: keyText
                anchors.centerIn: parent
                text: control.displayText
                font.pixelSize: 52 * scaleHint
                font.family: "Roboto"
                color: "white"
            }

            // 押下エフェクト
            states: State {
                name: "pressed"
                when: control.pressed
                PropertyChanges {
                    target: keyBg
                    scale: 0.95
                }
            }

            transitions: Transition {
                NumberAnimation {
                    properties: "scale"
                    duration: 100
                }
            }
        }
    }
}
```

#### Step 5: リソースファイルに追加

```xml
<!-- resources.qrc -->
<RCC version="1.0">
<qresource prefix="/styles">
    <file>customstyle.qml</file>
</qresource>
</RCC>
```

#### Step 6: スタイルを適用

```qml
// main.qml
import QtQuick 2.0
import QtQuick.VirtualKeyboard.Settings 2.2

ApplicationWindow {
    Component.onCompleted: {
        VirtualKeyboardSettings.style = "qrc:/styles/customstyle.qml"
    }
}
```

---

## 4. 実装例

### 4.1 シンプルなフラットスタイル

```qml
import QtQuick 2.0
import QtQuick.VirtualKeyboard.Styles 2.1

KeyboardStyle {
    keyboardDesignWidth: 2560
    keyboardDesignHeight: 800

    keyboardBackground: Rectangle {
        color: "#ecf0f1"
    }

    keyPanel: KeyPanel {
        Rectangle {
            anchors.fill: parent
            anchors.margins: 5 * scaleHint
            color: control.pressed ? "#34495e" : "#ffffff"
            border.color: "#bdc3c7"
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: control.displayText
                font.pixelSize: 48 * scaleHint
                color: control.pressed ? "#ffffff" : "#2c3e50"
            }
        }
    }

    backspaceKeyPanel: KeyPanel {
        Rectangle {
            anchors.fill: parent
            anchors.margins: 5 * scaleHint
            color: control.pressed ? "#c0392b" : "#e74c3c"

            Image {
                anchors.centerIn: parent
                source: "qrc:/images/backspace.svg"
                width: 48 * scaleHint
                height: 48 * scaleHint
            }
        }
    }

    enterKeyPanel: KeyPanel {
        Rectangle {
            anchors.fill: parent
            anchors.margins: 5 * scaleHint
            color: control.pressed ? "#27ae60" : "#2ecc71"

            Text {
                anchors.centerIn: parent
                text: control.displayText || "↵"
                font.pixelSize: 48 * scaleHint
                color: "#ffffff"
            }
        }
    }
}
```

### 4.2 グラスモルフィズムスタイル

```qml
import QtQuick 2.0
import QtQuick.VirtualKeyboard.Styles 2.1
import QtGraphicalEffects 1.0

KeyboardStyle {
    keyboardDesignWidth: 2560
    keyboardDesignHeight: 800

    keyboardBackground: Rectangle {
        color: "transparent"

        FastBlur {
            anchors.fill: parent
            source: ShaderEffectSource {
                sourceItem: applicationWindow  // 背景をぼかす
                sourceRect: Qt.rect(0, 0, width, height)
            }
            radius: 64
        }

        Rectangle {
            anchors.fill: parent
            color: "#40ffffff"  // 半透明白
        }
    }

    keyPanel: KeyPanel {
        Rectangle {
            anchors.fill: parent
            anchors.margins: 8 * scaleHint
            radius: 12
            color: "#60ffffff"
            border.color: "#80ffffff"
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: control.displayText
                font.pixelSize: 48 * scaleHint
                color: "#333333"
            }

            layer.enabled: true
            layer.effect: DropShadow {
                color: "#40000000"
                radius: 8
                samples: 16
            }
        }
    }
}
```

### 4.3 ダークモード対応

```qml
import QtQuick 2.0
import QtQuick.VirtualKeyboard.Styles 2.1

KeyboardStyle {
    id: currentStyle

    // ダークモードフラグ
    property bool darkMode: true

    // カラーパレット
    readonly property color bgColor: darkMode ? "#1e1e1e" : "#ffffff"
    readonly property color keyColor: darkMode ? "#2d2d30" : "#e0e0e0"
    readonly property color textColor: darkMode ? "#ffffff" : "#000000"
    readonly property color accentColor: "#0078d7"

    keyboardDesignWidth: 2560
    keyboardDesignHeight: 800

    keyboardBackground: Rectangle {
        color: bgColor
    }

    keyPanel: KeyPanel {
        Rectangle {
            anchors.fill: parent
            anchors.margins: 5 * scaleHint
            radius: 4
            color: control.pressed ? accentColor : keyColor

            Text {
                anchors.centerIn: parent
                text: control.displayText
                font.pixelSize: 48 * scaleHint
                color: control.pressed ? "#ffffff" : textColor
            }
        }
    }
}
```

---

## 5. ベストプラクティス

### 5.1 パフォーマンス最適化

#### 1. scaleHintを活用

```qml
// ❌ Bad: 固定サイズ
font.pixelSize: 48

// ✅ Good: スケーラブル
font.pixelSize: 48 * scaleHint
```

#### 2. リソースの再利用

```qml
KeyboardStyle {
    // 共通プロパティを定義
    readonly property int baseFontSize: 48
    readonly property color baseColor: "#333"

    keyPanel: KeyPanel {
        Text {
            font.pixelSize: baseFontSize * scaleHint
            color: baseColor
        }
    }
}
```

#### 3. 重い処理を避ける

```qml
// ❌ Bad: Delegate内でShaderEffect
keyPanel: KeyPanel {
    ShaderEffect {
        // 多数のキーで実行されるとパフォーマンス低下
    }
}

// ✅ Good: 必要な場所のみ
keyboardBackground: Item {
    ShaderEffect {
        // 背景1つだけ
    }
}
```

### 5.2 アクセシビリティ

#### 1. 十分なコントラスト比

```qml
// WCAG AAレベル: 4.5:1以上
keyPanel: KeyPanel {
    Rectangle {
        color: "#2c3e50"  // 背景: ダーク
        Text {
            color: "#ffffff"  // テキスト: 白 (良好なコントラスト)
        }
    }
}
```

#### 2. 視認性の高いサイズ

```qml
// 最小タッチターゲット: 44x44 dp
keyPanel: KeyPanel {
    Rectangle {
        // 最小高さを確保
        implicitHeight: Math.max(88 * scaleHint, 44)
    }
}
```

### 5.3 レスポンシブデザイン

#### デバイスサイズ対応

```qml
KeyboardStyle {
    // 画面サイズに応じて調整
    keyboardDesignWidth: Screen.width > 1024 ? 2560 : 1280
    keyboardDesignHeight: Screen.width > 1024 ? 800 : 400

    keyboardRelativeBottomMargin: {
        if (Screen.width > 1920) return 0.15    // 大画面
        if (Screen.width > 1024) return 0.10    // 中画面
        return 0.05                              // 小画面
    }
}
```

### 5.4 テーマの切り替え

```qml
// ThemeManager.qml
pragma Singleton
import QtQuick 2.0

QtObject {
    property string currentTheme: "dark"

    function loadTheme(themeName) {
        currentTheme = themeName
        var stylePath = "qrc:/styles/" + themeName + "style.qml"
        VirtualKeyboardSettings.style = stylePath
    }
}
```

使用例:
```qml
Button {
    text: "Toggle Theme"
    onClicked: {
        ThemeManager.loadTheme(
            ThemeManager.currentTheme === "dark" ? "light" : "dark"
        )
    }
}
```

---

## まとめ

Qt Virtual Keyboard 5.12.10のスタイリングシステムは、**Component Delegate**パターンにより、完全なカスタマイズが可能です。

**重要なポイント**:
1. **scaleHint**で自動スケーリング
2. **KeyPanel.control**でキー情報にアクセス
3. **Component**で柔軟なUI定義
4. **パフォーマンス**を考慮した実装

**次のステップ**:
- デフォルトスタイル (`/src/virtualkeyboard/content/styles/default/style.qml`) を参考に独自スタイルを作成
- レイアウトシステムガイドでキー配置のカスタマイズを学習
