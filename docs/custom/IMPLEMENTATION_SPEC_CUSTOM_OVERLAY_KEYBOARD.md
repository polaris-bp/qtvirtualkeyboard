# カスタムオーバーレイキーボード改造仕様書

**バージョン**: 1.0
**対象**: Qt Virtual Keyboard 5.12.10
**最終更新**: 2026-01-10
**作成者**: Claude (based on source code analysis)

---

## 目次

1. [要件定義](#1-要件定義)
2. [設計方針](#2-設計方針)
3. [既存機能の活用](#3-既存機能の活用)
4. [アーキテクチャ設計](#4-アーキテクチャ設計)
5. [コンポーネント仕様](#5-コンポーネント仕様)
6. [実装手順](#6-実装手順)
7. [テスト計画](#7-テスト計画)
8. [リスクと対策](#8-リスクと対策)

---

## 1. 要件定義

### 1.1 機能要件

| ID | 要件 | 優先度 |
|----|------|--------|
| FR-01 | キーボード立ち上がり時、キーボードで画面全体を覆う | 必須 |
| FR-02 | キーボードの上半分はキーボードに内包された入力フィールドとなる | 必須 |
| FR-03 | 入力を確定後、エンターによって入力内容が覆う前の入力フィールドに反映される | 必須 |
| FR-04 | 既存のキーボード機能（予測変換、多言語対応等）を維持 | 必須 |
| FR-05 | ESCキーまたは閉じるボタンでオーバーレイを閉じる | 推奨 |

### 1.2 非機能要件

| ID | 要件 | 目標値 |
|----|------|--------|
| NFR-01 | キーボード表示速度 | 300ms以内 |
| NFR-02 | 既存コードへの影響 | 最小限（既存機能を破壊しない） |
| NFR-03 | 保守性 | 既存アーキテクチャに準拠 |

### 1.3 制約条件

- Qt Virtual Keyboard 5.12.10のAPIを使用
- QML QtQuick 2.x系を使用（6.x系ではない）
- 既存の入力メソッドプラグインとの互換性を維持

---

## 2. 設計方針

### 2.1 既存機能の最大活用

既存の**フルスクリーンモード**機能を拡張する方針を採用します。

**理由**:
1. `ShadowInputContext`が既に実装されている
2. `VirtualKeyboardSettings.fullScreenMode`で制御可能
3. テキスト同期ロジックが実装済み
4. 既存コードの変更を最小化できる

### 2.2 実装アプローチ

```
【既存フルスクリーンモード】
┌─────────────────────────┐
│  ShadowInputControl     │ ← 入力フィールド
├─────────────────────────┤
│                         │
│      Keyboard           │ ← キーボード
│                         │
└─────────────────────────┘

【カスタムオーバーレイモード】
┌─────────────────────────┐
│  CustomInputField       │ ← 上半分
│  (画面の50%)             │
├─────────────────────────┤
│      Keyboard           │ ← 下半分
│  (画面の50%)             │
└─────────────────────────┘
  ↑ 画面全体を覆う
```

### 2.3 変更レベル

| コンポーネント | 変更内容 | 変更レベル |
|--------------|---------|----------|
| **InputPanel.qml** | オーバーレイモード追加 | 中 |
| **Keyboard.qml** | レイアウト調整 | 小 |
| **新規コンポーネント** | CustomOverlayKeyboard.qml作成 | - |
| **EnterKey動作** | カスタマイズ | 小 |
| **設定追加** | overlayModeプロパティ追加 | 小 |

---

## 3. 既存機能の活用

### 3.1 ShadowInputContext（既存）

**ファイル**: `src/virtualkeyboard/shadowinputcontext_p.h`

```cpp
class ShadowInputContext : public QObject {
    Q_OBJECT
public:
    QString preeditText() const;
    QString surroundingText() const;
    int cursorPosition() const;

    // InputContextからの同期
    void update(Qt::InputMethodQueries queries);
};
```

**活用方法**:
- `InputContext.priv.shadow`経由でアクセス
- 既存のテキスト同期メカニズムを利用

### 3.2 ShadowInputControl.qml（既存）

**ファイル**: `src/virtualkeyboard/content/components/ShadowInputControl.qml` (138行)

**主要機能**:
```qml
Item {
    property alias text: textInput.text
    property alias cursorPosition: textInput.cursorPosition

    Flickable {
        TextInput {
            id: textInput
            text: InputContext.preeditText
            // カーソル同期
            // スクロール処理
        }
    }
}
```

**活用方法**:
- そのまま再利用可能
- スタイル調整のみ

### 3.3 VirtualKeyboardSettings（既存）

**ファイル**: `src/virtualkeyboard/settings_p.h`

**活用するプロパティ**:
```cpp
bool fullScreenMode() const;
void setFullScreenMode(bool fullScreenMode);
```

**拡張方法**:
新しいプロパティ `overlayMode` を追加する選択肢もあるが、
まずは既存の `fullScreenMode` を拡張する形で実装。

---

## 4. アーキテクチャ設計

### 4.1 コンポーネント構成

```
Application Window
└── InputPanel (既存・改造)
    ├── SelectionControl (既存)
    └── Keyboard (既存・改造)
        └── [通常モード時のコンテンツ]

OR

Application Window
└── CustomOverlayKeyboard (新規)
    ├── CustomInputField (新規)
    │   └── ShadowInputControl (既存活用)
    └── Keyboard (既存活用)
```

### 4.2 実装パターン（2案）

#### パターンA: InputPanel拡張型（推奨）

**メリット**:
- 既存の状態管理を活用
- InputContextとの連携が容易
- 変更範囲が明確

**デメリット**:
- InputPanel.qmlの変更が必要

#### パターンB: 完全新規コンポーネント型

**メリット**:
- 既存コードに影響なし
- 独立して開発可能

**デメリット**:
- InputContextとの連携を再実装
- 状態管理の重複

**選択**: パターンAを採用

### 4.3 モード切替設計

```qml
InputPanel {
    property bool overlayMode: false  // 新規プロパティ

    // overlayMode == true の場合
    // → 画面全体を覆うレイアウトに切り替え

    // overlayMode == false の場合
    // → 通常の画面下部配置
}
```

### 4.4 データフロー

```
[ユーザーがテキスト入力フィールドをタップ]
    ↓
[InputContext.show() 呼び出し]
    ↓
[InputPanel.active = true]
    ↓
[overlayMode = true に設定]
    ↓
[画面全体を覆うレイアウトに変更]
    ↓
[上半分: CustomInputField表示]
[下半分: Keyboard表示]
    ↓
[ユーザーが入力]
    ↓
[CustomInputField.text に反映]
    ↓
[ユーザーがEnterキー押下]
    ↓
[InputContext.commit(CustomInputField.text)]
    ↓
[元のテキストフィールドにテキスト確定]
    ↓
[overlayMode = false]
[InputPanel非表示]
```

---

## 5. コンポーネント仕様

### 5.1 CustomOverlayKeyboard.qml（新規）

**責務**: オーバーレイモード時のレイアウト管理

**ファイルパス**: `src/virtualkeyboard/content/components/CustomOverlayKeyboard.qml`

```qml
import QtQuick 2.0
import QtQuick.VirtualKeyboard 2.3
import QtQuick.VirtualKeyboard.Styles 2.1
import QtQuick.Layouts 1.0

Item {
    id: overlayKeyboard

    // プロパティ
    property bool active: false
    readonly property alias keyboard: keyboard
    readonly property alias inputField: inputField

    // 背景（画面全体を覆う）
    Rectangle {
        anchors.fill: parent
        color: "#F0F0F0"  // ライトグレー背景
        opacity: 0.95

        MouseArea {
            // タップイベントを吸収（下のウィジェットに伝播させない）
            anchors.fill: parent
            preventStealing: true
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // 上半分: 入力フィールド
        Item {
            id: inputFieldContainer
            Layout.fillWidth: true
            Layout.preferredHeight: parent.height * 0.5  // 画面の50%

            CustomInputField {
                id: inputField
                anchors.fill: parent
                anchors.margins: 20
            }
        }

        // 下半分: キーボード
        Item {
            id: keyboardContainer
            Layout.fillWidth: true
            Layout.preferredHeight: parent.height * 0.5  // 画面の50%

            Keyboard {
                id: keyboard
                anchors.fill: parent

                // オーバーレイモード専用設定
                property bool overlayMode: true
            }
        }
    }

    // Enterキーハンドリング
    Connections {
        target: keyboard
        onEnterKeyPressed: {
            // 入力内容を元のInputContextに反映
            if (inputField.text.length > 0) {
                InputContext.commit(inputField.text)
            }
            // オーバーレイを閉じる
            overlayKeyboard.active = false
        }
    }

    // ESCキーでオーバーレイを閉じる
    Keys.onEscapePressed: {
        overlayKeyboard.active = false
    }

    // アニメーション
    Behavior on opacity {
        NumberAnimation { duration: 200 }
    }

    states: [
        State {
            name: "active"
            when: overlayKeyboard.active
            PropertyChanges { target: overlayKeyboard; visible: true; opacity: 1.0 }
        },
        State {
            name: "inactive"
            when: !overlayKeyboard.active
            PropertyChanges { target: overlayKeyboard; visible: false; opacity: 0.0 }
        }
    ]
}
```

### 5.2 CustomInputField.qml（新規）

**責務**: オーバーレイモード専用の入力フィールド

**ファイルパス**: `src/virtualkeyboard/content/components/CustomInputField.qml`

```qml
import QtQuick 2.0
import QtQuick.VirtualKeyboard 2.3

Rectangle {
    id: inputFieldRoot
    color: "white"
    border.color: "#CCCCCC"
    border.width: 2
    radius: 8

    // 公開プロパティ
    property alias text: textInput.text
    property alias cursorPosition: textInput.cursorPosition

    // 内部プロパティ
    readonly property real fontSize: Math.min(height * 0.15, 48)

    // タイトルバー
    Rectangle {
        id: titleBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 50
        color: "#4A90E2"
        radius: 8

        Text {
            anchors.centerIn: parent
            text: "入力"
            font.pixelSize: 24
            font.bold: true
            color: "white"
        }

        // 閉じるボタン
        Rectangle {
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            width: 40
            height: 40
            radius: 20
            color: "#F44336"

            Text {
                anchors.centerIn: parent
                text: "×"
                font.pixelSize: 28
                font.bold: true
                color: "white"
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    // 親のCustomOverlayKeyboardを閉じる
                    inputFieldRoot.parent.parent.parent.active = false
                }
            }
        }
    }

    // スクロール可能なテキスト入力エリア
    Flickable {
        id: flickable
        anchors.top: titleBar.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 15

        contentWidth: textInput.width
        contentHeight: textInput.height
        clip: true

        // テキスト入力
        TextInput {
            id: textInput
            width: flickable.width
            height: Math.max(flickable.height, contentHeight)

            font.pixelSize: inputFieldRoot.fontSize
            wrapMode: TextInput.Wrap
            color: "#333333"
            selectionColor: "#4A90E2"
            selectedTextColor: "white"

            // InputContextと同期
            Binding {
                target: textInput
                property: "text"
                value: InputContext.preeditText + InputContext.surroundingText.substring(0, InputContext.cursorPosition)
                when: InputContext.preeditText.length > 0
            }

            // カーソル表示
            cursorVisible: true
            cursorDelegate: Rectangle {
                width: 2
                color: "#4A90E2"

                SequentialAnimation on opacity {
                    loops: Animation.Infinite
                    NumberAnimation { to: 0; duration: 500 }
                    NumberAnimation { to: 1; duration: 500 }
                }
            }

            // カーソル位置に自動スクロール
            onCursorRectangleChanged: {
                flickable.ensureVisible(cursorRectangle)
            }
        }
    }

    // Flickableのヘルパー関数
    function ensureVisible(rect) {
        if (flickable.contentY > rect.y) {
            flickable.contentY = rect.y
        } else if (flickable.contentY + flickable.height < rect.y + rect.height) {
            flickable.contentY = rect.y + rect.height - flickable.height
        }
    }
}
```

### 5.3 InputPanel.qml（既存・改造）

**ファイルパス**: `src/virtualkeyboard/content/InputPanel.qml` (147行)

**変更内容**:

```qml
// 既存コード（変更なし）
Item {
    id: inputPanel

    // ========== 追加 START ==========
    property bool overlayMode: false  // オーバーレイモード有効化フラグ
    // ========== 追加 END ==========

    // 既存の active プロパティ
    readonly property bool active: Qt.inputMethod.visible

    // ========== 追加 START ==========
    // オーバーレイモード時のサイズ変更
    implicitHeight: overlayMode ? parent.height : keyboard.height
    implicitWidth: overlayMode ? parent.width : parent.width
    anchors.fill: overlayMode ? parent : undefined
    anchors.left: !overlayMode ? parent.left : undefined
    anchors.right: !overlayMode ? parent.right : undefined
    anchors.bottom: !overlayMode ? parent.bottom : undefined
    // ========== 追加 END ==========

    // ========== 追加 START ==========
    // オーバーレイキーボード
    CustomOverlayKeyboard {
        id: customOverlay
        anchors.fill: parent
        visible: overlayMode
        active: overlayMode && inputPanel.active
        z: 100  // 最前面に表示
    }
    // ========== 追加 END ==========

    // 既存のコンポーネント（通常モード時に表示）
    SelectionControl {
        objectName: "selectionControl"
        x: -parent.x
        y: -parent.y
        enabled: active && !keyboard.fullScreenMode && !overlayMode  // 変更
    }

    Keyboard {
        id: keyboard
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: !overlayMode  // 追加
    }

    // 既存のBinding（変更なし）
    Binding {
        target: InputContext.priv
        property: "keyboardRectangle"
        value: mapToItem(null, keyboard.x, keyboard.y, keyboard.width, keyboard.height)
    }
}
```

### 5.4 Keyboard.qml（既存・微調整）

**ファイルパス**: `src/virtualkeyboard/content/components/Keyboard.qml` (1621行)

**変更内容**:

```qml
Item {
    id: keyboard

    // ========== 追加 START ==========
    property bool overlayMode: false  // 親から設定される
    // ========== 追加 END ==========

    // 既存コード...

    // EnterKeyのカスタマイズ
    // ========== 追加 START ==========
    signal enterKeyPressed()  // 新規シグナル
    // ========== 追加 END ==========
}
```

### 5.5 EnterKey.qml（既存・改造）

**ファイルパス**: `src/virtualkeyboard/content/components/EnterKey.qml` (68行)

**変更内容**:

```qml
BaseKey {
    id: enterKey
    key: Qt.Key_Return
    text: "\n"

    // 既存コード...

    // ========== 追加 START ==========
    onClicked: {
        // オーバーレイモードの場合、カスタム動作
        if (keyboard.overlayMode) {
            keyboard.enterKeyPressed()
            // 通常のキーイベントは送信しない
            return
        }
        // 通常モードは既存動作
    }
    // ========== 追加 END ==========
}
```

---

## 6. 実装手順

### フェーズ1: 基本実装（2-3日）

#### Step 1: 新規コンポーネント作成

```bash
# ファイル作成
touch src/virtualkeyboard/content/components/CustomOverlayKeyboard.qml
touch src/virtualkeyboard/content/components/CustomInputField.qml
```

#### Step 2: QRCリソース登録

**ファイル**: `src/virtualkeyboard/content/virtualkeyboard_content.qrc`

```xml
<RCC version="1.0">
<qresource prefix="/QtQuick/VirtualKeyboard/content">
    <!-- 既存ファイル... -->

    <!-- 追加 -->
    <file>components/CustomOverlayKeyboard.qml</file>
    <file>components/CustomInputField.qml</file>
</qresource>
</RCC>
```

#### Step 3: .proファイル更新

**ファイル**: `src/virtualkeyboard/virtualkeyboard.pro`

```qmake
# RESOURCES セクションに追加
# （QRCファイルで管理されているため、通常は不要）
```

#### Step 4: 各コンポーネント実装

1. `CustomInputField.qml` 実装
2. `CustomOverlayKeyboard.qml` 実装
3. `InputPanel.qml` 改造
4. `Keyboard.qml` シグナル追加
5. `EnterKey.qml` 改造

### フェーズ2: テスト・調整（1-2日）

#### Step 5: サンプルアプリケーション作成

```qml
// test_overlay.qml
import QtQuick 2.0
import QtQuick.Window 2.0
import QtQuick.VirtualKeyboard 2.3

Window {
    id: window
    visible: true
    width: 800
    height: 600

    TextField {
        id: textField
        anchors.centerIn: parent
        width: 400
        placeholderText: "タップして入力"

        onFocusChanged: {
            if (focus) {
                // オーバーレイモード有効化
                inputPanel.overlayMode = true
            }
        }
    }

    InputPanel {
        id: inputPanel
        anchors.fill: parent
    }
}
```

#### Step 6: 動作確認

- [ ] 画面全体を覆うか確認
- [ ] 上半分に入力フィールドが表示されるか確認
- [ ] 下半分にキーボードが表示されるか確認
- [ ] 入力がCustomInputFieldに反映されるか確認
- [ ] Enterキーで元のフィールドに確定されるか確認
- [ ] 閉じるボタンで閉じるか確認

### フェーズ3: 最適化・統合（1日）

#### Step 7: パフォーマンス最適化

- レンダリング最適化
- アニメーション調整
- メモリリーク確認

#### Step 8: ドキュメント作成

- ユーザーマニュアル
- API仕様書
- サンプルコード

---

## 7. テスト計画

### 7.1 単体テスト

| テストケース | 期待結果 | 優先度 |
|------------|---------|--------|
| CustomInputFieldの表示 | 画面上半分に正しく表示される | 高 |
| Keyboardの表示 | 画面下半分に正しく表示される | 高 |
| テキスト入力 | CustomInputFieldにテキストが表示される | 高 |
| Enterキー | 元のフィールドにcommitされる | 高 |
| 閉じるボタン | オーバーレイが非表示になる | 中 |
| ESCキー | オーバーレイが非表示になる | 中 |

### 7.2 統合テスト

| テストケース | 期待結果 | 優先度 |
|------------|---------|--------|
| 通常モードとの切り替え | overlayMode切り替えで正しく動作 | 高 |
| 予測変換機能 | 既存の予測変換が動作する | 高 |
| 多言語対応 | 各言語レイアウトで動作する | 高 |
| フルスクリーンモード併用 | 両モードが競合しない | 中 |

### 7.3 受け入れテスト

| シナリオ | 確認項目 |
|---------|---------|
| 基本操作 | テキストフィールドタップ → 入力 → Enter → 確定 |
| 長文入力 | スクロール動作の確認 |
| 複数フィールド | フィールド切り替え時の動作 |
| パフォーマンス | 表示速度300ms以内 |

---

## 8. リスクと対策

### 8.1 技術リスク

| リスク | 影響度 | 対策 |
|-------|--------|------|
| InputContextとの同期問題 | 高 | ShadowInputContextの既存実装を活用 |
| レイアウト崩れ | 中 | 複数画面サイズでテスト |
| パフォーマンス低下 | 中 | プロファイリングツールで測定 |
| 既存機能の破壊 | 高 | overlayMode分岐で既存動作を保護 |

### 8.2 運用リスク

| リスク | 影響度 | 対策 |
|-------|--------|------|
| ユーザビリティ低下 | 中 | ユーザーテスト実施 |
| 保守コスト増加 | 中 | ドキュメント整備 |
| バージョンアップ時の互換性 | 低 | 既存APIのみ使用 |

---

## 9. 実装上の注意事項

### 9.1 既存機能との共存

```qml
// overlayMode と fullScreenMode の関係
InputPanel {
    overlayMode: true   // 画面全体を覆う

    Keyboard {
        fullScreenMode: false  // 通常のフルスクリーンモードは無効
        overlayMode: true      // オーバーレイモードを優先
    }
}
```

### 9.2 InputContextとの連携

```qml
// CustomInputFieldからInputContextへのcommit
CustomOverlayKeyboard {
    onEnterKeyPressed: {
        // 方法1: 直接commit
        InputContext.commit(inputField.text)

        // 方法2: setPreeditText → commit
        // InputContext.setPreeditText(inputField.text)
        // InputContext.commit()
    }
}
```

### 9.3 リソース管理

```qml
// オーバーレイ非表示時のリソース解放
CustomOverlayKeyboard {
    visible: active

    // activeがfalseになったらテキストをクリア
    onActiveChanged: {
        if (!active) {
            inputField.text = ""
        }
    }
}
```

### 9.4 タッチイベント処理

```qml
// オーバーレイ背景でタッチを吸収
Rectangle {
    anchors.fill: parent

    MouseArea {
        anchors.fill: parent
        preventStealing: true  // タッチイベントを下に伝播させない

        onClicked: {
            // 背景クリック時の動作（何もしない）
        }
    }
}
```

---

## 10. 実装完了基準

### 10.1 機能完了基準

- [ ] FR-01: 画面全体を覆う動作確認
- [ ] FR-02: 上半分に入力フィールド表示確認
- [ ] FR-03: Enterキーで元のフィールドに確定確認
- [ ] FR-04: 既存機能の動作確認（全テストパス）
- [ ] FR-05: 閉じるボタン動作確認

### 10.2 品質完了基準

- [ ] 単体テスト100%パス
- [ ] 統合テスト100%パス
- [ ] コードレビュー完了
- [ ] ドキュメント整備完了
- [ ] パフォーマンステスト基準クリア（<300ms）

### 10.3 リリース完了基準

- [ ] ビルド成功（リリースモード）
- [ ] 実機テスト完了（3デバイス以上）
- [ ] ユーザーマニュアル作成
- [ ] リリースノート作成

---

## 11. 付録

### 11.1 参考資料

- Qt Virtual Keyboard 5.12.10 ソースコード
- QVK_5.12_COMPONENT_SPECIFICATIONS.md
- QVK_5.12_ARCHITECTURE_OVERVIEW.md
- QVK_5.12_DATA_FLOW_SEQUENCE.md

### 11.2 用語集

| 用語 | 説明 |
|------|------|
| オーバーレイモード | 画面全体を覆うキーボード表示モード |
| InputContext | Qt入力メソッドフレームワークのコンテキスト |
| ShadowInputContext | フルスクリーンモード用の仮想入力コンテキスト |
| Preedit | 確定前のテキスト（予測変換中） |
| Commit | テキストの確定 |

### 11.3 変更履歴

| 日付 | バージョン | 変更内容 | 作成者 |
|------|----------|---------|--------|
| 2026-01-10 | 1.0 | 初版作成 | Claude |

---

**END OF DOCUMENT**
