# カスタムキーボードオーバーレイ実装設計書（Qt 5.12.10版）

## 1. 概要

### 1.1 目的
Qt Virtual Keyboard 5.12.10に、画面全体を覆うカスタムキーボードオーバーレイ機能を実装する。このキーボードは上半分に入力フィールドを内包し、入力確定後にエンターキーで元の入力フィールドに内容を反映する。

### 1.2 要件
1. **全画面オーバーレイ**: キーボード起動時に画面全体を覆う
2. **内包入力フィールド**: キーボードの上半分に入力フィールドを配置
3. **入力反映**: エンターキー押下で元の入力フィールドに内容を反映

### 1.3 対象バージョン
- **Qt Version**: 5.12.10
- **QML Version**: QtQuick 2.x系
- **Build System**: qmake

### 1.4 対象ユーザー
- タッチスクリーンデバイスを使用するユーザー
- 視認性の高い入力インターフェースを必要とするユーザー
- フルスクリーンでの入力体験を求めるユーザー

---

## 2. システムアーキテクチャ

### 2.1 現状分析（Qt 5.12.10）

#### 既存のコンポーネント階層
```
InputPanel.qml (最上位コンテナ)
└── Keyboard.qml (キーボード本体)
    ├── wordCandidateView (単語候補リスト)
    ├── ShadowInputControl.qml (既存のフルスクリーンモード用)
    ├── keyboardInputArea (MultiPointTouchArea)
    └── KeyboardLayoutLoader.qml (キーレイアウトローダー)
        └── KeyboardLayout.qml
            └── KeyboardRow.qml × N (キー行)
```

#### 既存の関連機能
- **フルスクリーンモード**: `VirtualKeyboardSettings.fullScreenMode`
  - `ShadowInputControl.qml`で実装済み
  - キーボード上部に`TextInput`エリアを表示
  - `Flickable`でスクロール対応

- **入力コンテキスト**: `QVirtualKeyboardInputContext`
  - C++側でフォーカスオブジェクトと通信
  - `InputContext.commit(text)`メソッドで入力を確定

#### QMLインポート（Qt 5.12.10）
```qml
import QtQuick 2.0
import QtQuick.Layouts 1.0
import QtQuick.Window 2.2
import QtQuick.VirtualKeyboard 2.3
import QtQuick.VirtualKeyboard.Styles 2.1
import QtQuick.VirtualKeyboard.Settings 2.2
```

### 2.2 新規アーキテクチャ

#### コンポーネント構成
```
InputPanel.qml
└── CustomOverlayKeyboard.qml (新規作成) ★
    ├── Rectangle (背景オーバーレイ)
    ├── Column
    │   ├── OverlayInputField.qml (新規作成) ★
    │   │   ├── Rectangle (ツールバー)
    │   │   ├── Flickable
    │   │   │   └── TextInput (入力フィールド)
    │   │   └── Text (プレースホルダー)
    │   └── Keyboard.qml (既存コンポーネント、配置調整)
```

#### 新規作成ファイル
1. **`CustomOverlayKeyboard.qml`**
   - パス: `/src/virtualkeyboard/content/CustomOverlayKeyboard.qml`
   - 役割: 全画面オーバーレイコンテナ

2. **`OverlayInputField.qml`**
   - パス: `/src/virtualkeyboard/content/components/OverlayInputField.qml`
   - 役割: キーボード上半分の入力フィールド

#### 既存ファイルの修正箇所
1. **`InputPanel.qml`**
   - オーバーレイモード切替ロジック追加

2. **`Keyboard.qml`**
   - オーバーレイモード時のレイアウト調整
   - エンターキー押下シグナル追加

3. **`qvirtualkeyboardsettings_p.h`** (C++)
   - `overlayMode`プロパティ追加

4. **`virtualkeyboard.pro`** (qmake)
   - 新規QMLファイルのリソース追加

---

## 3. 詳細設計

### 3.1 CustomOverlayKeyboard.qml

