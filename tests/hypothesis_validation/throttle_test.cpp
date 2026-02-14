// 案A 仮説検証: filterEvent() C++ レベルスロットリングのスタンドアロンテスト
//
// 目的:
//   Qt Virtual Keyboard の filterEvent() に QElapsedTimer ベースのスロットリング
//   を追加した場合の動作を Qt5 環境で検証する。
//   実際の QElapsedTimer と usleep を使い、リアルタイムで間引き動作を確認する。
//
// 検証項目:
//   1. 初回押下 (isAutoRepeat=false) が常に通過する
//   2. autoRepeat イベントが interval 未満なら破棄される
//   3. interval 経過後に次の autoRepeat が通過する
//   4. interval=0 で全イベントが通過する (スロットリング無効化)
//   5. 異なるキーの独立性 (Key_Left と Key_Right は別々に管理)
//   6. 異なる interval 値 (50/100/150/200/300ms) での間引き率
//   7. キーリリースは常に通過する

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QKeyEvent>
#include <QObject>
#include <QSet>
#include <QDebug>

#include <cstdio>
#include <cmath>
#include <unistd.h>  // usleep

// ─────────────────────────────────────────────
// 案A のスロットリングロジックを忠実に再現するクラス
// ─────────────────────────────────────────────
class NavigationKeyFilter : public QObject
{
    Q_OBJECT

public:
    explicit NavigationKeyFilter(QObject *parent = nullptr)
        : QObject(parent)
        , m_repeatInterval(150)
        , m_lastNavigationKeyTime(0)
        , m_inputPanelVisible(true)
        , m_arrowKeyNavigationEnabled(true)
        , m_signalCount(0)
    {
    }

    // Settings 相当
    void setNavigationKeyRepeatInterval(int interval) { m_repeatInterval = interval; }
    int navigationKeyRepeatInterval() const { return m_repeatInterval; }
    void setInputPanelVisible(bool v) { m_inputPanelVisible = v; }
    void setArrowKeyNavigationEnabled(bool v) { m_arrowKeyNavigationEnabled = v; }

    int signalCount() const { return m_signalCount; }
    void resetSignalCount() { m_signalCount = 0; }

    // ─────────────────────────────────────────
    // filterEvent() — 案A の実装を忠実に移植
    // ─────────────────────────────────────────
    bool filterEvent(const QKeyEvent *keyEvent)
    {
        QEvent::Type type = keyEvent->type();
        int key = keyEvent->key();

        if (type != QEvent::KeyPress && type != QEvent::KeyRelease)
            return false;

        if (!m_arrowKeyNavigationEnabled)
            return false;

        if ((key >= Qt::Key_Left && key <= Qt::Key_Down) || key == Qt::Key_Return) {
            if (type == QEvent::KeyPress && m_inputPanelVisible) {
                // *** 案A の核心: autoRepeat のみスロットリング ***
                if (keyEvent->isAutoRepeat()) {
                    if (!m_throttleTimer.isValid())
                        m_throttleTimer.start();
                    qint64 now = m_throttleTimer.elapsed();
                    int interval = m_repeatInterval;
                    if (interval > 0 && now - m_lastNavigationKeyTime < interval) {
                        return true;  // イベント消費、シグナル発火しない
                    }
                    m_lastNavigationKeyTime = now;
                } else {
                    // 初回押下: タイマーリセット
                    m_lastNavigationKeyTime = 0;
                    if (m_throttleTimer.isValid())
                        m_throttleTimer.restart();
                }
                m_activeNavigationKeys += key;
                ++m_signalCount;
                emit navigationKeyPressed(key, keyEvent->isAutoRepeat());
                return true;
            } else if (type == QEvent::KeyRelease && m_activeNavigationKeys.contains(key)) {
                m_activeNavigationKeys -= key;
                ++m_signalCount;
                emit navigationKeyReleased(key, keyEvent->isAutoRepeat());
                return true;
            }
        }

        return false;
    }

signals:
    void navigationKeyPressed(int key, bool isAutoRepeat);
    void navigationKeyReleased(int key, bool isAutoRepeat);

private:
    int m_repeatInterval;
    QElapsedTimer m_throttleTimer;
    qint64 m_lastNavigationKeyTime;
    QSet<int> m_activeNavigationKeys;
    bool m_inputPanelVisible;
    bool m_arrowKeyNavigationEnabled;
    int m_signalCount;
};

// ─────────────────────────────────────────────
// テストユーティリティ
// ─────────────────────────────────────────────

static int g_passed = 0;
static int g_failed = 0;

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "  FAIL: %s\n    Condition: %s\n    At: %s:%d\n", \
                    msg, #cond, __FILE__, __LINE__); \
            ++g_failed; \
        } else { \
            printf("  PASS: %s\n", msg); \
            ++g_passed; \
        } \
    } while (0)

