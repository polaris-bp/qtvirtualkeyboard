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

テキスト入力欄の設定によって異なります：

| 設定 | ずれ量 | 発生するか |
|------|--------|-----------|
| 上揃え（AlignTop） | 0px | 発生しない |
| 上下中央揃え（VCenter） | 行高さの変化量 ÷ 2 | **発生する** |
| 下揃え（AlignBottom） | 行高さの変化量 | **発生する** |

一般的な TextField は上下中央揃え（VCenter）を使うため、
ほとんどのアプリケーションで発生します。

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

### 2-4. Qt が行っている補償（部分的に機能）

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
- これにより、テキストのベースライン位置は `topPadding + fm.ascent()` で一定に保たれる

**この補償は AlignTop の場合にのみ完全に機能します。**

### 2-5. 補償が VCenter/AlignBottom で不完全な理由（検証済み: 欠陥）

補償は描画位置を上にずらしますが、`contentSize.height` の変化による
垂直スクロール値（vscroll）の再計算までは防げません。

**根拠となるソースコード:**

```cpp
// qtdeclarative/src/quick/items/qquicktextinput.cpp  1808-1811行
if (!autoScroll || heightUsed <= height) {
    // text fits in br; use vscroll for alignment
    vscroll = -QQuickTextUtil::alignedY(
                heightUsed, height, vAlign & ~(Qt::AlignAbsolute|Qt::AlignHorizontal_Mask));
}
```

**解説（VCenter の場合の精密なトレース）:**

条件: 固定高さ 40px、topPadding = 5px、bottomPadding = 5px、
      利用可能高さ = 40 - 5 - 5 = 30px

| 状態 | contentSize.height | vscroll | 描画 offset.y | ベースライン位置 |
|------|-------------------|---------|--------------|----------------|
| 英語のみ (ascent=14, lineHeight=20) | 20px | -(30-20)/2 = **-5** | 5-(-5)-0 = **10** | 10+14 = **24px** |
| 日本語入力後 (ascent=18, lineHeight=24) | 24px | -(30-24)/2 = **-3** | 5-(-3)-4 = **4** | 4+18 = **22px** |

**ベースラインが 24px → 22px に移動。2px 上にずれる。**

同じ計算を AlignTop で行うと：

| 状態 | contentSize.height | vscroll | 描画 offset.y | ベースライン位置 |
|------|-------------------|---------|--------------|----------------|
| 英語のみ | 20px | **0** | 5-0-0 = **5** | 5+14 = **19px** |
| 日本語入力後 | 24px | **0** | 5-0-4 = **1** | 1+18 = **19px** |

**AlignTop ではベースラインは 19px のまま。ずれない。**

VCenter で補償が不完全になる原因：
1. contentSize.height が 20→24 に増加
2. VCenter の vscroll が再計算される（-5 → -3 に変化）
3. updatePaintNode の補償は delta=4 を引くが、vscroll が +2 変化するため、差し引き 2px 分ずれる
4. AlignTop では vscroll が常に 0 なので、この問題は起きない

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

**具体的な影響（AlignTop の場合）:**

| 要素 | y 座標の計算 | 日本語入力後の値 |
|------|-------------|----------------|
| テキスト描画の先頭 | topPadding - vscroll - delta | 5 - 0 - 4 = **1px** |
| カーソルの先頭 | l.y() - vscroll + topPadding | 0 - 0 + 5 = **5px** |

テキストは y=1px から始まるのに、カーソルは y=5px から始まる。
カーソルがテキストより **4px 下にずれている**。

ただし、カーソルとテキストの**ベースライン位置は一致**しています：
- テキストのベースライン: 1 + 18(lineAscent) = 19px
- カーソル位置 + fm.ascent: 5 + 14(fmAscent) = 19px

つまり、文字の底辺は揃うが、カーソルの上下端がテキストの上下端と合わない状態です。

---

## 3. 責任の切り分け（自社 or Qt 本体）

### Qt 本体（qtbase + qtdeclarative）側の問題

| 分類 | 内容 | ソースコードの場所 | 性質 |
|------|------|-------------------|------|
| 設計 | フォールバック時に qMax で行メトリクスを集約 | qtbase: qtextengine.cpp 1449-1453行 | 意図的な設計（バグではない） |
| 設計の副作用 | contentSize.height がフォールバックで変化 | qtdeclarative: qquicktextinput.cpp 3048-3061行 | 設計の論理的帰結 |
| **欠陥** | VCenter/AlignBottom で補償が不完全（vscroll の変化を考慮していない） | qtdeclarative: qquicktextinput.cpp 1808-1811, 1918行 | **バグ** |
| **欠陥** | cursorRectangle に delta 補償がない | qtdeclarative: qquicktextinput.cpp 874行 | **バグ** |