#### 完全実装コード
```qml
/****************************************************************************
**
** Copyright (C) 2024 Custom Implementation
** Qt 5.12.10 Compatible
**
****************************************************************************/

import QtQuick 2.0
import QtQuick.VirtualKeyboard 2.3
import QtQuick.VirtualKeyboard.Settings 2.2

Item {
    id: customOverlayKeyboard
    objectName: "customOverlayKeyboard"

    // 表示制御
    property bool active: false

    // 入力内容の一時保存
    property string overlayText: ""

    // 元の入力フィールドへの参照
    property var originalInputItem: InputContext.inputItem

    // レイアウト比率 (上半分:下半分 = 1:1)
    property real inputFieldHeightRatio: 0.5

    // キーボード参照
    property alias keyboard: keyboardInstance

    anchors.fill: parent
    visible: active
    z: 1000  // 最前面に表示

    // 半透明背景
    Rectangle {
        id: backgroundOverlay
        anchors.fill: parent
        color: "#E0000000"  // 88%黒
        opacity: 1.0

        MouseArea {
            // 背景クリックを防ぐ
            anchors.fill: parent
            onClicked: {
                // 何もしない（背景タップを無効化）
            }
        }
    }

    // メインコンテナ
    Column {
        anchors.fill: parent
        spacing: 0

        // 上半分: 入力フィールドエリア
        OverlayInputField {
            id: overlayInputField
            width: parent.width
            height: parent.height * inputFieldHeightRatio
            text: customOverlayKeyboard.overlayText
            keyboard: keyboardInstance

            onTextChanged: {
                customOverlayKeyboard.overlayText = text
            }

            onCloseRequested: {
                closeOverlay()
            }

            onCommitRequested: {
                commitOverlayText()
                closeOverlay()
            }
        }

        // 下半分: キーボードエリア
        Keyboard {
            id: keyboardInstance
            width: parent.width
            height: parent.height * (1 - inputFieldHeightRatio)

            // 既存のactiveプロパティを上書きしない
            // オーバーレイモード時は常にアクティブ
        }
    }

    // キーボードのキー入力を監視
    Connections {
        target: InputContext

        onPreeditTextChanged: {
            // 予測変換テキストを入力フィールドに反映
            if (active && InputContext.preeditText.length > 0) {
                overlayInputField.appendText(InputContext.preeditText)
            }
        }
    }

    // Enterキー押下を監視
    Connections {
        target: keyboardInstance

        // Keyboard.qmlでactiveKeyが変更されたとき
        onActiveKeyChanged: {
            if (active && keyboardInstance.activeKey) {
                var key = keyboardInstance.activeKey.key
                var text = keyboardInstance.activeKey.text

                if (key === Qt.Key_Return || key === Qt.Key_Enter) {
                    commitOverlayText()
                    closeOverlay()
                } else if (key === Qt.Key_Backspace) {
                    overlayInputField.backspace()
                } else if (text && text.length > 0) {
                    overlayInputField.appendText(text)
                }
            }
        }
    }

    function commitOverlayText() {
        if (originalInputItem && overlayText.length > 0) {
            // InputContextを通じて元の入力フィールドに反映
            InputContext.commit(overlayText)
        }
    }

    function closeOverlay() {
        active = false
        overlayText = ""
        overlayInputField.clear()
    }

    function openOverlay() {
        active = true
        overlayText = ""
        overlayInputField.focus = true
    }

    // アニメーション
    Behavior on opacity {
        NumberAnimation {
            duration: 200
            easing.type: Easing.OutCubic
        }
    }

    opacity: active ? 1.0 : 0.0
}
```

### 3.2 OverlayInputField.qml