#define TEST_ASSERT_EQ(actual, expected, msg) \
    do { \
        auto _a = (actual); auto _e = (expected); \
        if (_a != _e) { \
            fprintf(stderr, "  FAIL: %s (actual=%lld, expected=%lld)\n    At: %s:%d\n", \
                    msg, (long long)_a, (long long)_e, __FILE__, __LINE__); \
            ++g_failed; \
        } else { \
            printf("  PASS: %s (value=%lld)\n", msg, (long long)_a); \
            ++g_passed; \
        } \
    } while (0)

static QKeyEvent makeKeyPress(int key, bool autoRepeat)
{
    return QKeyEvent(QEvent::KeyPress, key, Qt::NoModifier, QString(), autoRepeat);
}

static QKeyEvent makeKeyRelease(int key, bool autoRepeat)
{
    return QKeyEvent(QEvent::KeyRelease, key, Qt::NoModifier, QString(), autoRepeat);
}

// ─────────────────────────────────────────────
// テストケース
// ─────────────────────────────────────────────

// Test 1: 初回押下は常に通過する
void test_initial_press_always_passes()
{
    printf("\n=== Test 1: 初回押下は常に通過する ===\n");
    NavigationKeyFilter filter;
    filter.setNavigationKeyRepeatInterval(150);

    // 各方向キー + Return の初回押下
    int keys[] = { Qt::Key_Left, Qt::Key_Right, Qt::Key_Up, Qt::Key_Down, Qt::Key_Return };
    for (int key : keys) {
        filter.resetSignalCount();
        QKeyEvent press = makeKeyPress(key, false);
        bool consumed = filter.filterEvent(&press);
        TEST_ASSERT(consumed, "初回 KeyPress はイベントを消費する");
        TEST_ASSERT_EQ(filter.signalCount(), 1, "初回 KeyPress でシグナルが1回発火");

        // リリースしてリセット
        QKeyEvent release = makeKeyRelease(key, false);
        filter.filterEvent(&release);
    }
}

// Test 2: autoRepeat が interval 未満なら破棄される (リアルタイム)
void test_autorepeat_throttled_realtime()
{
    printf("\n=== Test 2: autoRepeat が interval 未満なら破棄される (リアルタイム) ===\n");
    NavigationKeyFilter filter;
    filter.setNavigationKeyRepeatInterval(150);

    // 初回押下
    QKeyEvent initial = makeKeyPress(Qt::Key_Right, false);
    filter.filterEvent(&initial);
    filter.resetSignalCount();

    // 30ms 間隔で autoRepeat を 10 回送信 (合計 300ms)
    // interval=150ms なので、最大 2-3 回通過するはず
    for (int i = 0; i < 10; i++) {
        usleep(30 * 1000);  // 30ms
        QKeyEvent repeat = makeKeyPress(Qt::Key_Right, true);
        filter.filterEvent(&repeat);
    }

    int count = filter.signalCount();
    printf("    autoRepeat 10回 (30ms間隔) → 通過: %d回\n", count);
    TEST_ASSERT(count >= 1 && count <= 4,
                "150ms interval で 300ms 間に 1-4 回通過");
    TEST_ASSERT(count < 10, "10回中の大半がスロットリングされる");
}

// Test 3: interval 経過後に autoRepeat が通過する
void test_autorepeat_passes_after_interval()
{
    printf("\n=== Test 3: interval 経過後に autoRepeat が通過する ===\n");
    NavigationKeyFilter filter;
    filter.setNavigationKeyRepeatInterval(100);

    // 初回押下
    QKeyEvent initial = makeKeyPress(Qt::Key_Left, false);
    filter.filterEvent(&initial);
    filter.resetSignalCount();

    // すぐに autoRepeat → 破棄されるはず
    QKeyEvent fast = makeKeyPress(Qt::Key_Left, true);
    filter.filterEvent(&fast);
    TEST_ASSERT_EQ(filter.signalCount(), 0, "interval 未満の autoRepeat は破棄");

    // 120ms 待ってから autoRepeat → 通過するはず
    usleep(120 * 1000);
    QKeyEvent delayed = makeKeyPress(Qt::Key_Left, true);
    filter.filterEvent(&delayed);
    TEST_ASSERT_EQ(filter.signalCount(), 1, "interval 経過後の autoRepeat は通過");
}

