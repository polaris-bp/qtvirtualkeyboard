# TextInput フォントフォールバックによるテキスト位置ずれの調査報告

## この文書の目的

Qt の TextInput / TextField で、日本語などの非ラテン文字を入力したときに
テキストの表示位置がずれる（ジャンプする）現象について、
Qt のソースコードを逐一検証した結果をまとめます。

**切り分けの結論: これは Qt 本体（qtbase + qtdeclarative）の問題であり、
qtvirtualkeyboard 側には原因がありません。**

---

## 1. 現象の説明（誰でもわかるように）

### 何が起きるか

テキスト入力欄に英語を入力しているとき、途中で日本語を入力すると、
**テキスト全体の位置が一瞬ずれる（上下にジャンプする）** ことがあります。

例：
```
"hello" と入力 → テキストは安定して表示される
"helloあ" と入力 → テキスト全体が数ピクセル上に動く
```

### なぜ起きるか（たとえ話）

テキスト入力欄は、文字を表示するための「行の高さ」を持っています。

- 英語フォント（例：Roboto）の行の高さが **20px** だとします
- 日本語フォント（例：Noto CJK）の行の高さが **24px** だとします

英語だけを入力しているときは、行の高さは 20px です。
日本語を1文字入力すると、その文字が切れないように、行の高さが **24px** に拡大されます。

この **行の高さの変化（20px → 24px）** がテキストの表示位置に影響を与え、
画面上でテキストがずれて見える原因です。

### どのくらいずれるか

**すべての verticalAlignment 設定で発生します。**

| 設定 | 発生条件 | ずれ量 |
|------|---------|--------|
| AlignTop（デフォルト） | 固定高さの TextField でフォールバック後の行高さが利用可能高さを超える場合 | contentSize はみ出し量と同じ |
| VCenter | 常に発生 | 行高さの変化量 ÷ 2 |
| AlignBottom | 常に発生 | 行高さの変化量 |

一般的な TextField はプライマリフォントの行高さに合わせたサイズなので、
フォールバックフォントの行高さがそれを超えると、**どの設定でも発生します**。

---

## 2. 技術的な原因（ソースコード検証済み）

### 2-1. フォントフォールバックとは

Qt は、指定されたフォント（プライマリフォント）で表示できない文字があると、
別のフォント（フォールバックフォント）を自動的に使います。
例えば、Roboto で「あ」を表示できないので、Noto CJK JP が使われます。

これ自体は正常で必要な機能です。

### 2-2. 行の高さが変わる仕組み（検証済み: 事実）

Qt のテキストレイアウトエンジンは、一行の中で使われたすべてのフォントの
高さ（ascent, descent, leading）を比較し、**一番大きい値を採用** します。

**根拠となるソースコード:**

```cpp
// qtbase/src/gui/text/qtextengine.cpp  1449-1453行
if (engineIdx != 0) {  // フォールバックフォント（非プライマリ）の場合
    QFontEngine *actualFontEngine = static_cast<QFontEngineMulti *>(fontEngine)->engine(engineIdx);
    si.ascent  = qMax(actualFontEngine->ascent(),  si.ascent);   // 大きい方を採用
    si.descent = qMax(actualFontEngine->descent(), si.descent);   // 大きい方を採用
    si.leading = qMax(actualFontEngine->leading(), si.leading);   // 大きい方を採用
}
```

**解説:**
- `engineIdx != 0` はプライマリフォント以外（＝フォールバックフォント）を意味する
- `qMax(a, b)` は a と b の大きい方を返す関数
- `si.ascent` 等は行のメトリクス（寸法情報）で、ここで更新される
- これは**意図的な設計**。フォールバックフォントの文字が切れて表示されることを防ぐため
- **これ自体はバグではない**

### 2-3. コンテンツサイズが変わる仕組み（検証済み: 事実）

行の高さが変わると、テキスト入力欄の「コンテンツの高さ」が変わります。

**根拠となるソースコード:**

```cpp
// qtdeclarative/src/quick/items/qquicktextinput.cpp  3044-3061行
do {
    line.setLineWidth(lineWidth);
    line.setPosition(QPointF(0, height));
    height += line.height();       // ← 行の高さを加算（フォールバック後は大きくなる）
    width = qMax(width, line.naturalTextWidth());
    line = m_textLayout.createLine();
} while (line.isValid());
// ...
contentSize = QSizeF(width, height);   // ← コンテンツサイズに反映
```

**解説:**
- `line.height()` は 2-2節 で qMax された値を含む行の高さ
- `contentSize` はテキスト入力欄が外部に報告する「中身の大きさ」
- フォールバック発生前後で `contentSize.height` の値が変わる

### 2-4. Qt が行っている補償（部分的にしか機能しない）

