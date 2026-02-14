# 実装ガイド: 物理キーボード ナビゲーションキーイベント間引き処理

## 1. 前回分析の訂正事項

前回の分析では仮想キーボード（タッチ入力）側のイベントパスと物理キーボード側のイベントパスを混同していた箇所があった。以下に訂正を明記する。

### 前回の案1「timerEvent のリピートレート制御」→ 誤り

`QVirtualKeyboardInputEngine::timerEvent()` (`qvirtualkeyboardinputengine.cpp:683-694`) は
**仮想キーボード上のキーを長押しした際のリピート処理専用**である。
物理キーボードのキーリピートはOS側で生成され、`timerEvent` は一切関与しない。

```
仮想キーボード: タッチ長押し → virtualKeyPress(repeat=true) → startTimer(600) → timerEvent → virtualKeyClick
物理キーボード: OSキーリピート → QKeyEvent(isAutoRepeat=true) → eventFilter → filterEvent
```

したがって、timerEvent のインターバル変更は**物理キーボードの問題には効果がない**。

### 前回の案3「virtualKeyClick レベルのスロットリング」→ 誤り

`virtualKeyClick()` (`qvirtualkeyboardinputengine.cpp:46-63`) は仮想キーボードの
キーイベントルーティング専用のメソッドである。物理キーボードの矢印キーは
`filterEvent()` で消費（return true）されるため、`virtualKeyClick` には到達しない。

### 前回の案4「sendKeyClick でのスロットリング」→ 誤り

`sendKeyClick()` (`qvirtualkeyboardinputcontext.cpp:195-224`) は `FallbackInputMethod` が
仮想キーボードのキーイベントをアプリケーションに転送する際に呼ばれるメソッドであり、
物理キーボードの矢印キーイベントはこのパスを通らない。

---

## 2. 物理キーボードイベントの正確なフロー

```
物理キーボード押下
    │
    ▼
OS/ウィンドウシステムが QKeyEvent を生成
 (長押し時: isAutoRepeat=true のイベントをOS側のリピートレートで連続生成)
    │
    ▼
フォーカスオブジェクトにイベント配信
    │
    ▼
PlatformInputContext::eventFilter()          ← [platforminputcontext.cpp:192-197]
  条件: event != m_filterEvent && object == m_focusObject && m_inputContext
    │
    ▼
QVirtualKeyboardInputContextPrivate::filterEvent()  ← [qvirtualkeyboardinputcontext_p.cpp:587-644]
    │
    ├─[矢印キー + パネル表示中 + KeyPress]
    │   activeNavigationKeys += key                  ← [行608]
    │   emit navigationKeyPressed(key, isAutoRepeat) ← [行609]
    │   return true  ← イベント消費（アプリに届かない）  [行610]
    │
    ├─[矢印キー + activeSetに含む + KeyRelease]
    │   activeNavigationKeys -= key                    ← [行612]
    │   emit navigationKeyReleased(key, isAutoRepeat)  ← [行613]
    │   return true  ← イベント消費                      [行614]
    │
    └─[その他のキー]
        return false ← イベントをアプリに通過              [行643]
    │
    ▼
Keyboard.qml: onNavigationKeyPressed ハンドラ           ← [Keyboard.qml:161-409]
    │
    ├─ Qt.Key_Left  → navigateToNextKey(-1*dir, 0, false)  [行218]
    ├─ Qt.Key_Right → navigateToNextKey( 1*dir, 0, false)  [行327]
    ├─ Qt.Key_Up    → navigateToNextKey(0, -1, ...)         [行269]
    ├─ Qt.Key_Down  → navigateToNextKey(0,  1, ...)         [行380]
    └─ Qt.Key_Return → press()/release() activeKey          [行395-399]
    │
    ▼
navigateToNextKey() → nextKeyInNavigation()             ← [Keyboard.qml:1095-1094]
  navigationCursor を更新
    │
    ▼
navigationHighlight (Loader) が視覚的に追従              ← [Keyboard.qml:604-648]
  Behavior on x/y: NumberAnimation { duration: 200ms }  ← [行636-647]
```

### 重要な事実