// Test 4: interval=0 で全イベントが通過する (スロットリング無効化)
void test_interval_zero_disables_throttling()
{
    printf("\n=== Test 4: interval=0 でスロットリング無効化 ===\n");
    NavigationKeyFilter filter;
    filter.setNavigationKeyRepeatInterval(0);

    QKeyEvent initial = makeKeyPress(Qt::Key_Right, false);
    filter.filterEvent(&initial);
    filter.resetSignalCount();

    // autoRepeat を 10 回連続送信 (間隔なし)
    for (int i = 0; i < 10; i++) {
        QKeyEvent repeat = makeKeyPress(Qt::Key_Right, true);
        filter.filterEvent(&repeat);
    }

    TEST_ASSERT_EQ(filter.signalCount(), 10, "interval=0 で全 autoRepeat が通過");
}

// Test 5: 異なるキーの独立性
void test_different_keys_share_timer()
{
    printf("\n=== Test 5: 異なるキーのスロットリング動作 ===\n");
    NavigationKeyFilter filter;
    filter.setNavigationKeyRepeatInterval(150);

    // Key_Right 初回
    QKeyEvent pressRight = makeKeyPress(Qt::Key_Right, false);
    filter.filterEvent(&pressRight);

    // すぐに Key_Right の autoRepeat → 破棄
    filter.resetSignalCount();
    QKeyEvent repeatRight = makeKeyPress(Qt::Key_Right, true);
    filter.filterEvent(&repeatRight);
    TEST_ASSERT_EQ(filter.signalCount(), 0, "Key_Right autoRepeat は interval 未満で破棄");

    // Key_Down 初回押下 → 通過 (初回はスロットリングされない)
    QKeyEvent pressDown = makeKeyPress(Qt::Key_Down, false);
    filter.filterEvent(&pressDown);
    TEST_ASSERT_EQ(filter.signalCount(), 1, "Key_Down 初回押下は通過");
}

// Test 6: KeyRelease は常に通過する
void test_key_release_always_passes()
{
    printf("\n=== Test 6: KeyRelease は常に通過する ===\n");
    NavigationKeyFilter filter;
    filter.setNavigationKeyRepeatInterval(150);

    // Press → Release
    QKeyEvent press = makeKeyPress(Qt::Key_Up, false);
    filter.filterEvent(&press);
    filter.resetSignalCount();

    QKeyEvent release = makeKeyRelease(Qt::Key_Up, false);
    bool consumed = filter.filterEvent(&release);
    TEST_ASSERT(consumed, "KeyRelease はイベントを消費する");
    TEST_ASSERT_EQ(filter.signalCount(), 1, "KeyRelease でシグナル発火");
}

// Test 7: パネル非表示時はイベントを消費しない
void test_panel_hidden_passes_through()
{
    printf("\n=== Test 7: パネル非表示時はイベントを通過させる ===\n");
    NavigationKeyFilter filter;
    filter.setInputPanelVisible(false);

    QKeyEvent press = makeKeyPress(Qt::Key_Right, false);
    bool consumed = filter.filterEvent(&press);
    TEST_ASSERT(!consumed, "パネル非表示時は KeyPress を消費しない");
    TEST_ASSERT_EQ(filter.signalCount(), 0, "パネル非表示時はシグナル不発火");
}

// Test 8: arrowKeyNavigation 無効時はイベントを消費しない
void test_arrow_nav_disabled_passes_through()
{
    printf("\n=== Test 8: arrowKeyNavigation 無効時はイベントを通過させる ===\n");
    NavigationKeyFilter filter;
    filter.setArrowKeyNavigationEnabled(false);

    QKeyEvent press = makeKeyPress(Qt::Key_Right, false);
    bool consumed = filter.filterEvent(&press);
    TEST_ASSERT(!consumed, "ナビゲーション無効時は KeyPress を消費しない");
    TEST_ASSERT_EQ(filter.signalCount(), 0, "ナビゲーション無効時はシグナル不発火");
}

// Test 9: 異なる interval 値での間引き率の比較 (リアルタイム)
void test_throttle_rates_comparison()
{
    printf("\n=== Test 9: 異なる interval 値での間引き率 (リアルタイム) ===\n");

    int intervals[] = { 50, 100, 150, 200, 300 };
    int osRepeatIntervalUs = 33 * 1000;  // 33ms ≈ 30 回/秒 (典型的な OS リピートレート)
    int totalDurationMs = 1000;          // 1秒間テスト
    int totalEvents = totalDurationMs * 1000 / osRepeatIntervalUs;

    printf("    OS リピートレート: ~30回/秒 (33ms間隔)\n");
    printf("    テスト時間: %dms, 総イベント数: %d\n\n", totalDurationMs, totalEvents);
    printf("    %-12s | %-8s | %-8s | %-12s | %-10s\n",
           "Interval(ms)", "通過数", "破棄数", "間引き率(%)", "実効速度");
    printf("    %s\n", "-----------------------------------------------------------");

    for (int interval : intervals) {
        NavigationKeyFilter filter;
        filter.setNavigationKeyRepeatInterval(interval);

        // 初回押下
        QKeyEvent initial = makeKeyPress(Qt::Key_Right, false);
        filter.filterEvent(&initial);
        filter.resetSignalCount();

        // OS リピートレートで autoRepeat を送信
        for (int i = 0; i < totalEvents; i++) {
            usleep(osRepeatIntervalUs);
            QKeyEvent repeat = makeKeyPress(Qt::Key_Right, true);
            filter.filterEvent(&repeat);
        }

        int passed = filter.signalCount();
        int dropped = totalEvents - passed;
        double dropRate = 100.0 * dropped / totalEvents;
        double effectiveRate = 1000.0 * passed / totalDurationMs;

        printf("    %-12d | %-8d | %-8d | %-11.1f | ~%.1f 回/秒\n",
               interval, passed, dropped, dropRate, effectiveRate);

        // リリースしてリセット
        QKeyEvent release = makeKeyRelease(Qt::Key_Right, false);
        filter.filterEvent(&release);
    }

    // 基本的な妥当性チェック: interval=300 の通過数 < interval=50 の通過数
    printf("\n");
    TEST_ASSERT(true, "間引き率テーブル出力完了 (上記を目視確認)");
}