#### 完全実装コード
```qml
/****************************************************************************
**
** Copyright (C) 2024 Custom Implementation
** Qt 5.12.10 Compatible
**
****************************************************************************/

import QtQuick 2.0
import QtQuick.VirtualKeyboard 2.3

Rectangle {
    id: overlayInputField
    color: "#FFFFFF"

    property alias text: textInput.text
    property string placeholderText: "テキストを入力してください..."
    property alias focus: textInput.focus
    property var keyboard: null

    signal closeRequested()
    signal commitRequested()

    // ツールバー
    Rectangle {
        id: toolbar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 60
        color: "#F0F0F0"

        Row {
            anchors.centerIn: parent
            spacing: 20

            // 閉じるボタン
            Rectangle {
                width: 100
                height: 40
                color: "#E0E0E0"
                radius: 4

                Text {
                    anchors.centerIn: parent
                    text: "× 閉じる"
                    font.pixelSize: 20
                    color: "#000000"
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: closeRequested()
                }
            }

            // 確定ボタン
            Rectangle {
                width: 100
                height: 40
                color: "#0078D7"
                radius: 4

                Text {
                    anchors.centerIn: parent
                    text: "✓ 確定"
                    font.pixelSize: 20
                    color: "#FFFFFF"
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: commitRequested()
                }
            }
        }
    }

    // スクロール可能な入力エリア
    Flickable {
        id: flickable
        anchors.top: toolbar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 20

        contentWidth: textInput.paintedWidth
        contentHeight: textInput.paintedHeight
        clip: true
        flickableDirection: Flickable.HorizontalFlick
        interactive: contentWidth > width

        function ensureVisible(rectangle) {
            if (contentX >= rectangle.x)
                contentX = rectangle.x
            else if (contentX + width <= rectangle.x + rectangle.width)
                contentX = rectangle.x + rectangle.width - width
        }

        TextInput {
            id: textInput
            width: Math.max(flickable.width, implicitWidth)
            height: Math.max(flickable.height, implicitHeight)
            font.pixelSize: 32
            color: "#000000"
            selectionColor: "#0078D7"
            selectedTextColor: "#FFFFFF"
            wrapMode: TextInput.Wrap
            selectByMouse: true

            // カーソル点滅
            property bool blinkStatus: true

            cursorDelegate: Rectangle {
                width: 2
                height: textInput.font.pixelSize
                color: "#0078D7"
                visible: textInput.blinkStatus

                Timer {
                    interval: 500
                    repeat: true
                    running: textInput.activeFocus
                    onTriggered: textInput.blinkStatus = !textInput.blinkStatus
                }
            }

            onCursorRectangleChanged: flickable.ensureVisible(cursorRectangle)

            onCursorPositionChanged: {
                blinkStatus = true
            }
        }

        // プレースホルダー
        Text {
            id: placeholder
            anchors.fill: textInput
            text: overlayInputField.placeholderText
            font: textInput.font
            color: "#808080"
            visible: textInput.text.length === 0
        }
    }

    function appendText(newText) {
        textInput.text += newText
    }

    function backspace() {
        if (textInput.text.length > 0) {
            textInput.text = textInput.text.slice(0, -1)
        }
    }

    function clear() {
        textInput.text = ""
    }
}
```

### 3.3 InputPanel.qml の修正

#### 修正内容
```qml
// InputPanel.qml の既存コードに追加

import QtQuick 2.0
import QtQuick.VirtualKeyboard 2.1

Item {
    id: inputPanel

    property alias active: keyboard.active
    // ... 既存のプロパティ ...

    // 既存のKeyboard（通常モード用）
    Loader {
        id: keyboard
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        // オーバーレイモード時は非表示
        visible: !customOverlayKeyboard.active

        source: Qt.resolvedUrl("content/components/Keyboard.qml")

        // ... 既存の設定 ...
    }

    // 新規: カスタムオーバーレイキーボード
    CustomOverlayKeyboard {
        id: customOverlayKeyboard
        anchors.fill: parent

        // 設定に応じてオーバーレイモードを有効化
        active: VirtualKeyboardSettings.overlayMode && keyboard.active
    }

    // ... 既存の関数とロジック ...
}
```

### 3.4 Keyboard.qml の修正

#### 修正内容
Keyboard.qmlは基本的に既存のままで動作しますが、オーバーレイモード時の高さ調整を追加します。

```qml
// Keyboard.qml に追加するプロパティ

Item {
    id: keyboard

    // 新規プロパティ
    property bool overlayMode: false

    // 既存のwidth/height計算
    width: keyboardBackground.width
    height: overlayMode ? parent.height : (keyboardBackground.height + ...)

    // ... 既存のコード ...
}
```

### 3.5 設定管理 (C++)

#### qvirtualkeyboardsettings_p.h への追加
```cpp
// src/virtualkeyboard/qvirtualkeyboardsettings_p.h

class QVirtualKeyboardSettings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool overlayMode READ overlayMode WRITE setOverlayMode NOTIFY overlayModeChanged)
    // ... 既存のプロパティ ...

public:
    explicit QVirtualKeyboardSettings(QObject *parent = nullptr);

    bool overlayMode() const;
    void setOverlayMode(bool enable);

signals:
    void overlayModeChanged();

private:
    bool m_overlayMode;
    // ... 既存のメンバー変数 ...
};
```

#### qvirtualkeyboardsettings.cpp への実装
```cpp
// src/virtualkeyboard/qvirtualkeyboardsettings.cpp

QVirtualKeyboardSettings::QVirtualKeyboardSettings(QObject *parent)
    : QObject(parent)
    , m_overlayMode(false)
    // ... 既存の初期化 ...
{
}

bool QVirtualKeyboardSettings::overlayMode() const
{
    return m_overlayMode;
}

void QVirtualKeyboardSettings::setOverlayMode(bool enable)
{
    if (m_overlayMode != enable) {
        m_overlayMode = enable;
        emit overlayModeChanged();
    }
}
```

