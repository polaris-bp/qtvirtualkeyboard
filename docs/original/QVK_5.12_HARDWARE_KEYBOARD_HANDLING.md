# Qt Virtual Keyboard 5.12.10 - ハードウェアキーボード処理仕様

**バージョン**: 5.12.10
**最終更新**: 2026-01-11
**対象読者**: 開発者、システムインテグレーター

---

## 目次

1. [概要](#1-概要)
2. [イベント処理フロー](#2-イベント処理フロー)
3. [キーイベントフィルタリング](#3-キーイベントフィルタリング)
4. [矢印キーナビゲーション](#4-矢印キーナビゲーション)
5. [制約事項](#5-制約事項)
6. [実装詳細](#6-実装詳細)

---

## 1. 概要

### 1.1 ハードウェアキーボードとは

**ハードウェアキーボード**（物理キーボード）からの入力は、Qt Virtual Keyboardと併用可能です。

```
┌─────────────────────────────────────────────┐
│         アプリケーション                     │
│         (QLineEdit, QTextEdit)              │
└─────────────────────────────────────────────┘
           ↑                    ↑
           │                    │
    ┌──────┴──────┐      ┌─────┴─────┐
    │  物理キーボード│      │仮想キーボード│
    │  (ハードウェア)│      │   (Qt VKB)  │
    └──────────────┘      └───────────┘
```

### 1.2 基本動作

| シナリオ | 動作 |
|---------|------|
| **通常入力** | ハードウェアキーボードから直接入力 |
| **予測変換中に物理キー押下** | preeditTextを自動commit → 物理キー入力 |
| **矢印キーナビゲーション** | 条件付きサポート（設定により有効化） |
| **仮想キーボード表示中** | 両方の入力が可能 |

### 1.3 設計方針

Qt Virtual Keyboardは、**ハードウェアキーボードと仮想キーボードの共存**を想定しています。

**重要な原則**:
> "Break composing text since the virtual keyboard does not support hard keyboard events"
>
> （出典: `qvirtualkeyboardinputcontext_p.cpp:508`）

つまり：
- 仮想キーボードで予測変換中（preeditText）に物理キーを押すと、**自動的に確定**される
- その後、物理キーの入力が処理される

---

## 2. イベント処理フロー

### 2.1 全体フロー

```
[物理キーボードでキー押下]
    ↓
[QKeyEvent生成]
    ↓
[QGuiApplication::sendEvent()]
    ↓
[PlatformInputContext::eventFilter()]  ← イベントフィルタ
    ↓
[QVirtualKeyboardInputContextPrivate::filterEvent()]
    ↓
┌─────────────────────────────────────┐
│ KeyPress/KeyRelease判定              │
│   - activeKeys更新                   │
│   - 矢印キーナビゲーション処理       │
│   - preeditText自動commit            │
└─────────────────────────────────────┘
    ↓
[アプリケーションへ伝播]
```

### 2.2 イベントフィルタの役割

**ファイル**: `src/virtualkeyboard/platforminputcontext.cpp` (lines 209-214)

```cpp
bool PlatformInputContext::eventFilter(QObject *object, QEvent *event)
{
    if (event != m_filterEvent && object == m_focusObject && m_inputContext)
        return m_inputContext->priv()->filterEvent(event);
    return false;
}
```

**動作**:
1. フォーカスオブジェクトからのイベントをキャッチ
2. `QVirtualKeyboardInputContextPrivate::filterEvent()`に転送
3. `true`を返せばイベントを消費（アプリに伝播しない）
4. `false`を返せばアプリに伝播

---

## 3. キーイベントフィルタリング

### 3.1 filterEvent()の実装

**ファイル**: `src/virtualkeyboard/qvirtualkeyboardinputcontext_p.cpp` (lines 476-513)

```cpp
bool QVirtualKeyboardInputContextPrivate::filterEvent(const QEvent *event)
{
    QEvent::Type type = event->type();
    if (type == QEvent::KeyPress || type == QEvent::KeyRelease) {
        const QKeyEvent *keyEvent = static_cast<const QKeyEvent *>(event);

        // Keep track of pressed keys update key event state
        if (type == QEvent::KeyPress)
            activeKeys += keyEvent->nativeScanCode();
        else if (type == QEvent::KeyRelease)
            activeKeys -= keyEvent->nativeScanCode();

        if (activeKeys.isEmpty())
            clearState(State::KeyEvent);
        else
            setState(State::KeyEvent);

#ifdef QT_VIRTUALKEYBOARD_ARROW_KEY_NAVIGATION
        // 矢印キーナビゲーション処理（後述）
        // ...
#endif

        // Break composing text since the virtual keyboard
        // does not support hard keyboard events
        if (!preeditText.isEmpty())
            commit();
    }
    return false;
}
```

### 3.2 主要処理

#### ① キー状態の追跡

**ファイル**: `src/virtualkeyboard/qvirtualkeyboardinputcontext_p.cpp` (lines 483-491)

```cpp
// Keep track of pressed keys update key event state
if (type == QEvent::KeyPress)
    activeKeys += keyEvent->nativeScanCode();  // QSet::insert()
else if (type == QEvent::KeyRelease)
    activeKeys -= keyEvent->nativeScanCode();  // QSet::remove()

if (activeKeys.isEmpty())
    clearState(State::KeyEvent);
else
    setState(State::KeyEvent);
```

**用途**: 現在押されているキーを追跡

**nativeScanCode()**:
- プラットフォーム固有のスキャンコード（ハードウェアレベルの識別子）
- Qt::Key定数とは異なる（例: 同じ"A"キーでも異なるキーボードで異なるスキャンコード）
- quint32型

**activeKeysの型**: `QSet<quint32>` (line 169)
- セット型なので重複なし
- `+=`演算子で要素追加（`insert()`のエイリアス）
- `-=`演算子で要素削除（`remove()`のエイリアス）

#### ② 予測変換テキストの自動確定

```cpp
// preeditTextが存在する場合、自動commit
if (!preeditText.isEmpty())
    commit();
```

**重要**: これにより、仮想キーボードで予測変換中に物理キーを押すと、**予測変換が確定**されます。

**例**:

```
【シナリオ】
1. 仮想キーボードで "hel" と入力（preeditText = "hel"）
2. 予測候補: "hello", "help", "helicopter"
3. 物理キーボードで "Enter" キー押下

【動作】
1. filterEvent() が KeyPress を検知
2. preeditText.isEmpty() == false
3. commit() 実行 → "hel" が確定
4. その後 Enter キーイベントがアプリに伝播
```

---

## 4. 矢印キーナビゲーション

### 4.1 コンパイル時設定

矢印キーナビゲーションは、`QT_VIRTUALKEYBOARD_ARROW_KEY_NAVIGATION`マクロで有効化されます。

**ビルド時の定義**:

```qmake
# virtualkeyboard.pro
DEFINES += QT_VIRTUALKEYBOARD_ARROW_KEY_NAVIGATION
```

### 4.2 対象キー

以下のキーがナビゲーションキーとして扱われます：

| キー | Qt定数 | 用途 |
|------|--------|------|
| ← | `Qt::Key_Left` | 左移動 |
| → | `Qt::Key_Right` | 右移動 |
| ↑ | `Qt::Key_Up` | 上移動 |
| ↓ | `Qt::Key_Down` | 下移動 |
| Enter | `Qt::Key_Return` | 決定 |

### 4.3 実装詳細

**ファイル**: `src/virtualkeyboard/qvirtualkeyboardinputcontext_p.cpp` (lines 493-506)

```cpp
#ifdef QT_VIRTUALKEYBOARD_ARROW_KEY_NAVIGATION
int key = keyEvent->key();
if ((key >= Qt::Key_Left && key <= Qt::Key_Down) || key == Qt::Key_Return) {
    if (type == QEvent::KeyPress && platformInputContext->isInputPanelVisible()) {
        activeNavigationKeys += key;
        emit navigationKeyPressed(key, keyEvent->isAutoRepeat());
        return true;  // イベントを消費（アプリに伝播しない）
    } else if (type == QEvent::KeyRelease && activeNavigationKeys.contains(key)) {
        activeNavigationKeys -= key;
        emit navigationKeyReleased(key, keyEvent->isAutoRepeat());
        return true;  // イベントを消費（アプリに伝播しない）
    }
}
#endif
```

### 4.4 動作条件

矢印キーナビゲーションが動作する条件：

1. ✅ `QT_VIRTUALKEYBOARD_ARROW_KEY_NAVIGATION`が定義されている
2. ✅ 仮想キーボードが表示中（`isInputPanelVisible() == true`）
3. ✅ 対象キーが押下された

**重要**: この場合、イベントは**消費される**（`return true`）ため、アプリケーションには伝播しません。

### 4.5 シグナル

矢印キー押下時に発行されるシグナル：

**ファイル**: `src/virtualkeyboard/qvirtualkeyboardinputcontext_p.h` (lines 114-115)

```cpp
Q_SIGNALS:
    void navigationKeyPressed(int key, bool isAutoRepeat);
    void navigationKeyReleased(int key, bool isAutoRepeat);
```

**パラメータ**:
- `key`: Qt::Key定数（Qt::Key_Left, Qt::Key_Right, Qt::Key_Up, Qt::Key_Down, Qt::Key_Return）
- `isAutoRepeat`: キーリピート時true

**用途**: QML側で矢印キーによるキーボード内のフォーカス移動などを実装

**QMLからのアクセス**:

```qml
Connections {
    target: InputContext.priv  // privプロパティ経由でアクセス

    onNavigationKeyPressed: {
        console.log("Key pressed:", key, "Auto-repeat:", isAutoRepeat)
    }

    onNavigationKeyReleased: {
        console.log("Key released:", key)
    }
}
```

---

## 5. 制約事項

### 5.1 予測変換との共存不可

**制約**: ハードウェアキーボードと仮想キーボードの予測変換は共存できません。

```
❌ サポートされないシナリオ:
1. 仮想キーボードで "hel" と入力（予測変換中）
2. 物理キーボードで "l" を追加入力
3. 予測候補が "hell..." に更新される ← 不可能

✅ 実際の動作:
1. 仮想キーボードで "hel" と入力（予測変換中）
2. 物理キーボードで "l" を押下
3. "hel" が自動commit → "l" が追加入力
4. 結果: "hell" （予測変換は中断）
```

### 5.2 仮想キーボードの入力メソッドは物理キーに適用されない

**理由**: `filterEvent()`が常に`false`を返すため、物理キーイベントは基本的に**そのまま**アプリに伝播します。

```cpp
// filterEvent() の戻り値
return false;  // イベントを伝播させる
```

**例外**: 矢印キーナビゲーション有効時は`true`を返す（イベント消費）

### 5.3 IME連携は限定的

Qt Virtual Keyboardは、OS標準のIME（Input Method Editor）と完全には統合されていません。

| 機能 | サポート |
|------|---------|
| 物理キーボード → アプリ直接入力 | ✅ サポート |
| 物理キーボード → Qt VKB入力メソッド経由 | ❌ 非サポート |
| OS標準IME（日本語入力等） | ⚠ Qt VKBとは別系統 |

---

## 6. 実装詳細

### 6.1 PlatformInputContextの役割

**ファイル**: `src/virtualkeyboard/platforminputcontext_p.h`

```cpp
class PlatformInputContext : public QPlatformInputContext
{
    Q_OBJECT
public:
    // イベントフィルタ（ハードウェアキーイベントをキャッチ）
    virtual bool eventFilter(QObject *object, QEvent *event);

protected:
    // イベント送信（アプリへ）
    void sendEvent(QEvent *event);
    void sendKeyEvent(QKeyEvent *event);

private:
    QPointer<QObject> m_focusObject;  // フォーカス中のオブジェクト
    QEvent *m_filterEvent;             // フィルタ中のイベント
};
```

### 6.2 フォーカスオブジェクトへのイベントフィルタ登録

**ファイル**: `src/virtualkeyboard/platforminputcontext.cpp` (lines 186-199)

```cpp
void PlatformInputContext::setFocusObject(QObject *object)
{
    if (m_focusObject != object) {
        // 古いフォーカスオブジェクトからフィルタを削除
        if (m_focusObject)
            m_focusObject->removeEventFilter(this);

        m_focusObject = object;

        // 新しいフォーカスオブジェクトにフィルタを登録
        if (m_focusObject)
            m_focusObject->installEventFilter(this);

        emit focusObjectChanged();
    }
    update(Qt::ImQueryAll);
}
```

**動作**:
1. テキスト入力フィールドがフォーカスを得る
2. `setFocusObject()`が呼ばれる
3. `installEventFilter(this)`でイベントフィルタを登録
4. 以降、そのフィールドへのキーイベントは`eventFilter()`を経由

### 6.3 activeKeysの管理

**データ構造**:

```cpp
// qvirtualkeyboardinputcontext_p.h (line 169)
QSet<quint32> activeKeys;  // 現在押されているキーのスキャンコード

#ifdef QT_VIRTUALKEYBOARD_ARROW_KEY_NAVIGATION
QSet<int> activeNavigationKeys;  // line 167: アクティブなナビゲーションキー
#endif
```

**用途**:
- `activeKeys`: どのキー（スキャンコード）が押されているかを追跡
- `activeNavigationKeys`: どのナビゲーションキー（Qt::Key）が押されているかを追跡
- すべてのキーが離されたら状態をクリア

**状態管理**:

```cpp
if (activeKeys.isEmpty())
    clearState(State::KeyEvent);  // キーイベント状態クリア
else
    setState(State::KeyEvent);    // キーイベント状態セット
```

**State列挙型の定義** (lines 76-84):

```cpp
enum class State {
    Reselect = 0x1,            // 単語再選択状態
    InputMethodEvent = 0x2,    // InputMethodEvent処理中
    KeyEvent = 0x4,            // キーイベント処理中
    InputMethodClick = 0x8,    // 入力メソッドクリック中
    SyncShadowInput = 0x10     // ShadowInput同期中
};
Q_DECLARE_FLAGS(StateFlags, QVirtualKeyboardInputContextPrivate::State)
```

**状態管理関数** (lines 136-139):

```cpp
inline void setState(const State &state) { stateFlags.setFlag(state); }
inline void clearState(const State &state) { stateFlags &= ~StateFlags(state); }
inline bool testState(const State &state) const { return stateFlags.testFlag(state); }
inline bool isEmptyState() const { return !stateFlags; }
```

**QVirtualKeyboardScopedState** (lines 176-195):

スコープベースの状態管理ヘルパークラス：

```cpp
class QVirtualKeyboardScopedState
{
public:
    QVirtualKeyboardScopedState(QVirtualKeyboardInputContextPrivate *d,
                                QVirtualKeyboardInputContextPrivate::State state) :
        d(d), state(state)
    {
        d->setState(state);  // コンストラクタで状態セット
    }

    ~QVirtualKeyboardScopedState()
    {
        d->clearState(state);  // デストラクタで状態クリア
    }
};
```

**用途**: RAII（Resource Acquisition Is Initialization）パターンで状態を自動管理

### 6.4 sendKeyEvent()の実装

**ファイル**: `src/virtualkeyboard/platforminputcontext.cpp` (lines 235-244)

```cpp
void PlatformInputContext::sendKeyEvent(QKeyEvent *event)
{
    const QGuiApplication *app = qApp;
    QWindow *focusWindow = app ? app->focusWindow() : nullptr;
    if (focusWindow) {
        m_filterEvent = event;
        QGuiApplication::sendEvent(focusWindow, event);
        m_filterEvent = nullptr;
    }
}
```

**用途**: 仮想キーボードからのキーイベント送信

**m_filterEventの役割**:

```cpp
// eventFilter()内で
if (event != m_filterEvent && ...)  // ← 自分が送ったイベントは無視
    return m_inputContext->priv()->filterEvent(event);
```

これにより、**無限ループを防止**:
```
仮想キーボード → sendKeyEvent() → eventFilter() → filterEvent()
    ↑                                                    ↓
    └────────────────────────────────────────────────────┘
    （無限ループにならない: event == m_filterEventで判定）
```

---

## 7. 実践例

### 7.1 ハードウェアキーボードとの共存

```qml
import QtQuick 2.0
import QtQuick.VirtualKeyboard 2.3

ApplicationWindow {
    TextField {
        id: textField
        placeholderText: "タイプしてください"

        // 物理キーボードからの入力も受け付ける
        // Qt VKBは自動的にpreeditTextをcommitする
    }

    InputPanel {
        id: inputPanel
        anchors.bottom: parent.bottom

        // 物理キーボードと仮想キーボード両方で入力可能
    }
}
```

### 7.2 矢印キーナビゲーションの活用

```qml
// QT_VIRTUALKEYBOARD_ARROW_KEY_NAVIGATIONが有効な場合

Connections {
    target: InputContext.priv

    onNavigationKeyPressed: {
        console.log("Navigation key pressed:", key)

        // 仮想キーボード内のフォーカス移動
        if (key === Qt.Key_Left) {
            // 左のキーにフォーカス移動
        } else if (key === Qt.Key_Right) {
            // 右のキーにフォーカス移動
        }
        // ...
    }
}
```

### 7.3 予測変換の自動確定を考慮した実装

```cpp
// アプリケーション側での考慮事項

// ❌ 悪い例: 予測変換中かどうかをチェックしない
void MyWidget::keyPressEvent(QKeyEvent *event) {
    // 物理キーが押されると、preeditTextが自動commitされる
    // この時点でテキストが確定済みになっている可能性
    QWidget::keyPressEvent(event);
}

// ✅ 良い例: InputContextのシグナルを監視
Connections {
    target: InputContext

    onPreeditTextChanged: {
        console.log("Preedit:", InputContext.preeditText)
    }

    // 物理キー押下で自動commit
    // → preeditTextChanged (空文字列に)
    // → surroundingTextChanged (確定テキスト追加)
}
```

---

## 8. まとめ

### 8.1 重要ポイント

| 項目 | 説明 |
|------|------|
| **基本動作** | 物理キーボードとQt VKBは共存可能 |
| **予測変換** | 物理キー押下で自動commit |
| **矢印キー** | コンパイル時設定で有効化可能 |
| **イベント伝播** | 基本的にアプリに伝播（矢印キーは例外） |
| **IME統合** | 限定的（OS標準IMEとは別系統） |

### 8.2 設計時の考慮事項

1. **予測変換中の物理キー入力**
   - 自動commitされることを前提に設計
   - ユーザーに混乱を与えない

2. **矢印キーナビゲーション**
   - 必要に応じてコンパイル時に有効化
   - QML側でシグナルをハンドリング

3. **入力メソッドの選択**
   - Qt VKB: タッチデバイス向け
   - OS標準IME: 物理キーボード向け
   - 両方を併用する場合は動作確認が必要

### 8.3 デバッグのヒント

**ログ出力**:

```cpp
// virtualkeyboarddebug_p.h で定義されたログカテゴリ
Q_LOGGING_CATEGORY(qlcVirtualKeyboard, "qt.virtualkeyboard")

// 使用例
VIRTUALKEYBOARD_DEBUG() << "filterEvent:" << event->type();
```

**環境変数**:

```bash
export QT_LOGGING_RULES="qt.virtualkeyboard=true"
./myapp
```

これにより、キーイベント処理の詳細なログが出力されます。

---

## 9. 参考資料

### 9.1 ソースファイル

| ファイル | 役割 |
|---------|------|
| `platforminputcontext_p.h` | プラットフォーム統合層 |
| `platforminputcontext.cpp` | イベントフィルタ実装 |
| `qvirtualkeyboardinputcontext_p.h` | プライベート実装ヘッダー |
| `qvirtualkeyboardinputcontext_p.cpp` | filterEvent()実装 |

### 9.2 関連ドキュメント

- [QVK_5.12_ARCHITECTURE_OVERVIEW.md](QVK_5.12_ARCHITECTURE_OVERVIEW.md)
- [QVK_5.12_COMPONENT_SPECIFICATIONS.md](QVK_5.12_COMPONENT_SPECIFICATIONS.md)
- [QVK_5.12_DATA_FLOW_SEQUENCE.md](QVK_5.12_DATA_FLOW_SEQUENCE.md)

---

**END OF DOCUMENT**