1. **スロットリング機構は一切存在しない**: filterEvent は受け取った全てのキーイベントを
   そのまま `navigationKeyPressed` シグナルとして発火する
2. **矢印キーの isAutoRepeat はチェックされていない**: QML側で `isAutoRepeat` を
   無視しているのは `Qt.Key_Return` のみ（行389,395,401,418）。矢印キーには
   isAutoRepeat のガードがない
3. **アニメーションは200msだが動作は即座に実行**: `navigationHighlight` の移動アニメーション
   は200msだが、`navigateToNextKey()` による論理的なカーソル位置更新は即座に実行される。
   高速なリピートイベントが来ると、アニメーション完了前に次の位置に飛ぶ

---

## 3. 実装案（おすすめ度順）

---

### 案A（最推奨）: filterEvent() での C++ レベルスロットリング

**変更対象ファイル:**
- `src/virtualkeyboard/qvirtualkeyboardinputcontext_p.h` (メンバ変数追加)
- `src/virtualkeyboard/qvirtualkeyboardinputcontext_p.cpp` (filterEvent 修正)
- `src/virtualkeyboard/settings_p.h` / `settings.cpp` (設定追加)

**設計:**
ナビゲーションキーイベントのシグナル発火前に `QElapsedTimer` で経過時間を確認し、
閾値未満のイベントを消費（破棄）する。

**変更箇所1: Settings にプロパティ追加**

```cpp
// settings_p.h - Settings クラス内 public セクションに追加
int navigationKeyRepeatInterval() const;
void setNavigationKeyRepeatInterval(int interval);
void resetNavigationKeyRepeatInterval();

// signals セクションに追加
void navigationKeyRepeatIntervalChanged();
```

```cpp
// settings.cpp - SettingsPrivate コンストラクタ初期化リストに追加
navigationKeyRepeatInterval(defaultNavigationKeyRepeatInterval)

// SettingsPrivate メンバ変数に追加
int navigationKeyRepeatInterval;
static const int defaultNavigationKeyRepeatInterval = 150;  // ms

// Settings メソッド実装
int Settings::navigationKeyRepeatInterval() const
{
    Q_D(const Settings);
    return d->navigationKeyRepeatInterval;
}

void Settings::setNavigationKeyRepeatInterval(int interval)
{
    Q_D(Settings);
    if (d->navigationKeyRepeatInterval != interval) {
        d->navigationKeyRepeatInterval = interval;
        emit navigationKeyRepeatIntervalChanged();
    }
}

void Settings::resetNavigationKeyRepeatInterval()
{
    setNavigationKeyRepeatInterval(SettingsPrivate::defaultNavigationKeyRepeatInterval);
}
```

**変更箇所2: InputContextPrivate にスロットリング状態を追加**

```cpp
// qvirtualkeyboardinputcontext_p.h - private メンバに追加
#include <QElapsedTimer>

QElapsedTimer navigationKeyThrottleTimer;
qint64 lastNavigationKeyTime;
```

**変更箇所3: filterEvent() のナビゲーションキー処理を修正**

```cpp
// qvirtualkeyboardinputcontext_p.cpp - filterEvent() 内 605-617行を以下に置換
if (Settings::instance()->arrowKeyNavigationEnabled()) {
    if ((key >= Qt::Key_Left && key <= Qt::Key_Down) || key == Qt::Key_Return) {
        if (type == QEvent::KeyPress && platformInputContext->isInputPanelVisible()) {
            // スロットリング: autoRepeat イベントのみ間引く
            if (keyEvent->isAutoRepeat()) {
                if (!navigationKeyThrottleTimer.isValid())
                    navigationKeyThrottleTimer.start();
                qint64 now = navigationKeyThrottleTimer.elapsed();
                int interval = Settings::instance()->navigationKeyRepeatInterval();
                if (now - lastNavigationKeyTime < interval) {
                    return true;  // イベントを消費するがシグナルは発火しない
                }
                lastNavigationKeyTime = now;
            } else {
                // 初回押下はスロットリングせず即座に処理
                lastNavigationKeyTime = 0;
                if (navigationKeyThrottleTimer.isValid())
                    navigationKeyThrottleTimer.restart();
            }
            activeNavigationKeys += key;
            emit navigationKeyPressed(key, keyEvent->isAutoRepeat());
            return true;
        } else if (type == QEvent::KeyRelease && activeNavigationKeys.contains(key)) {
            activeNavigationKeys -= key;
            emit navigationKeyReleased(key, keyEvent->isAutoRepeat());
            return true;
        }
    }
}
```