### 3.6 qmake ビルド設定

#### virtualkeyboard.pro への追加
```qmake
# src/virtualkeyboard/virtualkeyboard.pro

RESOURCES += \
    content/styles/default/default_style.qrc \
    content/styles/retro/retro_style.qrc \
    content/content.qrc

# content.qrc に以下を追加:
# <file>CustomOverlayKeyboard.qml</file>
# <file>components/OverlayInputField.qml</file>
```

#### content.qrc の更新
```xml
<!DOCTYPE RCC>
<RCC version="1.0">
<qresource prefix="/QtQuick/VirtualKeyboard/content">
    <!-- 既存のファイル -->
    <file>InputPanel.qml</file>
    <!-- ... -->

    <!-- 新規追加 -->
    <file>CustomOverlayKeyboard.qml</file>
    <file>components/OverlayInputField.qml</file>
</qresource>
</RCC>
```

---

## 4. データフロー

### 4.1 入力フロー

```
[ユーザー操作]
    ↓
[入力フィールドフォーカス]
    ↓
[InputContext.show()]
    ↓
[VirtualKeyboardSettings.overlayMode == true]
    ↓
[CustomOverlayKeyboard.active = true]
    ↓
[画面全体オーバーレイ表示]
    ↓
[OverlayInputField + Keyboard 表示]
    ↓
[キーボード入力]
    ↓
[OverlayInputField.text 更新]
    ↓
[Enter キー押下]
    ↓
[CustomOverlayKeyboard.commitOverlayText()]
    ↓
[InputContext.commit(overlayText)]
    ↓
[元の入力フィールドに反映]
    ↓
[CustomOverlayKeyboard.closeOverlay()]
    ↓
[オーバーレイ非表示]
```

### 4.2 キー入力処理フロー

```
[キー押下]
    ↓
[Keyboard.activeKey 変更]
    ↓
[Connections で検知]
    ↓
[キー種別判定]
    ├─ Enter → commitOverlayText() + closeOverlay()
    ├─ Backspace → overlayInputField.backspace()
    └─ 文字キー → overlayInputField.appendText(text)
```

---

## 5. UI/UXデザイン

### 5.1 レイアウト仕様

#### 画面分割比率
```
┌─────────────────────────────────┐
│  ツールバー (60px固定)          │
├─────────────────────────────────┤
│                                 │
│  入力フィールドエリア           │
│  (画面高さの50% - ツールバー)   │
│                                 │
├─────────────────────────────────┤
│                                 │
│  キーボードエリア               │
│  (画面高さの50%)                │
│                                 │
└─────────────────────────────────┘
```

#### 視覚的階層
1. **背景オーバーレイ**: 黒 88%透過 (`#E0000000`)
2. **入力フィールド**: 白背景 (`#FFFFFF`)
3. **キーボード**: スタイル準拠の背景色
4. **ツールバー**: ライトグレー (`#F0F0F0`)

### 5.2 タイポグラフィ

| 要素 | フォントサイズ | カラー |
|------|--------------|--------|
| 入力テキスト | 32px | #000000 |
| プレースホルダー | 32px | #808080 |
| ツールバーボタン | 20px | #000000 / #FFFFFF |

### 5.3 カラーパレット

| 要素 | カラーコード | 用途 |
|------|------------|------|
| オーバーレイ背景 | #E0000000 | 半透明黒背景 |
| 入力エリア背景 | #FFFFFF | 白 |
| ツールバー背景 | #F0F0F0 | ライトグレー |
| 確定ボタン | #0078D7 | Windows Blue |
| キャンセルボタン | #E0E0E0 | グレー |
| カーソル/選択 | #0078D7 | Windows Blue |

---

## 6. 技術仕様

### 6.1 開発環境
- **Qt Version**: 5.12.10
- **QML Version**: QtQuick 2.x
- **C++ Standard**: C++11
- **Build System**: qmake

### 6.2 依存関係

#### QMLモジュール
```qml
import QtQuick 2.0
import QtQuick.Layouts 1.0
import QtQuick.Window 2.2
import QtQuick.VirtualKeyboard 2.3
import QtQuick.VirtualKeyboard.Styles 2.1
import QtQuick.VirtualKeyboard.Settings 2.2
```