### qtvirtualkeyboard 側の状況

| 確認項目 | 結果 |
|----------|------|
| ShadowInputControl の TextInput | verticalAlignment 未設定 → **AlignTop**（デフォルト）。ベースラインずれは発生しない |
| ShadowInputControl の contentHeight | `shadowInput.contentHeight` を使用。フォールバックで変化するため、Flickable の contentHeight が変わる |
| キーラベルの verticalAlignment | `Text.AlignVCenter` を使用（style.qml 108行等）。ただしキーラベルは入力中に変わらないので影響なし |
| TextInput の既知バグ回避 | `positionToRectangle()` のパディングバグを手動で補正済み（tst_inputpanel.qml 2308行） |

**結論: qtvirtualkeyboard 側に原因となるコードはありません。**

ユーザーが体験する「テキスト位置ずれ」は、**アプリケーション側の TextField** が
VCenter を使っている場合に発生する Qt 本体の問題です。

---

## 4. 影響範囲

この問題は qtvirtualkeyboard に限った問題ではありません。
**Qt の TextInput / TextField を使うすべてのアプリケーション** で、
以下の条件がすべて揃うと発生します：

1. プライマリフォントでは表示できない文字を入力する
   （英語フォント＋日本語入力、など）
2. フォールバックフォントの ascent/descent がプライマリフォントより大きい
3. テキスト入力欄が **VCenter または AlignBottom** を使っている、
   **または** 暗黙の高さ（implicitHeight）に依存している

逆に、以下の場合は発生しません：
- AlignTop を使っている場合（ベースライン補償が完全に機能する）
- プライマリフォントがフォールバックフォントより大きい場合（delta = 0）
- フォントフォールバックが発生しない場合（同一フォントで全文字を描画）

---

## 5. 検証に使用したソースコード一覧

| ファイル | 行番号 | 確認内容 |
|----------|--------|----------|
| `qtbase/src/gui/text/qtextengine.cpp` | 1440-1461 | qMax による ascent/descent/leading の集約。`engineIdx != 0` でフォールバック時のみ発動 |
| `qtdeclarative/.../qquicktextinput.cpp` | 1914-1921 | updatePaintNode 内のベースライン補償。コメントで設計意図を明記 |
| `qtdeclarative/.../qquicktextinput.cpp` | 1808-1811 | updateVerticalScroll の vscroll 計算。contentSize.height に依存 |
| `qtdeclarative/.../qquicktextinput.cpp` | 3086-3101 | updateBaselineOffset。fm.ascent()（プライマリフォント）と surplusHeight を使用 |
| `qtdeclarative/.../qquicktextinput.cpp` | 860-883 | cursorRectangle の位置計算。delta 補償なし |
| `qtdeclarative/.../qquicktextinput.cpp` | 3044-3061 | contentSize の計算。line.height() を加算 |
| `qtdeclarative/.../qquicktextinput.cpp` | 3067-3071 | implicitHeight の更新。contentSize.height + padding |
| `qtvirtualkeyboard/src/components/ShadowInputControl.qml` | 68-76 | ShadowInput の TextInput。verticalAlignment 未設定（AlignTop） |
| `qtvirtualkeyboard/tests/.../tst_inputpanel.qml` | 2308 | TextInput の既知バグ（padding 未反映）への回避コード |

---

## 6. 対処方法の方向性

### A. Qt 本体の修正を求める場合（推奨）

Qt の JIRA（bugreports.qt.io）にバグとして報告し、以下の修正を依頼する：

1. **cursorRectangle() に delta 補償を追加**
   - updatePaintNode と同じ `lineAt(0).ascent() - fm.ascent()` の補償を
     cursorRectangle の y 座標計算に適用する

2. **VCenter/AlignBottom 使用時の vscroll 再計算問題の解決**
   - contentSize.height をプライマリフォント基準で固定するオプションの追加
   - または、vscroll 計算で delta を考慮するよう修正

### B. アプリケーション側で回避する場合

- TextField の高さを、想定されるフォールバックフォントの
  最大行高さで事前に確保する
- verticalAlignment を AlignTop に変更する
  （デザイン上許容される場合のみ）

### C. qtvirtualkeyboard 側で回避する場合（ShadowInputControl）

現状の ShadowInputControl は AlignTop を使っているため、
ベースラインずれは発生しません。ただし、contentHeight の変化が
Flickable に伝播する影響を抑えたい場合は、
最初のレイアウト時の contentHeight を固定値として保持する方法が考えられます。