Qt の開発者はこの問題を認識しており、テキスト描画時に
ベースラインが動かないよう補償を行っています。

**根拠となるソースコード:**

```cpp
// qtdeclarative/src/quick/items/qquicktextinput.cpp  1914-1918行
QPointF offset(leftPadding(), topPadding());
if (d->autoScroll && d->m_textLayout.lineCount() > 0) {
    QFontMetricsF fm(d->font);
    // the y offset is there to keep the baseline constant
    // in case we have script changes in the text.
    // （テキスト内のスクリプト変更時にベースラインを一定に保つためのy補正）
    offset += -QPointF(d->hscroll,
        d->vscroll + d->m_textLayout.lineAt(0).ascent() - fm.ascent());
}
```

**解説（精密な数値トレース）:**
- `fm.ascent()` = プライマリフォントの ascent（例: 14px）
- `lineAt(0).ascent()` = qMax 集約された行の ascent（例: 18px）
- `delta` = 18 - 14 = 4px（差分）
- テキスト描画位置は `delta` ピクセル分**上にずらされる**

**しかし、この補償は vscroll が変化しない場合にしか機能しません。**
次の 2-5節と 2-5b節で、vscroll が変化してしまうケースを示します。

### 2-5. AlignTop でも発生する理由（検証済み: 欠陥）

AlignTop でも、**固定高さの TextInput でフォールバック後の行高さが
利用可能高さを超える場合**、ベースラインがずれます。

**根拠となるソースコード:**

```cpp
// qtdeclarative/src/quick/items/qquicktextinput.cpp  1808-1831行
if (!autoScroll || heightUsed <= height) {
    // テキストが収まる場合 → アライメントに基づいて vscroll を計算
    vscroll = -QQuickTextUtil::alignedY(heightUsed, height, vAlign);
} else {
    // テキストがはみ出す場合 → カーソルが見えるようにスクロール
    QRectF r = currentLine.rect();
    qreal top = r.top();
    int bottom = r.bottom();
    if (bottom - vscroll >= height) {
        vscroll = bottom - height;    // ← ここで vscroll が非ゼロに！
    }
    // ...
}
```

**解説（AlignTop + 固定高さの場合の精密なトレース）:**

条件: TextInput 固定高さ = 30px、topPadding = 5px、bottomPadding = 5px、
      利用可能高さ = 30 - 5 - 5 = **20px**

**フォールバック前（英語のみ、lineHeight=20）:**

```
heightUsed = 20 → heightUsed <= 20 なので if 分岐に入る
AlignTop: alignedY = 0 → vscroll = 0
```

**フォールバック後（日本語入力、lineHeight=24）:**

```
heightUsed = 24 → heightUsed > 20 なので else 分岐に入る ← ★ここが重要
bottom = 24, height = 20
bottom - vscroll(0) = 24 >= 20 → vscroll = 24 - 20 = 4 ← ★vscroll が突然4に！
```

**vscroll がフォールバックの前後で 0 → 4 にジャンプします。**

描画位置とベースラインの変化：

| 状態 | vscroll | delta | offset.y | ベースライン |
|------|---------|-------|----------|------------|
| 英語のみ | **0** | 0 | 5-0-0 = **5** | 5+14 = **19px** |
| 日本語入力後 | **4** | 4 | 5-4-4 = **-3** | -3+18 = **15px** |

**AlignTop にもかかわらず、ベースラインが 19px → 15px に移動。4px 上にずれる。**

この問題が起きる条件：
- テキスト入力欄が固定高さを持っている（heightValid() が true）
- プライマリフォントの行高さ ≤ 利用可能高さ < フォールバック後の行高さ
- **一般的な TextField はこの条件を満たす**（プライマリフォントに合わせてサイズされるため）

### 2-5b. VCenter/AlignBottom でも発生する理由（検証済み: 欠陥）

テキストが収まる場合でも、VCenter/AlignBottom では
`contentSize.height` の変化により vscroll が変わるため、ベースラインがずれます。

**精密なトレース（VCenter の場合）:**

条件: 固定高さ 40px、topPadding = 5px、bottomPadding = 5px、
      利用可能高さ = 40 - 5 - 5 = 30px

| 状態 | contentSize.height | vscroll | 描画 offset.y | ベースライン |
|------|-------------------|---------|--------------|------------|
| 英語のみ (lineHeight=20) | 20px | -(30-20)/2 = **-5** | 5-(-5)-0 = **10** | 10+14 = **24px** |
| 日本語入力後 (lineHeight=24) | 24px | -(30-24)/2 = **-3** | 5-(-3)-4 = **4** | 4+18 = **22px** |

**ベースラインが 24px → 22px に移動。2px 上にずれる。**

