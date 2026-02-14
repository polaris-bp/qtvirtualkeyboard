# 実装ガイド: 物理キーボード ナビゲーションキーイベント間引き処理

## 概要

物理キーボードの矢印キーを長押ししたとき、OS が生成するキーリピートイベントが
仮想キーボードのナビゲーションカーソルに高速に伝わり、カーソルが飛んでしまう。
本ガイドでは、この問題の原因分析と推奨する2つの実装案を示す。

---

## 1. 現状のイベントフロー

物理キーボードの矢印キーが押下されると、以下のパスで仮想キーボード上のナビゲーション
カーソルが移動する。

```
物理キーボード押下
    |
    v
OS/ウィンドウシステムが QKeyEvent を生成
 (長押し時: isAutoRepeat=true のイベントをOS側のリピートレートで連続生成)
    |
    v
PlatformInputContext::eventFilter()          [platforminputcontext.cpp:192]
  条件: event != m_filterEvent && object == m_focusObject && m_inputContext
    |
    v
QVirtualKeyboardInputContextPrivate::filterEvent()
                                             [qvirtualkeyboardinputcontext_p.cpp:587]
    |
    +--[矢印キー + パネル表示中 + KeyPress]
    |    activeNavigationKeys += key                  (行608)
    |    emit navigationKeyPressed(key, isAutoRepeat) (行609)
    |    return true  -- イベント消費                   (行610)
    |
    +--[矢印キー + activeSetに含む + KeyRelease]
    |    activeNavigationKeys -= key                    (行612)
    |    emit navigationKeyReleased(key, isAutoRepeat)  (行613)
    |    return true  -- イベント消費                     (行614)
    |
    +--[その他のキー]
         return false -- イベントをアプリに通過           (行643)
    |
    v
Keyboard.qml: onNavigationKeyPressed ハンドラ  [Keyboard.qml:161]
    |
    +-- Qt.Key_Left  -> navigateToNextKey(-1*dir, 0, false)
    +-- Qt.Key_Right -> navigateToNextKey( 1*dir, 0, false)
    +-- Qt.Key_Up    -> navigateToNextKey(0, -1, ...)
    +-- Qt.Key_Down  -> navigateToNextKey(0,  1, ...)
    +-- Qt.Key_Return -> press()/release() activeKey
    |
    v
navigateToNextKey() -> nextKeyInNavigation()   [Keyboard.qml:1095]
  navigationCursor を更新
    |
    v
navigationHighlight (Loader) が視覚的に追従     [Keyboard.qml:625-648]
  Behavior on x/y: NumberAnimation { duration: 200ms; easing: OutCubic }
```

### 問題点

| # | 内容 | 該当箇所 |
|---|------|----------|
| 1 | **スロットリング機構が存在しない** — filterEvent は受け取った全てのキーイベントをそのまま `navigationKeyPressed` シグナルとして発火する | `filterEvent()` 行605-617 |
| 2 | **矢印キーの isAutoRepeat がチェックされていない** — Return キーのみ isAutoRepeat のガードがある | `Keyboard.qml` 行389,395,401,418 |
| 3 | **アニメーションと論理状態の不一致** — `navigationHighlight` の移動アニメーションは200msだが、`navigateToNextKey()` は即座に実行される。高速リピートではアニメーション完了前に次の位置へ飛ぶ | `Keyboard.qml` 行636-641 |

---

## 2. 推奨実装案

### 案A (推奨): filterEvent() での C++ レベルスロットリング

C++ の `filterEvent()` 内で `QElapsedTimer` を用いて `isAutoRepeat` イベントを間引く。
初回押下は即座に通過させ、リピートのみ時間制御する。

#### 変更対象ファイル

| ファイル | 変更内容 |
|----------|----------|
| `src/virtualkeyboard/settings_p.h` | `navigationKeyRepeatInterval` プロパティ宣言 |
| `src/virtualkeyboard/settings.cpp` | 同プロパティ実装 (get/set/reset/signal) |
| `src/virtualkeyboard/qvirtualkeyboardinputcontext_p.h` | スロットリング用メンバ変数 |
| `src/virtualkeyboard/qvirtualkeyboardinputcontext_p.cpp` | `filterEvent()` 修正 |

#### 変更箇所 1: Settings にプロパティ追加

既存の `arrowKeyNavigationEnabled` (settings_p.h:118-120) と同じパターンに従う。

**settings_p.h** — public セクション (`arrowKeyNavigationEnabled` の後):

```cpp
int navigationKeyRepeatInterval() const;
void setNavigationKeyRepeatInterval(int interval);
void resetNavigationKeyRepeatInterval();
```

**settings_p.h** — signals セクション (`arrowKeyNavigationEnabledChanged` の後):

```cpp
void navigationKeyRepeatIntervalChanged();
```

