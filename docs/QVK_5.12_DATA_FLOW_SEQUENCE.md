# Qt Virtual Keyboard 5.12.10 - データフローとシーケンス図

**バージョン**: 5.12.10
**最終更新**: 2026-01-10
**対象読者**: 開発者、システムインテグレーター

---

## 目次

1. [入力処理フロー](#1-入力処理フロー)
2. [シーケンス図](#2-シーケンス図)
3. [状態遷移](#3-状態遷移)
4. [通信パターン](#4-通信パターン)

---

## 1. 入力処理フロー

### 1.1 キー入力の全体フロー

```
┌────────────────────────────────────────────────────────────────┐
│                        ユーザー操作                             │
└────────────────────────────────────────────────────────────────┘
                              ↓
┌────────────────────────────────────────────────────────────────┐
│                   MultiPointTouchArea                           │
│                    (Keyboard.qml)                               │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ onPressed:                                               │  │
│  │   1. keyOnPoint(x, y) → キー検出                         │  │
│  │   2. setActiveKey(key) → アクティブ化                    │  │
│  │   3. press(key, true) → 押下処理                         │  │
│  └──────────────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────────────┘
                              ↓
┌────────────────────────────────────────────────────────────────┐
│                  Keyboard.press(key)                            │
│                    (Keyboard.qml:861-868)                       │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ if (key.enabled && !key.noKeyEvent) {                    │  │
│  │   InputContext.inputEngine.virtualKeyPress(              │  │
│  │     key.key, key.text, modifiers, key.repeat            │  │
│  │   )                                                      │  │
│  │ }                                                        │  │
│  │ soundEffect.play(key.soundEffect)                       │  │
│  └──────────────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────────────┘
                              ↓
┌────────────────────────────────────────────────────────────────┐
│        QVirtualKeyboardInputEngine::virtualKeyPress()          │
│              (C++ - qvirtualkeyboardinputengine.cpp)           │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ 1. setActiveKey(key) → activeKeyプロパティ更新           │  │
│  │ 2. setPreviousKey(prevKey)                              │  │
│  │ 3. if (repeat) startRepeatTimer()                       │  │
│  │ 4. emit virtualKeyPressed(key, text, modifiers, repeat) │  │
│  └──────────────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────────────┘
                              ↓
                        ユーザーがキーを離す
                              ↓
┌────────────────────────────────────────────────────────────────┐
│             MultiPointTouchArea.onReleased                      │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ releaseActiveKey() → release(key)                        │  │
│  └──────────────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────────────┘
                              ↓
┌────────────────────────────────────────────────────────────────┐
│              Keyboard.release(key)                              │
│                  (Keyboard.qml:869-875)                         │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ if (key.enabled && !key.noKeyEvent) {                    │  │
│  │   InputContext.inputEngine.virtualKeyRelease(            │  │
│  │     key.key, key.text, modifiers                        │  │
│  │   )                                                      │  │
│  │ }                                                        │  │
│  │ key.clicked() → シグナル発行                             │  │
│  └──────────────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────────────┘
                              ↓
┌────────────────────────────────────────────────────────────────┐
│       QVirtualKeyboardInputEngine::virtualKeyRelease()         │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ 1. stopRepeatTimer()                                    │  │
│  │ 2. emit virtualKeyReleased(key, text, modifiers)        │  │
│  └──────────────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────────────┘
                              ↓
┌────────────────────────────────────────────────────────────────┐
│        QVirtualKeyboardInputEngine::virtualKeyClick()          │
│              (qvirtualkeyboardinputengine.cpp:69-83)           │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ bool accept = false;                                    │  │
│  │ if (inputMethod) {                                      │  │
│  │   // 入力メソッドに委譲                                  │  │
│  │   accept = inputMethod->keyEvent(key, text, modifiers); │  │
│  │   if (!accept) {                                        │  │
│  │     // フォールバック (PlainInputMethod)               │  │
│  │     accept = fallbackInputMethod->keyEvent(...);        │  │
│  │   }                                                     │  │
│  │   emit virtualKeyClicked(key, text, modifiers, ...);    │  │
│  │ }                                                       │  │
│  │ return accept;                                          │  │
│  └──────────────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────────────┘
                              ↓
┌────────────────────────────────────────────────────────────────┐
│          AbstractInputMethod::keyEvent()                        │
│                  (プラグイン実装)                               │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ 【パターンA: 予測変換あり】                              │  │
│  │   1. 候補生成                                           │  │
│  │   2. InputContext.setPreeditText("hel")                 │  │
│  │      → 下線付きテキスト表示                             │  │
│  │   3. 候補リスト更新                                     │  │
│  │                                                         │  │
│  │ 【パターンB: 直接入力 (PlainInputMethod)】              │  │
│  │   1. InputContext.commit("a")                           │  │
│  │      → 即座に確定                                       │  │
│  └──────────────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────────────┘
                              ↓
┌────────────────────────────────────────────────────────────────┐
│         QVirtualKeyboardInputContext                            │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ setPreeditText() または commit()                         │  │
│  └──────────────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────────────┘
                              ↓
┌────────────────────────────────────────────────────────────────┐
│              QInputMethodEvent                                  │
│             (Qt Framework)                                      │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ QInputMethodEvent event(preeditText, attributes)         │  │
│  │ または                                                   │  │
│  │ QInputMethodEvent event(commitString, ...)               │  │
│  │                                                         │  │
│  │ QCoreApplication::sendEvent(focusObject, &event)        │  │
│  └──────────────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────────────┘
                              ↓
┌────────────────────────────────────────────────────────────────┐
│           アプリケーション                                      │
│         (QLineEdit / QTextEdit)                                 │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │ inputMethodEvent(QInputMethodEvent *event)               │  │
│  │   → テキスト表示更新                                     │  │
│  └──────────────────────────────────────────────────────────┘  │
└────────────────────────────────────────────────────────────────┘
```

### 1.2 PreeditとCommitの違い

#### Preedit (予測変換中)

```
アプリケーション表示:
┌─────────────────────┐
│ Hello w[orld]       │  ← [...]が予測変換中テキスト (下線付き)
└─────────────────────┘

InputContext状態:
- preeditText: "orld"
- surroundingText: "Hello w"
- cursorPosition: 7
```

**コード**:
```cpp
InputContext.setPreeditText("orld");
```

#### Commit (確定)

```
アプリケーション表示:
┌─────────────────────┐
│ Hello world|        │  ← |がカーソル位置
└─────────────────────┘

InputContext状態:
- preeditText: ""
- surroundingText: "Hello world"
- cursorPosition: 11
```

**コード**:
```cpp
InputContext.commit("orld");
```

### 1.3 候補選択フロー

```
[ユーザーが"h"+"e"+"l"を入力]
    ↓
[InputMethod.keyEvent() × 3回]
    ↓
[setPreeditText("hel")]
    ↓
[候補生成: "hello", "help", "helicopter"]
    ↓
[wordCandidateView に表示]
    ↓
[ユーザーが"hello"を選択]
    ↓
[wordCandidateView.onItemSelected(index)]
    ↓
[InputMethod.selectionListItemSelected(Type.Word, index)]
    ↓
[commit("hello")]
    ↓
[アプリケーションに"hello"が確定入力]
```

---

## 2. シーケンス図

### 2.1 通常の文字入力

```
ユーザー    MultiPointTouchArea   Keyboard     InputEngine    InputMethod  InputContext  App
   |                |                 |              |              |             |        |
   |--タップ------->|                 |              |              |             |        |
   |                |--onPressed----->|              |              |             |        |
   |                |                 |--press(key)->|              |             |        |
   |                |                 |              |--virtualKeyPress()         |        |
   |                |                 |              |              |             |        |
   |--リリース----->|                 |              |              |             |        |
   |                |--onReleased---->|              |              |             |        |
   |                |                 |--release(key)->              |             |        |
   |                |                 |              |--virtualKeyRelease()       |        |
   |                |                 |              |--virtualKeyClick()         |        |
   |                |                 |              |              |             |        |
   |                |                 |              |--keyEvent(key, text)       |        |
   |                |                 |              |              |             |        |
   |                |                 |              |<--return true-------------|        |
   |                |                 |              |                            |        |
   |                |                 |              |                            |        |
   |                |                 |              |           commit(text)---->|        |
   |                |                 |              |              |             |        |
   |                |                 |              |              |<--QInputMethodEvent-->
   |                |                 |              |              |             |        |
   |                |                 |              |              |             |<--inputMethodEvent()
   |                |                 |              |              |             |        |
   |<---画面更新---------------------------------------------------------
```

### 2.2 予測変換入力

```
ユーザー  Keyboard   InputEngine   HunspellInputMethod   InputContext   wordCandidateView   App
   |         |            |                  |                 |                |            |
   |--"h"--->|            |                  |                 |                |            |
   |         |--virtualKeyClick("h")         |                 |                |            |
   |         |            |--keyEvent("h")->|                 |                |            |
   |         |            |                  |--候補生成: ["hi", "he", "hello"]  |            |
   |         |            |                  |                 |                |            |
   |         |            |                  |--setPreeditText("h")             |            |
   |         |            |                  |                 |                |            |
   |         |            |<--emit selectionListChanged()------|                |            |
   |         |            |                  |                 |                |            |
   |         |<--wordCandidateListVisibleHint = true-----------|                |            |
   |         |            |                  |                 |                |            |
   |<--------候補リスト表示----------------------------------------------------|            |
   |         |            |                  |                 |                |            |
   |--"e"--->|            |                  |                 |                |            |
   |         |--virtualKeyClick("e")         |                 |                |            |
   |         |            |--keyEvent("e")->|                 |                |            |
   |         |            |                  |--候補更新: ["help", "hello", "heat"]            |
   |         |            |                  |                 |                |            |
   |         |            |                  |--setPreeditText("he")            |            |
   |         |            |<--emit selectionListChanged()------|                |            |
   |         |            |                  |                 |                |            |
   |<--------候補リスト更新---------------------------------------------------|            |
   |         |            |                  |                 |                |            |
   |--候補"hello"選択----------------------------------------------------------->|            |
   |         |            |                  |                 |                |            |
   |         |            |--selectionListItemSelected(0)----->|                |            |
   |         |            |                  |                 |                |            |
   |         |            |                  |--commit("hello")                 |            |
   |         |            |                  |                 |                |            |
   |         |            |                  |                 |<--QInputMethodEvent-------->|
   |         |            |                  |                 |                |            |
   |<---"hello"表示-----------------------------------------------------------
```

### 2.3 フルスクリーンモード

```
ユーザー  App   InputContext  VirtualKeyboardSettings  ShadowInputControl  Keyboard
   |       |         |                  |                      |               |
   |--フォーカス---->|                  |                      |               |
   |       |         |--show()--------->|                      |               |
   |       |         |                  |<--fullScreenMode=true               |
   |       |         |                  |                      |               |
   |       |         |                  |-------enabled=true-->|               |
   |       |         |                  |                      |               |
   |<------shadowInput表示-----------------------------|               |
   |       |         |                  |                      |               |
   |--キー入力-------------------------------------------------------------->|
   |       |         |                  |                      |               |
   |       |         |<--commit("text")----------------------------------------|
   |       |         |                  |                      |               |
   |       |         |                  |<--shadowInput.text = "text"--------|
   |       |         |                  |                      |               |
   |--Enter押下-------------------------------------------------------------->|
   |       |         |                  |                      |               |
   |       |<--QInputMethodEvent("text")----------------------------------|
   |       |         |                  |                      |               |
   |<--AppのフィールドにCommit--------------------------------------------
```

### 2.4 言語切替

```
ユーザー  ChangeLanguageKey  Keyboard      InputEngine    FolderListModel   KeyboardLayoutLoader
   |            |               |                |                |                  |
   |--言語切替キー押下---------->|                |                |                  |
   |            |               |                |                |                  |
   |            |--clicked()-->|                |                |                  |
   |            |               |                |                |                  |
   |            |               |--changeInputLanguage()          |                  |
   |            |               |                |                |                  |
   |            |               |--nextLocaleIndex()              |                  |
   |            |               |                |                |                  |
   |            |               |<--新しいlocaleIndex-------------|                  |
   |            |               |                |                |                  |
   |            |               |--localeIndex = newIndex         |                  |
   |            |               |                |                |                  |
   |            |               |--onLocaleIndexChanged           |                  |
   |            |               |                |                |                  |
   |            |               |--updateLayout()                 |                  |
   |            |               |                |                |                  |
   |            |               |--findLayout(locale, layoutType) |                  |
   |            |               |                |--fileExists()-->|                  |
   |            |               |                |<--true----------|                  |
   |            |               |                |                |                  |
   |            |               |--layout = newLayoutPath         |                  |
   |            |               |                |                |                  |
   |            |               |                |                |--onSourceChanged->|
   |            |               |                |                |                  |
   |            |               |                |                |--Loaderが新レイアウト読込
   |            |               |                |                |                  |
   |<---新しいレイアウト表示-----------------------------------------
```

---

## 3. 状態遷移

### 3.1 Shift/CapsLock状態遷移

```
          ┌──────────────┐
          │    Normal    │
          │  (小文字)     │
          └──────────────┘
               │     ▲
  Shiftキー押下│     │他のキー入力
               │     │(自動復帰)
               ▼     │
          ┌──────────────┐
          │    Shift     │
          │  (大文字)     │
          └──────────────┘
               │     ▲
 Shiftキー押下│     │Shiftキー押下
 (ダブルタップ)│     │(解除)
               ▼     │
          ┌──────────────┐
          │   CapsLock   │
          │  (大文字固定) │
          └──────────────┘
```

**実装** (ShiftKey.qml):
```qml
onClicked: {
    if (InputContext.capsLock) {
        // CapsLock → Normal
        InputContext.capsLock = false
        InputContext.shift = false
    } else if (InputContext.shift) {
        // Shift → CapsLock
        InputContext.capsLock = true
        InputContext.shift = false
    } else {
        // Normal → Shift
        InputContext.shift = true
    }
}

// 自動復帰ロジック (Keyboard.qml)
Connections {
    target: InputContext
    onPreeditTextChanged: {
        if (InputContext.shift && !InputContext.capsLock) {
            InputContext.shift = false  // Shiftを自動解除
        }
    }
}
```

### 3.2 レイアウトモード状態遷移

```
                  ┌────────────────┐
                  │      Main      │
                  │   (文字入力)    │
                  └────────────────┘
                       │      ▲
        SymbolModeキー │      │ SymbolModeキー
                       ▼      │
                  ┌────────────────┐
                  │    Symbols     │
                  │   (記号入力)    │
                  └────────────────┘
                       │      ▲
  HandwritingModeキー  │      │ キーボードモード復帰
                       ▼      │
                  ┌────────────────┐
                  │  Handwriting   │
                  │   (手書き)      │
                  └────────────────┘

  入力ヒント変更 (Qt.ImhDigitsOnly等)
                       ↓
                  ┌────────────────┐
                  │  Digits/Dialpad│
                  │  Numbers など   │
                  └────────────────┘
```

**実装** (Keyboard.qml:60-67):
```qml
readonly property string layoutType: {
    // 優先度順に判定
    if (handwritingMode) return "handwriting"
    if (dialableCharactersOnly) return "dialpad"      // Qt.ImhDialableCharactersOnly
    if (formattedNumbersOnly) return "numbers"        // Qt.ImhFormattedNumbersOnly
    if (digitsOnly) return "digits"                   // Qt.ImhDigitsOnly
    if (symbolMode) return "symbols"
    return "main"
}
```

### 3.3 キーボードアクティブ状態

```
        ┌──────────────┐
   ┌───>│   Inactive   │<───┐
   │    │ (非表示)      │    │
   │    └──────────────┘    │
   │            │            │
   │  フォーカスイン        │ フォーカスアウト
   │  show()               │ hide()
   │            │            │
   │            ▼            │
   │    ┌──────────────┐    │
   └────│    Active    │────┘
        │   (表示中)    │
        └──────────────┘
```

**実装** (InputPanel.qml:62):
```qml
property bool active: keyboard.active

Keyboard {
    id: keyboard
    active: Qt.inputMethod.visible  // Qtフレームワークと同期
}
```

---

## 4. 通信パターン

### 4.1 QML → C++ 通信

#### パターン1: プロパティ変更

```qml
// QMLから
InputContext.shift = true

// C++で
void QVirtualKeyboardInputContext::setShift(bool enable) {
    if (d->shift != enable) {
        d->shift = enable;
        emit shiftActiveChanged();  // シグナル発行
    }
}
```

#### パターン2: メソッド呼び出し

```qml
// QMLから
InputContext.commit("text")

// C++で
void QVirtualKeyboardInputContext::commit(const QString &text, ...) {
    QInputMethodEvent event;
    event.setCommitString(text, ...);
    sendInputMethodEvent(&event);
}
```

#### パターン3: Q_INVOKABLE メソッド

```qml
// QMLから
var exists = InputContext.priv.fileExists("path/to/layout.qml")

// C++で
class QVirtualKeyboardInputContextPrivate : public QObject {
    Q_INVOKABLE bool fileExists(const QString &path) {
        return QFile::exists(path);
    }
};
```

### 4.2 C++ → QML 通信

#### パターン1: プロパティ通知

```cpp
// C++で状態変更
void QVirtualKeyboardInputContext::setCursorPosition(int position) {
    if (d->cursorPosition != position) {
        d->cursorPosition = position;
        emit cursorPositionChanged();  // シグナル発行
    }
}
```

```qml
// QMLで監視
Connections {
    target: InputContext
    onCursorPositionChanged: {
        console.log("Cursor moved to:", InputContext.cursorPosition)
    }
}
```

#### パターン2: カスタムシグナル

```cpp
// C++でシグナル発行
signals:
    void virtualKeyClicked(Qt::Key key, const QString &text,
                          Qt::KeyboardModifiers modifiers, bool isAutoRepeat);
```

```qml
// QMLで受信
Connections {
    target: InputContext.inputEngine
    onVirtualKeyClicked: {
        console.log("Key clicked:", key, text)
    }
}
```

### 4.3 InputMethod ⇄ InputContext 通信

#### InputMethod → InputContext

```cpp
// プラグイン側
class HunspellInputMethod : public QVirtualKeyboardAbstractInputMethod {
    bool keyEvent(Qt::Key key, const QString &text, ...) override {
        // 候補生成
        QStringList candidates = generateCandidates(text);

        // Preedit設定
        inputContext()->setPreeditText(text);

        // 候補リスト更新
        emit selectionListChanged(SelectionListModel::Type::WordCandidateList);

        return true;
    }
};
```

#### InputContext → InputMethod

```cpp
// InputContextから入力メソッドへの通知
void QVirtualKeyboardInputContext::setShift(bool enable) {
    d->shift = enable;

    // 入力メソッドに通知
    if (d->inputEngine && d->inputEngine->inputMethod()) {
        d->inputEngine->inputMethod()->update();  // 状態更新要求
    }

    emit shiftActiveChanged();
}
```

### 4.4 Qt Framework連携

#### アプリケーション → InputContext

```cpp
// QLineEditがフォーカスを取得
void QLineEdit::focusInEvent(QFocusEvent *event) {
    QWidget::focusInEvent(event);

    // Qt入力メソッドフレームワークに通知
    Qt::InputMethodQuery query = Qt::ImQueryAll;
    QInputMethodQueryEvent queryEvent(query);
    QCoreApplication::sendEvent(this, &queryEvent);

    // → PlatformInputContext::update() が呼ばれる
    // → QVirtualKeyboardInputContext::update()
    // → キーボード表示
}
```

#### InputContext → アプリケーション

```cpp
// InputContextからアプリへテキスト送信
void QVirtualKeyboardInputContext::sendInputMethodEvent(QInputMethodEvent *event) {
    QObject *fo = qGuiApp->focusObject();
    if (fo) {
        QCoreApplication::sendEvent(fo, event);  // QLineEdit等にイベント送信
    }
}
```

---

## 5. データフロー図解

### 5.1 コンポーネント間データフロー

```
┌───────────────────────────────────────────────────────────────┐
│                    Application Layer                          │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐           │
│  │  QLineEdit  │  │  QTextEdit  │  │  Custom     │           │
│  └─────────────┘  └─────────────┘  └─────────────┘           │
└───────────────────────────────────────────────────────────────┘
         ▲                   │
         │ QInputMethodEvent │ Focus Events
         │                   ▼
┌───────────────────────────────────────────────────────────────┐
│              Qt Platform Input Context                        │
│  ┌─────────────────────────────────────────────────────────┐  │
│  │         PlatformInputContext                            │  │
│  │  - show() / hide()                                      │  │
│  │  - update() / reset()                                   │  │
│  │  - commit() / setFocusObject()                          │  │
│  └─────────────────────────────────────────────────────────┘  │
└───────────────────────────────────────────────────────────────┘
         ▲                   │
         │                   ▼
┌───────────────────────────────────────────────────────────────┐
│                  VKB C++ Core                                 │
│  ┌────────────────────┐      ┌────────────────────┐           │
│  │  InputContext      │◀────▶│  InputEngine       │           │
│  │  - preeditText     │      │  - activeKey       │           │
│  │  - cursorPosition  │      │  - inputMode       │           │
│  │  - shift/capsLock  │      │  - virtualKeyXXX() │           │
│  └────────────────────┘      └────────────────────┘           │
│         ▲                            │                        │
│         │                            ▼                        │
│         │                  ┌────────────────────┐             │
│         │                  │  InputMethod       │             │
│         │                  │  Plugin            │             │
│         │                  │  - keyEvent()      │             │
│         │                  │  - candidates      │             │
│         └──────────────────│  - traceXXX()      │             │
│                            └────────────────────┘             │
└───────────────────────────────────────────────────────────────┘
         ▲                   │
         │ Property Bindings │ Signal/Slot
         │                   ▼
┌───────────────────────────────────────────────────────────────┐
│                      VKB QML UI                               │
│  ┌────────────────┐    ┌────────────────┐                     │
│  │  InputPanel    │───▶│  Keyboard      │                     │
│  │                │    │  - layoutType  │                     │
│  └────────────────┘    │  - locale      │                     │
│         │              │  - active      │                     │
│         │              └────────────────┘                     │
│         │                      │                              │
│         ▼                      ▼                              │
│  ┌────────────────┐    ┌────────────────┐                     │
│  │SelectionControl│    │KeyboardLayout  │                     │
│  │                │    │  Loader        │                     │
│  └────────────────┘    └────────────────┘                     │
│                                │                              │
│                                ▼                              │
│                        ┌────────────────┐                     │
│                        │  Keys          │                     │
│                        │  (BaseKey等)   │                     │
│                        └────────────────┘                     │
└───────────────────────────────────────────────────────────────┘
         ▲                   │
         │                   │ Touch Events
         │                   ▼
┌───────────────────────────────────────────────────────────────┐
│                          User                                 │
└───────────────────────────────────────────────────────────────┘
```

---

## まとめ

このドキュメントでは、Qt Virtual Keyboard 5.12.10のデータフローとシーケンスを詳細に解説しました。

**重要なポイント**:
1. **タッチイベント → キーイベント → 入力メソッド → テキスト確定** の明確なフロー
2. **Preedit (予測変換中) と Commit (確定)** の2段階入力
3. **QML ⇄ C++** の双方向通信パターン
4. **InputContext** がハブとして全体を調整

**次のステップ**:
- スタイリングシステムガイドでUIカスタマイズ方法を学習
- レイアウトシステムガイドで新しいキーボードレイアウトの作成方法を学習