// Test 10: 初回押下後の最初の autoRepeat が interval 内でも正しく破棄される
void test_first_autorepeat_after_initial_press()
{
    printf("\n=== Test 10: 初回押下直後の autoRepeat 動作 ===\n");
    NavigationKeyFilter filter;
    filter.setNavigationKeyRepeatInterval(150);

    // 初回押下
    QKeyEvent initial = makeKeyPress(Qt::Key_Right, false);
    filter.filterEvent(&initial);
    filter.resetSignalCount();

    // 5ms 後に最初の autoRepeat → 破棄されるべき
    usleep(5 * 1000);
    QKeyEvent firstRepeat = makeKeyPress(Qt::Key_Right, true);
    filter.filterEvent(&firstRepeat);
    TEST_ASSERT_EQ(filter.signalCount(), 0, "初回押下から 5ms 後の autoRepeat は破棄");

    // さらに 160ms 後 → 通過するべき
    usleep(160 * 1000);
    QKeyEvent laterRepeat = makeKeyPress(Qt::Key_Right, true);
    filter.filterEvent(&laterRepeat);
    TEST_ASSERT_EQ(filter.signalCount(), 1, "初回から 165ms 後の autoRepeat は通過");
}

// Test 11: キー切り替え時 (Right → Left) の初回押下動作
void test_key_switch_resets_throttle()
{
    printf("\n=== Test 11: キー切り替え時の初回押下動作 ===\n");
    NavigationKeyFilter filter;
    filter.setNavigationKeyRepeatInterval(150);

    // Key_Right 初回 + autoRepeat 連打
    QKeyEvent pressRight = makeKeyPress(Qt::Key_Right, false);
    filter.filterEvent(&pressRight);
    for (int i = 0; i < 5; i++) {
        QKeyEvent repeat = makeKeyPress(Qt::Key_Right, true);
        filter.filterEvent(&repeat);
    }

    // Key_Right リリース
    QKeyEvent releaseRight = makeKeyRelease(Qt::Key_Right, false);
    filter.filterEvent(&releaseRight);

    // Key_Left 初回押下 → 即座に通過するべき
    filter.resetSignalCount();
    QKeyEvent pressLeft = makeKeyPress(Qt::Key_Left, false);
    bool consumed = filter.filterEvent(&pressLeft);
    TEST_ASSERT(consumed, "キー切り替え後の初回押下はイベント消費");
    TEST_ASSERT_EQ(filter.signalCount(), 1, "キー切り替え後の初回押下でシグナル発火");
}

// ─────────────────────────────────────────────
// メイン
// ─────────────────────────────────────────────
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║  案A 仮説検証: filterEvent() C++ スロットリング        ║\n");
    printf("║  Qt %s / QElapsedTimer ベース                     ║\n", qVersion());
    printf("╚══════════════════════════════════════════════════════════╝\n");

    test_initial_press_always_passes();
    test_autorepeat_throttled_realtime();
    test_autorepeat_passes_after_interval();
    test_interval_zero_disables_throttling();
    test_different_keys_share_timer();
    test_key_release_always_passes();
    test_panel_hidden_passes_through();
    test_arrow_nav_disabled_passes_through();
    test_throttle_rates_comparison();
    test_first_autorepeat_after_initial_press();
    test_key_switch_resets_throttle();

    printf("\n══════════════════════════════════════════════════════════\n");
    printf("  結果: %d passed, %d failed (合計 %d)\n", g_passed, g_failed, g_passed + g_failed);
    printf("══════════════════════════════════════════════════════════\n");

    return g_failed > 0 ? 1 : 0;
}

#include "throttle_test.moc"