**settings.cpp** — SettingsPrivate クラス:

```cpp
// メンバ変数に追加 (arrowKeyNavigationEnabled の後)
int navigationKeyRepeatInterval;

// デフォルト値定数に追加
static const int defaultNavigationKeyRepeatInterval = 150;  // ms

// コンストラクタ初期化リストに追加
navigationKeyRepeatInterval(defaultNavigationKeyRepeatInterval)
```

**settings.cpp** — Settings メソッド実装 (`resetArrowKeyNavigationEnabled` の後):

```cpp
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

#### 変更箇所 2: InputContextPrivate にスロットリング状態を追加

**qvirtualkeyboardinputcontext_p.h** — private メンバ (`activeNavigationKeys` 付近):

```cpp
#include <QElapsedTimer>

QElapsedTimer navigationKeyThrottleTimer;
qint64 lastNavigationKeyTime = 0;
```

#### 変更箇所 3: filterEvent() の修正

**qvirtualkeyboardinputcontext_p.cpp** — 行605-617 を以下に置換:

```cpp
if (Settings::instance()->arrowKeyNavigationEnabled()) {
    if ((key >= Qt::Key_Left && key <= Qt::Key_Down) || key == Qt::Key_Return) {
        if (type == QEvent::KeyPress && platformInputContext->isInputPanelVisible()) {
            // autoRepeat イベントのみ間引く
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
                // 初回押下: スロットリングせず即座に処理
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

#### 動作イメージ

```
時間 (ms)   0     30    60    90   120   150   180   210   240
            |     |     |     |     |     |     |     |     |
OS repeat:  R     R     R     R     R     R     R     R     R
            ^                             ^                 ^
            通過                          通過              通過
            (初回)                        (150ms経過)       (150ms経過)
```

- 初回押下 (isAutoRepeat=false): 即座に通過 — 応答性を維持
- リピート (isAutoRepeat=true): 前回発火から 150ms 未満なら破棄
- デフォルト 150ms = 毎秒約 6.7 回 (OS デフォルトの 30-40 回/秒から大幅に削減)

#### 利点と欠点

| 利点 | 欠点 |
|------|------|
| 変更箇所が局所的 (filterEvent 内の数行 + Settings プロパティ) | C++ 変更のためリビルドが必要 |
| 初回押下の応答性を犠牲にしない (isAutoRepeat のみ対象) | |
| Settings 経由で実行時に間隔を調整可能 | |
| 既存の `arrowKeyNavigationEnabled` と同じ設計パターン | |
| QML 側を一切変更しない | |

---

### 案B (代替): QML onNavigationKeyPressed での Timer スロットリング

C++ リビルドを避けたい場合の代替案。QML の `Timer` でリピートイベントを間引く。

#### 変更対象ファイル

| ファイル | 変更内容 |
|----------|----------|
| `src/components/Keyboard.qml` | Timer 追加 + ハンドラ修正 |

#### 変更箇所: Keyboard.qml

**Timer 追加** (行513 `pressAndHoldTimer` 付近):

```qml
Timer {
    id: navigationKeyThrottleTimer
    interval: 150  // ms
}
```

**onNavigationKeyPressed ハンドラ** (行161) の先頭にガードを追加:

```qml
function onNavigationKeyPressed(key, isAutoRepeat) {
    // 矢印キーの autoRepeat をスロットリング
    if (isAutoRepeat && (key === Qt.Key_Left || key === Qt.Key_Right ||
                         key === Qt.Key_Up || key === Qt.Key_Down)) {
        if (navigationKeyThrottleTimer.running) {
            return  // Timer 動作中は無視
        }
        navigationKeyThrottleTimer.restart()
    }

    // --- 以下、既存の switch 文（変更なし）---
    var initialKey
    var direction = wordCandidateView.effectiveLayoutDirection == Qt.LeftToRight ? 1 : -1
    switch (key) {
    // ...
    }
}
```

#### 利点と欠点

| 利点 | 欠点 |
|------|------|
| QML のみの変更で C++ リビルド不要 | Timer 精度がイベントループ依存 (数ms のばらつき) |
| ホットリロードでインターバルを即座にテスト可能 | filterEvent は依然として全イベントをシグナル発火するため、シグナル/スロットのオーバーヘッドが残る |
| 既存の Timer パターン (`pressAndHoldTimer`) と一貫 | 動的設定変更には追加の接続が必要 |

---

### 案の比較と選定

| 観点 | 案A (C++ filterEvent) | 案B (QML Timer) |
|------|----------------------|-----------------|
| 間引き精度 | 高い (QElapsedTimer) | イベントループ依存 |
| 不要なシグナル発火 | なし (発火前に破棄) | あり (QML で受けてから破棄) |
| 設定の動的変更 | Settings 経由で容易 | Timer.interval の直接変更 |
| 変更範囲 | C++ 4ファイル | QML 1ファイル |
| リビルド | 必要 | 不要 |

**推奨: 案A 単体**

案A は filterEvent 内で不要なイベントを発火前に破棄するため、
QML 側への影響がゼロで、最もクリーンな解決策となる。
Settings プロパティの追加パターンは `arrowKeyNavigationEnabled` と同一であり、
コードベースとの一貫性も高い。

---

## 3. テスト方針

### 手動テスト

| # | テスト内容 | 期待結果 |
|---|-----------|----------|
| 1 | 仮想キーボード表示中に矢印キーを長押し | カーソルが設定間隔に従って移動 (飛ばない) |
| 2 | 矢印キーを単押し (tap) | 即座にカーソルが1つ移動 |
| 3 | 4方向全てで長押し | 全方向でスロットリングが有効 |
| 4 | Return キーの長押し | 既存動作に影響なし (Return は行389のガードで保護済み) |
| 5 | `navigationKeyRepeatInterval` を動的に変更 | 変更後の値で即座にスロットリングが反映 |
| 6 | `navigationKeyRepeatInterval` を 0 に設定 | スロットリング無効 (全イベント通過) |

### 自動テスト

```cpp
void tst_InputContext::navigationKeyThrottling()
{
    // Setup
    showInputPanel();
    QSignalSpy spy(inputContext->priv(),
                   &QVirtualKeyboardInputContextPrivate::navigationKeyPressed);

    // 間隔を 100ms に設定
    Settings::instance()->setNavigationKeyRepeatInterval(100);

    // autoRepeat イベントを間隔 0 で 20 回送信
    for (int i = 0; i < 20; i++) {
        QKeyEvent press(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier,
                        QString(), true /* autoRepeat */);
        QGuiApplication::sendEvent(focusObject, &press);
    }

    // 間引きにより発火回数が 20 未満であることを確認
    QVERIFY(spy.count() < 20);

    // 初回押下 (非 autoRepeat) は常に通過することを確認
    spy.clear();
    QKeyEvent initialPress(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier,
                           QString(), false /* not autoRepeat */);
    QGuiApplication::sendEvent(focusObject, &initialPress);
    QCOMPARE(spy.count(), 1);
}

void tst_InputContext::navigationKeyThrottlingDisabled()
{
    showInputPanel();
    QSignalSpy spy(inputContext->priv(),
                   &QVirtualKeyboardInputContextPrivate::navigationKeyPressed);

    // 間隔 0 でスロットリング無効化
    Settings::instance()->setNavigationKeyRepeatInterval(0);

    for (int i = 0; i < 10; i++) {
        QKeyEvent press(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier,
                        QString(), true);
        QGuiApplication::sendEvent(focusObject, &press);
    }

    // 全てのイベントが通過
    QCOMPARE(spy.count(), 10);
}
```

---

## 4. 設定値チューニングガイド

| 設定値 (ms) | 移動速度 (回/秒) | 体感 |
|-------------|------------------|------|
| 0 | 制限なし | スロットリング無効 |
| 50 | ~20 | 非常に速い |
| 100 | ~10 | 速め |
| **150** | **~6.7** | **推奨デフォルト** |
| 200 | ~5 | アニメーション duration と一致 |
| 300 | ~3.3 | ゆっくり |

### デフォルト値 150ms の根拠

1. **アニメーションとの整合** — `navigationHighlight` の移動アニメーション duration は
   200ms (Keyboard.qml:627)。150ms 間隔ではアニメーションが概ね追従できる
2. **十分な間引き効果** — 多くの OS のデフォルトキーリピートは 25-33 回/秒。
   150ms 間隔 (~6.7 回/秒) は約 75-80% のイベントを破棄する
3. **操作性の確保** — カーソルが「飛ぶ」と感じない速さを維持しつつ、
   キーボード全体の横断 (通常 10-12 キー) が約 1.5 秒で完了する

---

## 付録: 他の検討済みアプローチ

以下のアプローチも検討したが、案A・B と比較して推奨度が低いため採用を見送った。

| アプローチ | 概要 | 不採用理由 |
|-----------|------|-----------|
| OS キーリピート全抑止 + 独自タイマー | `isAutoRepeat` を全て無視し、自前の `QTimer` でリピートを再生成 | 実装量が多い。OS のキーリピート設定がユーザーに反映されなくなる |
| アニメーション完了待機 | `xAnimation.running` / `yAnimation.running` をチェックし、アニメーション中はリピートを無視 | アニメーション内部状態への依存が不安定。`noAnimations=true` 時にスロットリングが無効になる |
| OS キーリピートレート変更 | `xset r rate` や libinput quirks でシステム全体のリピートを調整 | QVK 以外の全アプリに影響。組み込み環境での変更が困難 |