**動作:**
- 初回のキー押下（isAutoRepeat=false）: 即座に通過 → 応答性を維持
- OSからのキーリピート（isAutoRepeat=true）: 前回発火から150ms以上経過していなければ破棄
- デフォルト150ms → 毎秒約6.7回のカーソル移動（OSデフォルトの30-40回/秒から大幅に削減）

**メリット:**
- 変更箇所が最小限かつ局所的（filterEvent 内の数行のみ）
- 初回押下の応答性を犠牲にしない（isAutoRepeat のみ対象）
- Settings 経由で実行時に間隔を調整可能
- 既存の Settings パターン（hwrTimeout 等）と完全に一貫
- テストが容易（キーリピートイベントをシミュレートするだけ）

**デメリット:**
- C++ レイヤーの変更なのでリビルドが必要

**おすすめ度: ★★★★★**

---

### 案B（推奨）: QML onNavigationKeyPressed ハンドラでの Timer ベーススロットリング

**変更対象ファイル:**
- `src/components/Keyboard.qml` のみ

**設計:**
QML の `Timer` を使い、矢印キーの連続イベントを間引く。

**変更箇所: Keyboard.qml**

```qml
// Keyboard.qml - keyboard ルート要素内（pressAndHoldTimer 付近、例えば行513の前）に追加
Timer {
    id: navigationKeyThrottleTimer
    interval: 150  // ms - 調整可能
    property int pendingKey: -1
    property bool pendingIsAutoRepeat: false
}

// onNavigationKeyPressed ハンドラ（行161）の先頭に追加
function onNavigationKeyPressed(key, isAutoRepeat) {
    // 矢印キーの autoRepeat イベントをスロットリング
    if (isAutoRepeat && (key === Qt.Key_Left || key === Qt.Key_Right ||
                         key === Qt.Key_Up || key === Qt.Key_Down)) {
        if (navigationKeyThrottleTimer.running) {
            return  // 間引き: Timer 動作中は無視
        }
        navigationKeyThrottleTimer.restart()
    }

    // --- 以下、既存の switch 文 ---
    var initialKey
    var direction = wordCandidateView.effectiveLayoutDirection == Qt.LeftToRight ? 1 : -1
    switch (key) {
    // ...（既存コード変更なし）
    }
}
```

**動作:**
- 初回キー押下 → 即座に処理（isAutoRepeat=false なのでスロットリング対象外）
- キーリピート → Timer が動作中なら破棄、停止していれば処理して Timer 再開

**メリット:**
- QML のみの変更でC++リビルド不要
- ホットリロードでインターバル値を即座にテスト可能
- 既存のTimer パターン（pressAndHoldTimer, releaseInaccuracyTimer）と一貫
- 仮想キーボードのナビゲーションだけに的を絞った変更

**デメリット:**
- QML の Timer は精度がイベントループ依存（数ms のばらつきあり）
- 環境変数やAPIでの動的設定変更には追加の接続が必要
- C++ 側の filterEvent は依然として全てのイベントをシグナル発火するため、
  シグナル/スロットのオーバーヘッドは残る

**おすすめ度: ★★★★☆**

---

### 案C: filterEvent() での isAutoRepeat 完全抑止 + 独自タイマーリピート

**変更対象ファイル:**
- `src/virtualkeyboard/qvirtualkeyboardinputcontext_p.h`
- `src/virtualkeyboard/qvirtualkeyboardinputcontext_p.cpp`

**設計:**
OS のキーリピートイベントを全て無視し、自前の QTimer で制御されたリピートを生成する。
`QVirtualKeyboardInputEngine::timerEvent()` の「初回600ms → 以後50ms」パターンを踏襲する。

