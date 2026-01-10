# Qt Virtual Keyboard 5.12.10 - コンポーネント詳細仕様

**バージョン**: 5.12.10
**最終更新**: 2026-01-10
**対象読者**: 開発者、カスタマイズ実装者

---

## 目次

1. [QMLコンポーネント](#1-qmlコンポーネント)
   - [InputPanel](#11-inputpanel)
   - [Keyboard](#12-keyboard)
   - [KeyboardLayout/KeyboardRow](#13-keyboardlayout--keyboardrow)
   - [BaseKey](#14-basekey)
   - [各種キー型](#15-各種キー型)
   - [ShadowInputControl](#16-shadowinputcontrol)
   - [SelectionControl](#17-selectioncontrol)
2. [C++クラス](#2-cクラス)
   - [QVirtualKeyboardInputContext](#21-qvirtualkeyboardinputcontext)
   - [QVirtualKeyboardInputEngine](#22-qvirtualkeyboardinputengine)
   - [QVirtualKeyboardSettings](#23-qvirtualkeyboardsettings)
   - [AbstractInputMethod](#24-abstractinputmethod)

---

## 1. QMLコンポーネント

### 1.1 InputPanel

**ファイル**: `/src/virtualkeyboard/content/InputPanel.qml`
**行数**: 147行
**役割**: アプリケーションに統合する最上位コンテナ

#### プロパティ

| プロパティ | 型 | 読取 | 書込 | 説明 |
|----------|-----|------|------|------|
| `active` | bool | ✓ | ✗ | キーボードの表示状態 (読取専用) |
| `externalLanguageSwitchEnabled` | bool | ✓ | ✓ | カスタム言語選択UI有効化 (default: false) |

#### シグナル

```qml
signal externalLanguageSwitch(var localeList, int currentIndex)
```

言語切替キー押下時に発行。カスタムUIで言語選択を実装可能。

**パラメータ**:
- `localeList`: 利用可能な言語コードの配列 (例: `["en_GB", "ja_JP"]`)
- `currentIndex`: 現在の言語インデックス

#### 内部構造

```qml
Item {
    id: inputPanel

    // サイズ計算 (lines 113-115)
    implicitHeight: keyboard.height - keyboard.wordCandidateView.y

    // テキスト選択ハンドル (lines 117-122)
    SelectionControl {
        objectName: "selectionControl"
        x: -parent.x
        y: -parent.y
        enabled: active && !keyboard.fullScreenMode
    }

    // キーボード本体 (lines 125-130)
    Keyboard {
        id: keyboard
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
    }

    // キーボード矩形をInputContextに通知 (lines 137-145)
    Binding {
        target: InputContext.priv
        property: "keyboardRectangle"
        value: mapToItem(null, keyboard.x, keyboard.y, keyboard.width, keyboard.height)
    }
}
```

#### 使用例

```qml
import QtQuick 2.0
import QtQuick.VirtualKeyboard 2.3

ApplicationWindow {
    id: window

    InputPanel {
        id: inputPanel
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        // カスタム言語選択
        externalLanguageSwitchEnabled: true
        onExternalLanguageSwitch: {
            // カスタムUIで言語選択
            languageDialog.open(localeList, currentIndex)
        }
    }
}
```

---

### 1.2 Keyboard

**ファイル**: `/src/virtualkeyboard/content/components/Keyboard.qml`
**行数**: 1621行
**役割**: キーボードのメインロジックと状態管理

#### 主要プロパティ

| プロパティ | 型 | デフォルト | 説明 |
|----------|-----|----------|------|
| `active` | bool | false | キーボードアクティブ状態 |
| `locale` | string | "en_GB" | 現在の言語ロケール |
| `localeIndex` | int | 0 | 言語リストのインデックス |
| `layoutType` | string | "main" | レイアウトタイプ |
| `fullScreenMode` | bool | false | フルスクリーンモード |
| `symbolMode` | bool | false | 記号モード |
| `handwritingMode` | bool | false | 手書きモード |
| `style` | KeyboardStyle | - | スタイルオブジェクト |

#### layoutTypeの自動判定 (lines 60-67)

```qml
readonly property string layoutType: {
    if (handwritingMode)
        return "handwriting"
    if (dialableCharactersOnly)    // Qt.ImhDialableCharactersOnly
        return "dialpad"
    if (formattedNumbersOnly)      // Qt.ImhFormattedNumbersOnly
        return "numbers"
    if (digitsOnly)                // Qt.ImhDigitsOnly
        return "digits"
    if (symbolMode)
        return "symbols"
    return "main"
}
```

#### 入力モード関連プロパティ (lines 91-133)

```qml
// 入力ヒントから判定
readonly property bool uppercase: InputContext.shift || InputContext.capsLock
readonly property bool dialableCharactersOnly:
    InputContext.inputMethodHints & Qt.ImhDialableCharactersOnly
readonly property bool formattedNumbersOnly:
    InputContext.inputMethodHints & Qt.ImhFormattedNumbersOnly
readonly property bool digitsOnly:
    InputContext.inputMethodHints & Qt.ImhDigitsOnly
readonly property bool latinOnly:
    InputContext.inputMethodHints & Qt.ImhLatinOnly
```

#### 主要メソッド

##### updateLayout() (lines 1406-1415)

言語/レイアウトタイプ変更時に呼び出される:

```qml
function updateLayout() {
    var newLayout = findLayout(locale, layoutType)
    if (!newLayout.length) {
        newLayout = findLayout(locale, "main")
        if (!newLayout.length)
            return
    }

    // 無駄な再読み込みを防ぐ
    if (layout !== newLayout) {
        layout = newLayout
    }
}
```

##### findLayout(localeName, layoutType) (lines 1598-1609)

レイアウトファイルを検索:

```qml
function findLayout(localeName, layoutType) {
    var layoutFile = getLayoutFile(localeName, layoutType)
    if (InputContext.priv.fileExists(layoutFile))
        return layoutFile

    // フォールバックチェック
    var fallbackFile = getFallbackFile(localeName, layoutType)
    if (InputContext.priv.fileExists(fallbackFile)) {
        layoutFile = getLayoutFile("fallback", layoutType)
        if (InputContext.priv.fileExists(layoutFile))
            return layoutFile
    }

    return ""
}
```

#### キーハンドリング (MultiPointTouchArea)

タッチイベント処理 (lines 1030-1082):

```qml
MultiPointTouchArea {
    id: keyboardInputArea
    anchors.fill: parent

    // タッチ開始
    onPressed: {
        keyboard.navigationModeActive = false

        for (var i = 0; i < touchPoints.length; i++) {
            var key = keyOnPoint(touchPoints[i].x, touchPoints[i].y)
            if (key) {
                initialKey = key
                activeTouchPoint = touchPoints[i]
                setActiveKey(key)
                press(key, true)
                break
            }
        }
    }

    // タッチ移動
    onUpdated: {
        if (!activeTouchPoint) return

        var key = keyOnPoint(activeTouchPoint.x, activeTouchPoint.y)
        if (key !== activeKey) {
            if (activeKey) {
                activeKey.active = false
            }
            setActiveKey(key)
        }
    }

    // タッチ終了
    onReleased: {
        if (activeTouchPoint && containsPoint(touchPoints, activeTouchPoint)) {
            releaseActiveKey()
        }
    }
}
```

##### press(key, isRealPress) (lines 861-868)

キー押下処理:

```qml
function press(key, isRealPress) {
    if (key && key.enabled) {
        if (!key.noKeyEvent) {
            InputContext.inputEngine.virtualKeyPress(
                key.key, key.text, keyboard.modifiers, key.repeat
            )
        }
        if (isRealPress) {
            soundEffect.play(key.soundEffect)
        }
    }
}
```

##### release(key) (lines 869-875)

キー解放処理:

```qml
function release(key) {
    if (key && key.enabled) {
        if (!key.noKeyEvent) {
            InputContext.inputEngine.virtualKeyRelease(
                key.key, key.text, keyboard.modifiers
            )
        }
        key.clicked()
    }
}
```

#### レイアウトローダー (lines 808-830)

```qml
Loader {
    id: keyboardLayoutLoader
    asynchronous: false
    anchors.fill: parent

    Binding {
        target: keyboardLayoutLoader
        property: "source"
        value: keyboard.layout
        when: keyboard.layout.length > 0
    }

    onItemChanged: {
        if (item) {
            if (item.inputMode !== undefined && item.inputMode !== -1)
                inputModeNeedsReset = true

            // 入力メソッド設定
            if (typeof item.createInputMethod === 'function') {
                customInputMethod = item.createInputMethod()
            }
        }
    }
}
```

---

### 1.3 KeyboardLayout / KeyboardRow

#### KeyboardLayout

**ファイル**: `/src/virtualkeyboard/content/components/KeyboardLayout.qml`
**行数**: 148行
**役割**: レイアウトのルートコンテナ

**プロパティ**:

| プロパティ | 型 | デフォルト | 説明 |
|----------|-----|----------|------|
| `inputMethod` | var | null | カスタム入力メソッド |
| `inputMode` | int | -1 | 入力モード (上書き用) |
| `keyWeight` | real | 160 | デフォルトキー幅 |
| `smallTextVisible` | bool | true | 代替キーヒント表示 |
| `sharedLayouts` | var | [] | 入力メソッド共有レイアウト |

**メソッド**:

```qml
function createInputMethod() {
    return null  // サブクラスでオーバーライド
}
```

**例**:

```qml
KeyboardLayout {
    keyWeight: 160  // デフォルトキー幅
    inputMode: InputEngine.InputMode.Latin

    KeyboardRow {
        // キー配置
    }
}
```

#### KeyboardRow

**ファイル**: `/src/virtualkeyboard/content/components/KeyboardRow.qml`
**行数**: 62行
**役割**: キーの水平行

**実装**:

```qml
RowLayout {
    id: keyboardRow

    property real keyWeight: parent ? parent.keyWeight : undefined
    property bool smallTextVisible: parent ? parent.smallTextVisible : false

    spacing: 0

    // 子要素のキーはLayout.preferredWidthを使用してweightに基づいてサイズ調整
}
```

**使用例**:

```qml
KeyboardRow {
    Key { key: Qt.Key_Q; text: "q"; weight: 160 }
    Key { key: Qt.Key_W; text: "w"; weight: 160 }
    BackspaceKey { weight: 240 }  // 1.5倍幅
}
```

---

### 1.4 BaseKey

**ファイル**: `/src/virtualkeyboard/content/components/BaseKey.qml`
**行数**: 250行
**役割**: すべてのキー型の基底クラス

#### プロパティ

| プロパティ | 型 | デフォルト | 説明 |
|----------|-----|----------|------|
| `weight` | real | - | 相対的な幅 (Layout.preferredWidthに使用) |
| `text` | string | "" | 入力テキスト |
| `displayText` | string | text | 表示テキスト |
| `smallText` | string | "" | 右上の小テキスト (代替キーヒント) |
| `smallTextVisible` | bool | - | smallText表示フラグ |
| `alternativeKeys` | var | undefined | 長押し代替キーリスト |
| `key` | int | Qt.Key_unknown | Qtキーコード |
| `noKeyEvent` | bool | false | キーイベント送信無効化 |
| `active` | bool | false | 押下状態 |
| `pressed` | bool | active | 視覚的押下状態 |
| `uppercased` | bool | keyboard.uppercased | 大文字表示 |
| `functionKey` | bool | false | 機能キーフラグ |
| `showPreview` | bool | true | プレビューバブル表示 |
| `enabled` | bool | true | キー有効/無効 |
| `repeat` | bool | false | オートリピート有効化 |
| `keyType` | int | QtVirtualKeyboard.KeyType.BaseKey | キー種別 |
| `soundEffect` | int | QtVirtualKeyboard.SoundEffect.KeyPress | 効果音 |

#### レイアウト

```qml
Item {
    id: keyItem

    Layout.fillWidth: false
    Layout.fillHeight: true
    Layout.preferredWidth: keyWeight  // weightに基づく

    Loader {
        id: keyPanel
        source: keyPanelDelegate  // スタイルから取得
        anchors.fill: parent
    }
}
```

#### シグナル

```qml
signal clicked()
```

カスタムハンドリング用。キー解放時に発行。

#### 使用例

```qml
import QtQuick 2.0
import QtQuick.VirtualKeyboard 2.1

BaseKey {
    key: Qt.Key_A
    text: "a"
    displayText: uppercased ? "A" : "a"
    alternativeKeys: "aäåãâàá"
    weight: 160

    onClicked: {
        console.log("Key clicked:", text)
    }
}
```

---

### 1.5 各種キー型

すべてBaseKeyを継承。

#### Key (標準文字キー)

**ファイル**: `Key.qml` (51行)

```qml
BaseKey {
    id: keyItem
    keyType: QtVirtualKeyboard.KeyType.Key

    // プロパティ: text, displayText, alternativeKeysを使用
}
```

**例**:
```qml
Key {
    key: Qt.Key_E
    text: "e"
    alternativeKeys: "êeëèé"
}
```

#### EnterKey

**ファイル**: `EnterKey.qml` (68行)

動的にラベルとアイコンが変化:

```qml
EnterKey {
    readonly property int actionId: InputContext.inputItem ?
        InputContext.inputItem.EnterKeyAction.actionId : EnterKeyAction.None

    readonly property string label: {
        switch (actionId) {
        case EnterKeyAction.Go: return "GO"
        case EnterKeyAction.Send: return "SEND"
        case EnterKeyAction.Next: return "NEXT"
        case EnterKeyAction.Done: return "DONE"
        case EnterKeyAction.Search: return "SEARCH"
        default: return ""
        }
    }

    key: Qt.Key_Return
    text: "\n"
    displayText: label
    functionKey: true
}
```

**アクション設定** (アプリケーション側):
```qml
TextField {
    id: emailField
    EnterKeyAction.actionId: EnterKeyAction.Next
    EnterKeyAction.label: "Next"
}
```

#### BackspaceKey

**ファイル**: `BackspaceKey.qml` (48行)

```qml
BaseKey {
    id: backspaceKey
    key: Qt.Key_Backspace
    repeat: true  // 長押しでリピート
    functionKey: true
    keyPanelDelegate: keyboard.style.backspaceKeyPanel
}
```

#### ShiftKey

**ファイル**: `ShiftKey.qml` (56行)

大文字/小文字切替:

```qml
BaseKey {
    id: shiftKey
    key: Qt.Key_Shift
    functionKey: true
    keyPanelDelegate: keyboard.style.shiftKeyPanel

    onClicked: {
        // ダブルクリックでCapsLock
        if (InputContext.capsLock) {
            InputContext.capsLock = false
            InputContext.shift = false
        } else if (InputContext.shift) {
            InputContext.capsLock = true
            InputContext.shift = false
        } else {
            InputContext.shift = true
        }
    }
}
```

#### SpaceKey

**ファイル**: `SpaceKey.qml` (50行)

現在の言語名を表示:

```qml
BaseKey {
    id: spaceKey
    key: Qt.Key_Space
    text: " "
    displayText: Qt.locale(InputContext.locale).nativeLanguageName
    repeat: true
    showPreview: false
    keyPanelDelegate: keyboard.style.spaceKeyPanel
}
```

#### その他のキー

| キー型 | ファイル | 主な用途 |
|-------|---------|---------|
| **SymbolModeKey** | SymbolModeKey.qml | 記号モード切替 |
| **ChangeLanguageKey** | ChangeLanguageKey.qml | 言語切替 |
| **HideKeyboardKey** | HideKeyboardKey.qml | キーボード非表示 |
| **HandwritingModeKey** | HandwritingModeKey.qml | 手書きモード切替 |
| **FillerKey** | FillerKey.qml | 空白スペース (レイアウト調整) |
| **NumberKey** | NumberKey.qml | 数字キー |

---

### 1.6 ShadowInputControl

**ファイル**: `/src/virtualkeyboard/content/components/ShadowInputControl.qml`
**行数**: 138行
**役割**: フルスクリーンモード時の入力フィールド

#### 有効化条件

```qml
Item {
    id: control
    enabled: keyboard.active && VirtualKeyboardSettings.fullScreenMode
}
```

#### 構造

```qml
Item {
    // 背景をブロック
    MouseArea { anchors.fill: parent }

    Loader {
        sourceComponent: keyboard.style.fullScreenInputContainerBackground

        Loader {
            sourceComponent: keyboard.style.fullScreenInputBackground

            Flickable {
                id: flickable
                flickableDirection: Flickable.HorizontalFlick
                interactive: contentWidth > width
                contentWidth: shadowInput.width

                TextInput {
                    id: shadowInput
                    objectName: "shadowInput"

                    // スタイルプロパティ
                    font: keyboard.style.fullScreenInputFont
                    color: keyboard.style.fullScreenInputColor
                    selectionColor: keyboard.style.fullScreenInputSelectionColor
                    selectedTextColor: keyboard.style.fullScreenInputSelectedTextColor
                    cursorDelegate: keyboard.style.fullScreenInputCursor

                    // 入力ヒント
                    inputMethodHints: InputContext.inputMethodHints
                    echoMode: (InputContext.inputMethodHints & Qt.ImhHiddenText) ?
                              TextInput.Password : TextInput.Normal

                    // カーソル同期
                    onCursorPositionChanged: {
                        cursorSyncTimer.restart()
                        blinkStatus = true
                    }

                    Timer {
                        id: cursorSyncTimer
                        interval: 0
                        onTriggered: {
                            var anchorPosition = shadowInput.getAnchorPosition()
                            if (anchorPosition !== InputContext.anchorPosition ||
                                shadowInput.cursorPosition !== InputContext.cursorPosition) {
                                InputContext.priv.forceCursorPosition(
                                    anchorPosition, shadowInput.cursorPosition
                                )
                            }
                        }
                    }

                    // カーソル点滅
                    property bool blinkStatus: true
                    Timer {
                        id: cursorTimer
                        interval: Qt.styleHints.cursorFlashTime / 2
                        repeat: true
                        running: true
                        onTriggered: shadowInput.blinkStatus = !shadowInput.blinkStatus
                    }
                }
            }
        }
    }

    // InputContextとバインド
    Binding {
        target: InputContext.priv.shadow
        property: "inputItem"
        value: shadowInput
        when: VirtualKeyboardSettings.fullScreenMode
    }
}
```

#### 動作

1. `VirtualKeyboardSettings.fullScreenMode = true`で有効化
2. キーボード上部に`TextInput`を表示
3. アプリケーションの入力フィールドは隠れる
4. 入力内容は`shadowInput`に表示され、InputContextを通じてアプリに同期
5. Enter等で確定すると、アプリケーションのフィールドに反映

---

### 1.7 SelectionControl

**ファイル**: `/src/virtualkeyboard/content/components/SelectionControl.qml`
**行数**: 100行
**役割**: テキスト選択ハンドル

#### 構造

```qml
Item {
    property bool handleIsMoving: false
    property var inputContext: InputContext

    visible: inputContext.selectionControlVisible && !InputContext.animating

    // アンカーハンドル (選択開始位置)
    Loader {
        id: anchorHandle
        sourceComponent: keyboard.style.selectionHandle

        x: inputContext.anchorRectangle.x - width / 2
        y: inputContext.anchorRectangle.y + inputContext.anchorRectangle.height

        MouseArea {
            anchors.fill: parent

            onPositionChanged: {
                handleIsMoving = true
                var xx = x + anchorHandle.x + mouse.x
                var yy = y + anchorHandle.y + mouse.y - keyboard.style.selectionHandleHeight
                inputContext.setSelectionOnFocusObject(Qt.point(xx, yy), anchorHandle)
            }

            onReleased: {
                handleIsMoving = false
            }
        }
    }

    // カーソルハンドル (選択終了位置)
    Loader {
        id: cursorHandle
        sourceComponent: keyboard.style.selectionHandle

        x: inputContext.cursorRectangle.x - width / 2
        y: inputContext.cursorRectangle.y + inputContext.cursorRectangle.height

        MouseArea {
            // anchorHandleと同様の実装
        }
    }
}
```

#### 動作

1. テキスト選択時に2つのハンドルが表示
2. ドラッグで選択範囲を調整
3. `InputContext.setSelectionOnFocusObject()`で選択を更新

---

## 2. C++クラス

### 2.1 QVirtualKeyboardInputContext

**ヘッダー**: `qvirtualkeyboardinputcontext.h`
**実装**: `qvirtualkeyboardinputcontext.cpp`
**役割**: 入力コンテキスト管理、アプリケーション連携

#### 主要プロパティ

```cpp
class QVirtualKeyboardInputContext : public QObject {
    Q_OBJECT

    // Shift/CapsLock状態
    Q_PROPERTY(bool shift READ isShiftActive WRITE setShift NOTIFY shiftActiveChanged)
    Q_PROPERTY(bool capsLock READ isCapsLockActive WRITE setCapsLock NOTIFY capsLockActiveChanged)
    Q_PROPERTY(bool uppercase READ isUppercase NOTIFY uppercaseChanged)

    // カーソル位置
    Q_PROPERTY(int anchorPosition READ anchorPosition NOTIFY anchorPositionChanged)
    Q_PROPERTY(int cursorPosition READ cursorPosition NOTIFY cursorPositionChanged)
    Q_PROPERTY(QRectF cursorRectangle READ cursorRectangle NOTIFY cursorRectangleChanged)
    Q_PROPERTY(QRectF anchorRectangle READ anchorRectangle NOTIFY anchorRectangleChanged)

    // テキスト
    Q_PROPERTY(QString preeditText READ preeditText WRITE setPreeditText NOTIFY preeditTextChanged)
    Q_PROPERTY(QString surroundingText READ surroundingText NOTIFY surroundingTextChanged)
    Q_PROPERTY(QString selectedText READ selectedText NOTIFY selectedTextChanged)

    // 入力ヒント
    Q_PROPERTY(Qt::InputMethodHints inputMethodHints READ inputMethodHints NOTIFY inputMethodHintsChanged)

    // 入力エンジン
    Q_PROPERTY(QVirtualKeyboardInputEngine *inputEngine READ inputEngine CONSTANT)

    // その他
    Q_PROPERTY(bool animating READ isAnimating WRITE setAnimating NOTIFY animatingChanged)
    Q_PROPERTY(QString locale READ locale WRITE setLocale NOTIFY localeChanged)
    Q_PROPERTY(QObject *inputItem READ inputItem NOTIFY inputItemChanged)
    Q_PROPERTY(ShiftHandler *shiftHandler READ shiftHandler CONSTANT)
};
```

#### 主要メソッド

##### setPreeditText() (予測変換テキスト設定)

```cpp
void setPreeditText(const QString &text,
                    QList<QInputMethodEvent::Attribute> attributes = QList<...>(),
                    int replaceFrom = 0,
                    int replaceLength = 0);
```

**用途**: 入力中の一時テキスト (下線付き) を設定

**例**:
```cpp
// "hello"を予測変換テキストとして設定
InputContext.setPreeditText("hello");
```

##### commit() (テキスト確定)

```cpp
Q_INVOKABLE void commit();
Q_INVOKABLE void commit(const QString &text, int replaceFrom = 0, int replaceLength = 0);
```

**用途**: テキストをアプリケーションに確定送信

**例**:
```cpp
// "Hello World"を確定
InputContext.commit("Hello World");
```

##### clear() (入力クリア)

```cpp
Q_INVOKABLE void clear();
```

**用途**: preeditTextをクリア

##### sendKeyClick() (キーイベント送信)

```cpp
Q_INVOKABLE void sendKeyClick(int key, const QString &text, int modifiers = 0);
```

**用途**: 直接キーイベントを送信

**例**:
```cpp
InputContext.sendKeyClick(Qt.Key_A, "a", 0);
```

#### シグナル

主要なシグナル:
- `shiftActiveChanged()`: Shift状態変更
- `cursorPositionChanged()`: カーソル位置変更
- `preeditTextChanged()`: 予測変換テキスト変更
- `inputItemChanged()`: フォーカスアイテム変更
- `localeChanged()`: 言語変更

---

### 2.2 QVirtualKeyboardInputEngine

**ヘッダー**: `qvirtualkeyboardinputengine.h`
**実装**: `qvirtualkeyboardinputengine.cpp`
**役割**: キーイベント処理、入力メソッド管理

#### 入力モード列挙型

```cpp
enum class InputMode {
    Latin,
    Numeric,
    Dialable,
    Pinyin,
    Cangjie,
    Zhuyin,
    Hangul,
    Hiragana,
    Katakana,
    FullwidthLatin,
    Greek,
    Cyrillic,
    Arabic,
    Hebrew,
    ChineseHandwriting,
    JapaneseHandwriting,
    KoreanHandwriting,
    Thai
};
Q_ENUM(InputMode)
```

#### 主要プロパティ

```cpp
Q_PROPERTY(Qt::Key activeKey READ activeKey WRITE setActiveKey NOTIFY activeKeyChanged)
Q_PROPERTY(Qt::Key previousKey READ previousKey NOTIFY previousKeyChanged)
Q_PROPERTY(InputMode inputMode READ inputMode WRITE setInputMode NOTIFY inputModeChanged)
Q_PROPERTY(QList<int> inputModes READ inputModes NOTIFY inputModesChanged)
Q_PROPERTY(QVirtualKeyboardAbstractInputMethod *inputMethod READ inputMethod WRITE setInputMethod NOTIFY inputMethodChanged)
Q_PROPERTY(bool wordCandidateListVisibleHint READ wordCandidateListVisibleHint WRITE setWordCandidateListVisibleHint NOTIFY wordCandidateListVisibleHintChanged)
```

#### 主要メソッド

##### virtualKeyPress/Release/Click()

```cpp
Q_INVOKABLE bool virtualKeyPress(Qt::Key key,
                                 const QString &text,
                                 Qt::KeyboardModifiers modifiers,
                                 bool repeat);

Q_INVOKABLE bool virtualKeyRelease(Qt::Key key,
                                   const QString &text,
                                   Qt::KeyboardModifiers modifiers);

Q_INVOKABLE bool virtualKeyClick(Qt::Key key,
                                 const QString &text,
                                 Qt::KeyboardModifiers modifiers);
```

**戻り値**: 入力メソッドが処理した場合true

**内部動作**:
```cpp
bool QVirtualKeyboardInputEnginePrivate::virtualKeyClick(
    Qt::Key key, const QString &text, Qt::KeyboardModifiers modifiers, bool isAutoRepeat)
{
    bool accept = false;
    if (inputMethod) {
        // 入力メソッドに委譲
        accept = inputMethod->keyEvent(key, text, modifiers);
        if (!accept) {
            // フォールバック (PlainInputMethod)
            accept = fallbackInputMethod->keyEvent(key, text, modifiers);
        }
        emit q->virtualKeyClicked(key, text, modifiers, isAutoRepeat);
    }
    return accept;
}
```

##### setInputMethod()

```cpp
void setInputMethod(QVirtualKeyboardAbstractInputMethod *inputMethod);
```

入力メソッドを動的に切り替え。

---

### 2.3 QVirtualKeyboardSettings

**ヘッダー**: `qvirtualkeyboardsettings_p.h`
**実装**: `qvirtualkeyboardsettings.cpp`
**役割**: グローバル設定管理

#### プロパティ

```cpp
class QVirtualKeyboardSettings : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString style READ style WRITE setStyle NOTIFY styleChanged)
    Q_PROPERTY(QString styleName READ styleName NOTIFY styleNameChanged)
    Q_PROPERTY(QString locale READ locale WRITE setLocale NOTIFY localeChanged)
    Q_PROPERTY(QStringList availableLocales READ availableLocales NOTIFY availableLocalesChanged)
    Q_PROPERTY(QStringList activeLocales READ activeLocales WRITE setActiveLocales NOTIFY activeLocalesChanged)
    Q_PROPERTY(bool fullScreenMode READ fullScreenMode WRITE setFullScreenMode NOTIFY fullScreenModeChanged)
    Q_PROPERTY(QString layoutPath READ layoutPath WRITE setLayoutPath NOTIFY layoutPathChanged)
};
```

#### 使用例

```qml
import QtQuick.VirtualKeyboard.Settings 2.2

Component.onCompleted: {
    // フルスクリーンモード有効化
    VirtualKeyboardSettings.fullScreenMode = true

    // 言語制限
    VirtualKeyboardSettings.activeLocales = ["en_GB", "ja_JP"]

    // カスタムスタイル
    VirtualKeyboardSettings.style = "qrc:/custom/style.qml"
}
```

---

### 2.4 AbstractInputMethod

**ヘッダー**: `qvirtualkeyboardabstractinputmethod_p.h`
**役割**: 入力メソッドプラグインの基底クラス

#### 主要仮想メソッド

```cpp
class QVirtualKeyboardAbstractInputMethod : public QObject {
    Q_OBJECT

public:
    // 入力モード
    virtual QList<InputMode> inputModes(const QString &locale) = 0;
    virtual bool setInputMode(const QString &locale, InputMode inputMode) = 0;
    virtual bool setTextCase(TextCase textCase) = 0;

    // キーイベント
    virtual bool keyEvent(Qt::Key key, const QString &text, Qt::KeyboardModifiers modifiers) = 0;

    // 候補リスト
    virtual QList<SelectionListModel::Type> selectionLists();
    virtual int selectionListItemCount(SelectionListModel::Type type);
    virtual QVariant selectionListData(SelectionListModel::Type type, int index, SelectionListModel::Role role);
    virtual void selectionListItemSelected(SelectionListModel::Type type, int index);

    // 手書き認識
    virtual QList<PatternRecognitionMode> patternRecognitionModes() const;
    virtual QVirtualKeyboardTrace *traceBegin(int traceId, PatternRecognitionMode patternRecognitionMode,
                                              const QVariantMap &traceCaptureDeviceInfo,
                                              const QVariantMap &traceScreenInfo);
    virtual bool traceEnd(QVirtualKeyboardTrace *trace);

public slots:
    virtual void reset();
    virtual void update();

signals:
    void selectionListChanged(SelectionListModel::Type type);
    void selectionListActiveItemChanged(SelectionListModel::Type type, int index);

protected:
    QVirtualKeyboardInputContext *inputContext() const;
};
```

#### PlainInputMethod (デフォルト実装)

```cpp
class PlainInputMethod : public QVirtualKeyboardAbstractInputMethod {
    QList<InputMode> inputModes(const QString &locale) override {
        return QList<InputMode>() << InputMode::Latin << InputMode::Numeric;
    }

    bool keyEvent(Qt::Key key, const QString &text, Qt::KeyboardModifiers modifiers) override {
        if (text.length() > 0) {
            inputContext()->commit(text);
            return true;
        }
        return false;
    }
};
```

#### カスタム入力メソッド例

```cpp
class MyInputMethod : public QVirtualKeyboardAbstractInputMethod {
    bool keyEvent(Qt::Key key, const QString &text, Qt::KeyboardModifiers modifiers) override {
        if (key == Qt::Key_Space) {
            // スペース押下時に予測変換候補を表示
            updateCandidates();
            return true;
        }

        // デフォルト処理
        inputContext()->setPreeditText(text);
        return true;
    }

    void selectionListItemSelected(SelectionListModel::Type type, int index) override {
        // 候補選択時
        QString selected = m_candidates.at(index);
        inputContext()->commit(selected);
        reset();
    }
};
```

---

## まとめ

このドキュメントでは、Qt Virtual Keyboard 5.12.10の各コンポーネントの詳細仕様を解説しました。

**次のステップ**:
- データフロー図でコンポーネント間の相互作用を理解
- スタイリングシステムガイドでUI外観のカスタマイズ方法を学習
- レイアウトシステムガイドで新しいキーボードレイアウトの作成方法を学習