#### C++ヘッダー
```cpp
#include <QtVirtualKeyboard/qvirtualkeyboardinputcontext_p.h>
#include <QtVirtualKeyboard/qvirtualkeyboardsettings_p.h>
#include <QtCore/QObject>
#include <QtGui/QGuiApplication>
```

### 6.3 パフォーマンス要件

| 項目 | 目標値 |
|------|--------|
| オーバーレイ表示時間 | < 200ms |
| キー入力レスポンス | < 50ms |
| 入力確定処理時間 | < 100ms |
| メモリ使用量増加 | < 3MB |

### 6.4 互換性

#### 対応プラットフォーム
- Linux (X11, Wayland)
- Windows
- macOS
- Embedded Linux (eLinux)
- Android
- iOS

#### 既存機能との共存
- 通常キーボードモード: 影響なし（`overlayMode=false`時）
- フルスクリーンモード: `overlayMode`が優先
- ハンドライティングモード: 排他制御

---

## 7. 実装計画

### 7.1 フェーズ1: 基本構造 (2-3日)

#### タスク
1. `CustomOverlayKeyboard.qml` 作成
   - 全画面オーバーレイコンテナ
   - 基本プロパティ定義

2. `OverlayInputField.qml` 作成
   - TextInput コンポーネント
   - プレースホルダー実装

3. `InputPanel.qml` 修正
   - Loader追加
   - オーバーレイモード切替ロジック

4. `qvirtualkeyboardsettings_p.h/cpp` 修正
   - `overlayMode` プロパティ追加

5. qmake設定更新
   - `content.qrc` に新規ファイル追加
   - ビルド確認

#### 成果物
- 表示可能なオーバーレイキーボード
- 入力フィールドの基本UI

### 7.2 フェーズ2: 入力処理 (2-3日)

#### タスク
1. キーボード入力の連携
   - `Connections` でキー入力監視
   - テキスト追加/削除処理

2. テキスト同期処理
   - InputContext との連携
   - 予測変換対応

3. 確定処理実装
   - `commitOverlayText()` 関数
   - 元の入力フィールドへの反映

4. キーハンドリング
   - Enter, Backspace, 文字キー

#### 成果物
- 動作する入力・確定フロー

### 7.3 フェーズ3: UI改善 (1-2日)

#### タスク
1. ツールバー実装
   - 閉じるボタン
   - 確定ボタン

2. アニメーション追加
   - フェードイン/アウト

3. スタイル適用
   - カラー調整
   - フォント調整

4. スクロール対応
   - 長文入力時のFlickable動作確認

#### 成果物
- 洗練されたUI/UX

### 7.4 フェーズ4: テスト・最適化 (2-3日)

#### タスク
1. 統合テスト実施
2. プラットフォーム別動作確認
3. パフォーマンス計測
4. バグ修正

#### 成果物
- テスト済みの安定版

---

## 8. テスト計画

### 8.1 機能テスト

#### テストケース

| ID | テスト項目 | 入力 | 期待結果 |
|----|-----------|------|---------|
| FT-01 | オーバーレイ表示 | `overlayMode=true`, フォーカスイン | オーバーレイ表示 |
| FT-02 | テキスト入力 | キーボードで"Hello" | `overlayText="Hello"` |
| FT-03 | Enter確定 | テキスト入力後Enter | 元フィールドに反映 |
| FT-04 | 閉じるボタン | 閉じるボタンクリック | オーバーレイ非表示 |
| FT-05 | 空文字確定 | 空文字でEnter | 元フィールド変更なし |
| FT-06 | Backspace | "ABC"入力後Backspace | "AB"になる |
| FT-07 | 長文入力 | 100文字入力 | スクロール動作 |
| FT-08 | 日本語入力 | ひらがな入力 | 正しく表示 |

### 8.2 統合テスト

#### シナリオテスト

**ST-01: 基本フロー**
```
1. LineEditをクリック
2. オーバーレイキーボード表示確認
3. "こんにちは"と入力
4. Enterキー押下
5. 元のLineEditに"こんにちは"が反映されることを確認
```

**ST-02: キャンセルフロー**
```
1. TextFieldをクリック
2. "Test"と入力
3. 閉じるボタンクリック
4. 元のTextFieldが変更されていないことを確認
```

**ST-03: 複数回使用**
```
1. 入力フィールドA→入力→確定
2. 入力フィールドB→入力→確定
3. 両方に正しく反映されることを確認
```

### 8.3 プラットフォームテスト