**変更箇所: InputContextPrivate にタイマーメンバを追加**

```cpp
// qvirtualkeyboardinputcontext_p.h - private メンバに追加
int navigationRepeatTimerId;
int navigationRepeatCount;
int navigationRepeatKey;
```

**変更箇所: filterEvent() を修正 + timerEvent を追加**

```cpp
// filterEvent() 内の矢印キー KeyPress 処理
if (type == QEvent::KeyPress && platformInputContext->isInputPanelVisible()) {
    if (keyEvent->isAutoRepeat()) {
        return true;  // OS のリピートを全て無視
    }
    // 初回押下: 即座に処理 + リピートタイマー開始
    activeNavigationKeys += key;
    emit navigationKeyPressed(key, false);
    navigationRepeatKey = key;
    navigationRepeatCount = 0;
    navigationRepeatTimerId = startTimer(400);  // 初回遅延 400ms
    return true;
}

// KeyRelease 処理
else if (type == QEvent::KeyRelease && activeNavigationKeys.contains(key)) {
    activeNavigationKeys -= key;
    if (navigationRepeatTimerId) {
        killTimer(navigationRepeatTimerId);
        navigationRepeatTimerId = 0;
        navigationRepeatCount = 0;
    }
    emit navigationKeyReleased(key, false);
    return true;
}
```

```cpp
// timerEvent を QVirtualKeyboardInputContextPrivate に追加
// （注: QVirtualKeyboardInputContextPrivate は QObject を継承しているので timerEvent 利用可能）
void QVirtualKeyboardInputContextPrivate::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == navigationRepeatTimerId) {
        emit navigationKeyPressed(navigationRepeatKey, true);
        if (navigationRepeatCount == 0) {
            killTimer(navigationRepeatTimerId);
            navigationRepeatTimerId = startTimer(120);  // リピート間隔 120ms
        }
        navigationRepeatCount++;
    }
}
```

**動作:**
- 初回押下: 即座にカーソル移動
- 400ms 後: リピート開始
- 以降 120ms 間隔でリピート（毎秒約8.3回）
- キーリリース: タイマー停止

**メリット:**
- OS のキーリピート設定に依存しない完全に予測可能な動作
- 初回遅延・リピート間隔を完全制御（既存の virtualKeyPress のパターンと同じ設計思想）
- ユーザーのOS設定が異なる環境でも一貫した動作

**デメリット:**
- 実装量が最も多い（timerEvent のオーバーライド、タイマー管理）
- OS のキーリピート設定をユーザーが変更しても効果がなくなる
- QVirtualKeyboardInputContextPrivate に timerEvent を追加する必要がある

**おすすめ度: ★★★☆☆**

---

### 案D: navigationHighlight アニメーション完了を待機する方式

**変更対象ファイル:**
- `src/components/Keyboard.qml` のみ

**設計:**
`navigationHighlight` のアニメーション（現在200ms）が完了するまで次のナビゲーションを
ブロックする。

**変更箇所: Keyboard.qml**

```qml
// onNavigationKeyPressed ハンドラの先頭に追加
function onNavigationKeyPressed(key, isAutoRepeat) {
    if (isAutoRepeat && (key === Qt.Key_Left || key === Qt.Key_Right ||
                         key === Qt.Key_Up || key === Qt.Key_Down)) {
        // アニメーション中はスキップ
        if (xAnimation.running || yAnimation.running) {
            return
        }
    }
    // --- 以下、既存の switch 文 ---
}
```

**注意:** `xAnimation` と `yAnimation` は `navigationHighlight` 内の Behavior に定義されている
`NumberAnimation` である（Keyboard.qml:637,640）。これらの `id` が既に付与されているため
直接参照可能。

**動作:**
- アニメーション中（最大200ms）はリピートイベントを無視
- アニメーション完了直後の次イベントで次のキーに移動
- 結果として、カーソル移動速度がアニメーション速度に同期

**メリット:**
- 変更量が最小（QML に if 文 3行追加のみ）
- 視覚的に自然（アニメーションと論理状態が同期）
- 追加のタイマーやプロパティが不要