### 2-6. カーソルとテキストの描画位置ずれ（検証済み: 欠陥）

テキスト描画には 2-4節 の補償が適用されますが、
カーソルの位置計算には **同じ補償が適用されていません**。

**根拠となるソースコード:**

```cpp
// カーソル位置（qquicktextinput.cpp 873-874行）
qreal x = l.cursorToX(c) - d->hscroll + leftPadding();
qreal y = l.y() - d->vscroll + topPadding();
//              ↑ delta 補償なし
```

```cpp
// テキスト描画位置（qquicktextinput.cpp 1918行）
offset += -QPointF(d->hscroll,
    d->vscroll + d->m_textLayout.lineAt(0).ascent() - fm.ascent());
//              ↑ delta 補償あり
```

テキスト描画位置は delta 分上にずらされるが、カーソル位置はそのまま。
カーソルの上下端がテキストの上下端と数ピクセルずれます。

### 2-7. updateBaselineOffset とのさらなる不整合（検証済み: 欠陥）

`updateBaselineOffset()` は vscroll を一切考慮しません。

**根拠となるソースコード:**

```cpp
// qtdeclarative/src/quick/items/qquicktextinput.cpp  3086-3101行
void QQuickTextInputPrivate::updateBaselineOffset()
{
    QFontMetricsF fm(font);
    qreal yoff = 0;
    if (q->heightValid()) {
        const qreal surplusHeight = q->height() - contentSize.height()
                                    - q->topPadding() - q->bottomPadding();
        if (vAlign == QQuickTextInput::AlignBottom)
            yoff = surplusHeight;
        else if (vAlign == QQuickTextInput::AlignVCenter)
            yoff = surplusHeight/2;
    }
    q->setBaselineOffset(fm.ascent() + yoff + q->topPadding());
}
```

**AlignTop + コンテンツはみ出しケースでの不整合:**

| 項目 | 値 | 説明 |
|------|------|------|
| baselineOffset（報告値） | fm.ascent() + 0 + topPadding = 14 + 0 + 5 = **19px** | yoff=0（AlignTopなので） |
| 実際の描画ベースライン | offset.y + lineAscent = -3 + 18 = **15px** | vscroll=4 の影響 |
| **不整合** | **4px** | baselineOffset は vscroll を考慮していない |

外部のレイアウトシステム（例：親の Column で baseline alignment を使う場合）は
`baselineOffset` = 19px を信じてアイテムを配置しますが、
実際のテキストは 15px の位置にあります。**4px のずれが生じます。**

---

## 3. 欠陥の全体像

### すべての verticalAlignment で発生する共通の根本原因

```
フォントフォールバック発生
        ↓
qMax 集約で行の ascent/descent が増加（qtextengine.cpp 1449-1453行）
        ↓
contentSize.height が増加（qquicktextinput.cpp 3048-3061行）
        ↓
vscroll が再計算される（qquicktextinput.cpp 1808-1831行）
        ↓  ★ AlignTop: テキストがはみ出すと vscroll が 0 → 正の値にジャンプ
        ↓  ★ VCenter: contentSize 変化で vscroll が変化
        ↓  ★ AlignBottom: contentSize 変化で vscroll が変化
        ↓
updatePaintNode の補償は delta しか引かず、vscroll の変化を打ち消せない
        ↓
ベースラインがずれる
```

### Qt 本体（qtbase + qtdeclarative）側の欠陥一覧

| # | 欠陥 | ソースコードの場所 | 影響 |
|---|------|-------------------|------|
| 1 | vscroll 変化によるベースラインずれ（AlignTop） | qquicktextinput.cpp 1808-1831行 | 固定高さの TextInput でフォールバック後にコンテンツがはみ出すとベースラインが上にずれる |
| 2 | vscroll 変化によるベースラインずれ（VCenter/Bottom） | qquicktextinput.cpp 1808-1811行 | contentSize 変化により vscroll が再計算され、ベースラインがずれる |
| 3 | cursorRectangle に delta 補償がない | qquicktextinput.cpp 874行 | カーソルとテキストの上下端が数ピクセルずれる |
| 4 | updateBaselineOffset が vscroll を無視 | qquicktextinput.cpp 3086-3101行 | 報告される baselineOffset と実際の描画位置が不整合 |

### qtvirtualkeyboard 側の状況

| 確認項目 | 結果 |
|----------|------|
| ShadowInputControl の TextInput | verticalAlignment 未設定 → **AlignTop**（デフォルト） |
| ShadowInputControl の高さ | 固定高さではなく暗黙の高さ（implicitHeight）を使用。はみ出しは起きにくい |
| ShadowInputControl の contentHeight | `shadowInput.contentHeight` を使用。フォールバックで変化し、Flickable の contentHeight が変わる |
| TextInput の既知バグ回避 | `positionToRectangle()` のパディングバグを手動で補正済み（tst_inputpanel.qml 2308行） |