| プラットフォーム | テスト内容 |
|---------------|----------|
| Linux (Ubuntu 18.04) | 基本動作、日本語入力 |
| Windows 10 | 基本動作、タッチ操作 |
| Embedded Linux | パフォーマンス、メモリ使用量 |

---

## 9. リスクと対策

### 9.1 技術リスク

| リスク | 影響度 | 対策 |
|--------|-------|------|
| Qt 5.12のAPI制約 | 中 | 公式ドキュメント参照、代替実装 |
| 既存機能との競合 | 高 | フラグによる排他制御、十分な統合テスト |
| パフォーマンス劣化 | 中 | 遅延読み込み、リソース最適化 |
| メモリリーク | 中 | QML オブジェクトのライフサイクル管理 |

### 9.2 UXリスク

| リスク | 影響度 | 対策 |
|--------|-------|------|
| 誤操作によるデータ損失 | 高 | 確認ダイアログ追加検討 |
| 視認性の問題 | 中 | コントラスト調整、フォントサイズ調整可能に |
| 日本語入力の不具合 | 高 | 十分なテスト、IME連携確認 |

---

## 10. 保守・拡張性

### 10.1 設定のカスタマイズ

#### 拡張可能な設定項目
```cpp
// 将来的に追加可能
Q_PROPERTY(qreal overlayInputFieldHeightRatio ...)
Q_PROPERTY(QString overlayBackgroundColor ...)
Q_PROPERTY(int overlayFontSize ...)
Q_PROPERTY(bool overlayToolbarVisible ...)
```

### 10.2 拡張ポイント

#### 将来的な拡張案
1. **マルチテーマ対応**
   - ライトモード/ダークモード
   - カスタムカラースキーム

2. **高度な編集機能**
   - Undo/Redo
   - クリップボード連携

3. **アクセシビリティ**
   - スクリーンリーダー対応
   - ハイコントラストモード

---

## 11. ビルド手順

### 11.1 開発環境セットアップ

```bash
# Qt 5.12.10 インストール確認
qmake --version

# リポジトリクローン（既存）
cd /home/user/qtvirtualkeyboard

# ブランチ確認
git checkout claude/custom-keyboard-overlay-5.12-3O8au
```

### 11.2 ビルドコマンド

```bash
# qmake実行
qmake qtvirtualkeyboard.pro

# ビルド
make -j4

# インストール（オプション）
make install
```

### 11.3 テスト実行

```bash
# テストプログラム実行
cd tests/auto/inputpanel
qmake
make
./inputpanel
```

---

## 12. まとめ

### 12.1 実装の要点

1. **Qt 5.12.10対応**
   - QtQuick 2.x系のAPI使用
   - qmakeビルドシステム
   - 既存のShadowInputControlを参考

2. **モジュール性**
   - 新規コンポーネントは独立して実装
   - 既存コードへの影響を最小化

3. **段階的実装**
   - フェーズごとに動作確認
   - 早期のフィードバック収集

### 12.2 成功指標

| 指標 | 目標 |
|------|------|
| 機能完成度 | 100% (全要件実装) |
| ビルド成功率 | 100% |
| パフォーマンス | 要件達成 |
| バグ件数 | 重大バグ 0件 |

### 12.3 次のステップ

1. ステークホルダーレビュー
2. 実装開始（フェーズ1）
3. プロトタイプデモ
4. フィードバック収集
5. 段階的リリース

---

## 付録

### A. Qt 5.12.10 API リファレンス

- InputContext: https://doc.qt.io/qt-5.12/qml-qtquick-virtualkeyboard-inputcontext.html
- VirtualKeyboardSettings: https://doc.qt.io/qt-5.12/qml-qtquick-virtualkeyboard-settings-virtualkeyboardsettings.html
- TextInput: https://doc.qt.io/qt-5.12/qml-qtquick-textinput.html

### B. 用語集

| 用語 | 説明 |
|------|------|
| オーバーレイ | 既存UIの上に重ねて表示するUI要素 |
| InputContext | Qt VKBの入力管理コンテキスト |
| ShadowInputControl | 既存のフルスクリーンモード用入力コンポーネント |
| commit | 入力テキストを確定してアプリケーションに送信 |
| qmake | Qtのビルドシステム（Qt 5系で使用） |

### C. バージョン履歴

| バージョン | 日付 | 変更内容 | 作成者 |
|----------|------|---------|--------|
| 1.0 | 2026-01-10 | 初版作成（Qt 5.12.10版） | Claude |

---

**Document End**