**デメリット:**
- アニメーション duration に完全に依存するため、duration 変更時に連動して動作が変わる
- `noAnimations` が true の場合（duration=0）、スロットリングが効かない
- 視覚的なアニメーション実装の内部状態（running）に依存するのは設計として不安定

**おすすめ度: ★★★☆☆**

---

### 案E: OS キーリピートレート変更（QVK外の対策）

**変更対象:** アプリケーション側またはOS設定

**Linux (X11):**
```bash
xset r rate 400 100   # 初回遅延 400ms、リピート間隔 100ms（デフォルト: 660 25）
```

**Linux (Wayland / libinput):**
```ini
# /etc/libinput/local-overrides.quirks
[Keyboard Repeat]
MatchUdevType=keyboard
AttrKeyboardRepeatDelay=400
AttrKeyboardRepeatRate=100
```

**Qt アプリケーション内:**
Qt にはキーリピートレートを制御する公式APIは存在しない。
ただし、`QCoreApplication::installEventFilter()` でグローバルイベントフィルタを設置し、
アプリケーション全体で `isAutoRepeat` イベントを間引くことは可能。

**メリット:**
- QVK のコード変更が不要
- OS 全体で一貫した動作

**デメリット:**
- QVK 以外の全てのアプリケーションにも影響
- 組み込み環境では OS 設定変更が困難な場合がある
- アプリケーション配布時にユーザーの OS 設定を強制できない

**おすすめ度: ★★☆☆☆** — QVK 内での対策が困難な場合の代替手段

---

## 4. 推奨組み合わせ

| シナリオ | 推奨案 |
|---|---|
| 最小変更で確実に解決したい | **案A** (filterEvent C++ スロットリング) |
| C++ リビルドを避けたい | **案B** (QML Timer) |
| OS依存を完全排除したい | **案C** (独自タイマーリピート) |
| とにかく速く試したい | **案D** (アニメーション待機) |

**最も推奨する組み合わせ: 案A 単体**

案A は filterEvent 内の isAutoRepeat 判定により初回押下の応答性を維持しつつ、
Settings 経由で実行時に調整可能な間引き機構を提供する。
変更量も少なく、既存の Settings パターン（`hwrTimeoutForAlphabetic` 等）と
完全に一貫した設計である。

---

## 5. テスト方針

### 手動テスト

1. 仮想キーボードを表示した状態で物理キーボードの矢印キーを長押し
2. カーソルの移動速度が設定間隔に従って制限されることを確認
3. 矢印キーの単押し（tap）が即座に反応することを確認
4. 4方向全ての矢印キーで動作確認
5. Return キーの長押し動作に影響がないことを確認
6. `navigationKeyRepeatInterval` の値を動的に変更して反映されることを確認

### 自動テスト

```cpp
// テストの擬似コード
void tst_InputContext::navigationKeyThrottling()
{
    // 仮想キーボードを表示
    showInputPanel();

    // キーリピートイベントを高速に送信
    for (int i = 0; i < 20; i++) {
        QKeyEvent press(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier,
                        QString(), true /* autoRepeat */);
        QGuiApplication::sendEvent(focusObject, &press);
    }

    // 間引きにより、実際のカーソル移動回数が20未満であることを確認
    QVERIFY(actualNavigationCount < 20);
}
```

---

## 6. 設定値のチューニングガイド

| 設定値 (ms) | 移動速度 (回/秒) | 体感 |
|---|---|---|
| 50 | ~20 | 非常に速い（ほぼ間引きなし） |
| 100 | ~10 | 速め |
| **150** | **~6.7** | **推奨デフォルト** |
| 200 | ~5 | navigationHighlight アニメーション同期 |
| 300 | ~3.3 | ゆっくり |

推奨デフォルト値 **150ms** の根拠:
- navigationHighlight のアニメーション duration が 200ms であるため、
  150ms 間隔ではアニメーションが概ね追従できる
- 一般的なキーリピート体験（多くのOSのデフォルトは25-33回/秒）に対して
  十分な間引き効果がある
- ユーザーが「カーソルが飛ぶ」と感じないギリギリの速さ