**結論: qtvirtualkeyboard 側に原因となるコードはありません。**
問題はすべて Qt 本体の QQuickTextInput の実装に起因しています。

ただし、ShadowInputControl の contentHeight 変化がレイアウトに影響する
可能性があります（Flickable の contentHeight が変わるため）。

---

## 4. 影響範囲

この問題は qtvirtualkeyboard に限った問題ではありません。
**Qt の TextInput / TextField を使うすべてのアプリケーション** で、
以下の条件が揃うと発生します：

1. プライマリフォントでは表示できない文字を入力する
   （英語フォント＋日本語入力、など）
2. フォールバックフォントの ascent/descent がプライマリフォントより大きい

そして **すべての verticalAlignment 設定** で以下のように発生します：

| 設定 | 発生条件 |
|------|---------|
| AlignTop | 固定高さの TextInput で、フォールバック後の行高さ > 利用可能高さ |
| VCenter | 固定高さの TextInput で、フォールバック前後で contentSize.height が変化 |
| AlignBottom | 同上 |
| 暗黙の高さ使用 | implicitHeight 変化による親レイアウトの再配置 |

**発生しないケース:**
- プライマリフォントがフォールバックフォントより大きい場合（delta = 0）
- フォントフォールバックが発生しない場合（同一フォントで全文字を描画可能）
- AlignTop + 利用可能高さが十分に大きい場合（フォールバック後もはみ出さない）

---

## 5. 検証に使用したソースコード一覧

| ファイル | 行番号 | 確認内容 |
|----------|--------|----------|
| `qtbase/src/gui/text/qtextengine.cpp` | 1440-1461 | qMax による ascent/descent/leading の集約。`engineIdx != 0` でフォールバック時のみ発動 |
| `qtdeclarative/.../qquicktextinput.cpp` | 1914-1921 | updatePaintNode 内のベースライン補償。コメントで設計意図を明記 |
| `qtdeclarative/.../qquicktextinput.cpp` | 1798-1846 | updateVerticalScroll 全体。`heightUsed > height` 時に else 分岐に入り vscroll がジャンプ |
| `qtdeclarative/.../qquicktextinput.cpp` | 3086-3101 | updateBaselineOffset。vscroll を一切考慮しない |
| `qtdeclarative/.../qquicktextinput.cpp` | 860-883 | cursorRectangle の位置計算。delta 補償なし |
| `qtdeclarative/.../qquicktextinput.cpp` | 3044-3077 | updateLayout: contentSize 計算 → updateBaselineOffset → contentSizeChanged |
| `qtvirtualkeyboard/src/components/ShadowInputControl.qml` | 68-76 | ShadowInput の TextInput。verticalAlignment 未設定（AlignTop）、暗黙の高さ使用 |
| `qtvirtualkeyboard/tests/.../tst_inputpanel.qml` | 2308 | TextInput の既知バグ（padding 未反映）への回避コード |

---

## 6. 対処方法の方向性

### A. Qt 本体の修正を求める場合（推奨）

Qt の JIRA（bugreports.qt.io）にバグとして報告し、以下の修正を依頼する：

1. **updateVerticalScroll で delta を考慮する**
   - フォールバックによる行高さ増加を、はみ出し計算から除外する
   - または、vscroll 計算を補償と連動させる

2. **cursorRectangle() に delta 補償を追加する**
   - updatePaintNode と同じ `lineAt(0).ascent() - fm.ascent()` の補償を適用する

3. **updateBaselineOffset() に vscroll を反映する**
   - `setBaselineOffset(fm.ascent() + yoff + topPadding - vscroll_compensation)` のように、
     実際の描画位置と一致させる

4. **contentSize.height をプライマリフォント基準で安定化するオプション**
   - フォールバック時のメトリクス変化を contentSize に反映しない選択肢の追加

### B. アプリケーション側で回避する場合

- TextField の高さを、想定されるフォールバックフォントの
  最大行高さで事前に確保する（はみ出しを防ぐ）
- プライマリフォントに CJK グリフを含むフォントを指定する
  （フォールバック自体を防ぐ）

### C. qtvirtualkeyboard 側で回避する場合（ShadowInputControl）

現状の ShadowInputControl は暗黙の高さ（implicitHeight）を使っているため、
コンテンツはみ出しによる vscroll ジャンプは起きにくいです。
ただし、contentHeight の変化が Flickable に伝播する影響を抑えたい場合は、
初回レイアウト時の contentHeight を固定値として保持する方法が考えられます。
