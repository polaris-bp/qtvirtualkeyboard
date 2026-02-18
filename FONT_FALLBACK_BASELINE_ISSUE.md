# TextInput フォントフォールバック起因のテキスト位置ずれ — 調査報告

> **責任切り分けの結論:**
> Qt 本体（qtbase / qtdeclarative）の実装上の欠陥であり、
> qtvirtualkeyboard に原因はない。

---

## 1. 現象

テキスト入力欄（TextInput / TextField）で、プライマリフォントに含まれない
文字（例: 英語フォント設定で日本語・韓国語を入力）を打つと、
**既存テキスト全体の表示位置が数ピクセル上下にずれる。**

```
"hello"  → テキストは安定
"helloあ" → テキスト全体が上にジャンプ
```

この現象は **すべての verticalAlignment 設定で発生する。**

---

## 2. 原因の全体像

```
フォントフォールバック発生
    ↓
行の ascent/descent が qMax で拡大される        ← qtextengine.cpp:1451
    ↓
contentSize.height が増加する                    ← qquicktextinput.cpp:3048
    ↓
vscroll が再計算される                           ← qquicktextinput.cpp:1808
    ↓
updatePaintNode の補償が vscroll 変化を打ち消せない ← qquicktextinput.cpp:1918
    ↓
ベースラインがずれる
```

以下、各ステップをソースコードで証明する。

---

## 3. ソースコードによる証明

### 3-1. 行メトリクスの qMax 集約（意図的な設計）

```cpp
// qtbase/src/gui/text/qtextengine.cpp  1449–1453行
if (engineIdx != 0) {
    QFontEngine *actualFontEngine =
        static_cast<QFontEngineMulti *>(fontEngine)->engine(engineIdx);
    si.ascent  = qMax(actualFontEngine->ascent(),  si.ascent);
    si.descent = qMax(actualFontEngine->descent(), si.descent);
    si.leading = qMax(actualFontEngine->leading(), si.leading);
}
```

- `engineIdx != 0` = フォールバックフォント使用時
- フォールバックフォントの方が大きければ、行の ascent/descent が拡大される
- **意図的な設計**（文字の切れ防止）であり、これ自体はバグではない

### 3-2. contentSize.height の増加

```cpp
// qtdeclarative/src/quick/items/qquicktextinput.cpp  3044–3061行
do {
    line.setLineWidth(lineWidth);
    line.setPosition(QPointF(0, height));
    height += line.height();          // ← 3-1 で拡大された行高さ
    line = m_textLayout.createLine();
} while (line.isValid());
contentSize = QSizeF(width, height);  // ← コンテンツサイズに反映
```

行高さが変わると contentSize.height も変わる。これが後続の不具合の起点となる。

### 3-3. vscroll の再計算

```cpp
// qtdeclarative/src/quick/items/qquicktextinput.cpp  1798–1846行
const qreal height = qMax<qreal>(0,
    q->height() - q->topPadding() - q->bottomPadding());  // 利用可能高さ
qreal heightUsed = contentSize.height();

if (!autoScroll || heightUsed <= height) {
    // テキストが収まる → アライメントに基づく vscroll
    vscroll = -QQuickTextUtil::alignedY(heightUsed, height, vAlign);
} else {
    // テキストがはみ出す → カーソルを見せるためにスクロール
    if (bottom - vscroll >= height)
        vscroll = bottom - height;           // ← vscroll がジャンプ
    // ...
}
```

contentSize.height が増えると、vscroll は以下のように変化する:

- **テキストが収まる場合（AlignTop）**: vscroll = 0 のまま
- **テキストが収まる場合（VCenter）**: vscroll が contentSize 変化に応じて変化
- **テキストがはみ出す場合（全アライメント）**: vscroll が 0 から正の値にジャンプ

### 3-4. 補償コードと、その限界

```cpp
// qtdeclarative/src/quick/items/qquicktextinput.cpp  1914–1918行
QPointF offset(leftPadding(), topPadding());
if (d->autoScroll && d->m_textLayout.lineCount() > 0) {
    QFontMetricsF fm(d->font);
    // the y offset is there to keep the baseline constant
    // in case we have script changes in the text.
    offset += -QPointF(d->hscroll,
        d->vscroll + d->m_textLayout.lineAt(0).ascent() - fm.ascent());
}
```

この補償の結果、描画 offset.y は以下になる:

```
offset.y = topPadding − vscroll − delta
ベースライン = offset.y + lineAscent
             = topPadding − vscroll − delta + lineAscent
             = topPadding − vscroll + fm.ascent()
```

**vscroll が変化しなければベースラインは一定に保たれる。**
しかし 3-3 で示したように vscroll は変化する。
補償は delta のみを引くが、vscroll の変化分を打ち消す仕組みがない。

---

## 4. 数値による証明

以下の共通条件で各アライメントを検証する:

```
プライマリフォント: ascent=14, descent=6  → lineHeight=20
フォールバック後:   ascent=18, descent=6  → lineHeight=24
delta = 18 − 14 = 4
TextInput: 固定高さ=30, topPadding=5, bottomPadding=5
利用可能高さ = 30 − 5 − 5 = 20
```

### 4-1. AlignTop（テキストがはみ出す場合）

| | 英語のみ | 日本語入力後 |
|---|---|---|
| heightUsed | 20 | 24 |
| heightUsed ≤ 20? | Yes → if 分岐 | **No → else 分岐** |
| vscroll | 0 | bottom−height = 24−20 = **4** |
| delta | 0 | 4 |
| offset.y | 5−0−0 = 5 | 5−4−4 = **−3** |
| ベースライン | 5+14 = **19** | −3+18 = **15** |
| **ずれ** | | **4px 上にずれる** |

### 4-2. VCenter（テキストが収まる場合）

利用可能高さを 30px に変更（固定高さ 40px）:

| | 英語のみ | 日本語入力後 |
|---|---|---|
| heightUsed | 20 | 24 |
| heightUsed ≤ 30? | Yes | Yes |
| vscroll | −(30−20)/2 = −5 | −(30−24)/2 = **−3** |
| delta | 0 | 4 |
| offset.y | 5+5−0 = 10 | 5+3−4 = **4** |
| ベースライン | 10+14 = **24** | 4+18 = **22** |
| **ずれ** | | **2px 上にずれる** |

### 4-3. AlignTop（テキストが収まる十分な高さがある場合）

利用可能高さを 30px に変更:

| | 英語のみ | 日本語入力後 |
|---|---|---|
| heightUsed ≤ 30? | Yes | Yes |
| vscroll | 0 | 0 |
| delta | 0 | 4 |
| offset.y | 5−0−0 = 5 | 5−0−4 = 1 |
| ベースライン | 5+14 = **19** | 1+18 = **19** |
| **ずれ** | | **なし** |

**これが補償コードが設計通りに機能する唯一のケースである。**

---

## 5. 付随する欠陥

### 5-1. カーソルとテキストの位置ずれ

```cpp
// cursorRectangle() — qquicktextinput.cpp 873–874行
qreal y = l.y() - d->vscroll + topPadding();    // delta 補償なし
```

```cpp
// updatePaintNode() — qquicktextinput.cpp 1918行
offset.y = topPadding - vscroll - delta;          // delta 補償あり
```

代数的に確認:

```
カーソル y  = l.y() − vscroll + topPadding
テキスト y  = topPadding − vscroll − delta + l.y()    （addTextLayout の offset 経由）
差分        = delta（常に一定、vscroll に依存しない）
```

フォールバック発生時、カーソルは常にテキストより **delta ピクセル下** に描画される。
これはアライメントやはみ出しの有無に関係なく発生する。

### 5-2. baselineOffset と実描画位置の不整合

```cpp
// updateBaselineOffset() — qquicktextinput.cpp 3086–3101行
q->setBaselineOffset(fm.ascent() + yoff + q->topPadding());
// vscroll を一切参照していない
```

コンテンツがはみ出して vscroll が非ゼロになった場合、
外部に報告する baselineOffset と実際の描画位置が乖離する。

4-1 の例: 報告値 = 19px、実描画 = 15px → **4px の乖離**

---

## 6. 欠陥の一覧

| # | 内容 | ソースコード | 発生条件 |
|---|------|-------------|---------|
| 1 | vscroll ジャンプによるベースラインずれ | qquicktextinput.cpp:1808–1831, 1918 | 固定高さ TextInput で、フォールバック後の lineHeight が利用可能高さを超える場合（全アライメント） |
| 2 | contentSize 変化による vscroll 再計算でベースラインずれ | qquicktextinput.cpp:1808–1811, 1918 | 固定高さ TextInput で VCenter/AlignBottom を使用し、テキストが収まる場合 |
| 3 | cursorRectangle に delta 補償なし | qquicktextinput.cpp:874 | フォールバック発生時、常に |
| 4 | baselineOffset が vscroll を無視 | qquicktextinput.cpp:3086–3101 | コンテンツがはみ出して vscroll が非ゼロになった場合 |

- 欠陥 1, 2 がユーザーに見える「テキストのジャンプ」の直接原因
- 欠陥 3, 4 は付随する不整合

---

## 7. 責任の切り分け

### Qt 本体の問題である根拠

| 根拠 | 詳細 |
|------|------|
| 欠陥のあるコードはすべて qtdeclarative 内 | QQuickTextInput の updateVerticalScroll, updatePaintNode, cursorRectangle, updateBaselineOffset |
| 根本原因のコードは qtbase 内 | QTextEngine::shapeText() の qMax 集約 |
| Qt 開発者自身が問題を認識 | updatePaintNode のコメント: "keep the baseline constant in case we have script changes" |

### qtvirtualkeyboard 側の確認結果

| 確認項目 | 確認結果 | 影響 |
|----------|---------|------|
| ShadowInputControl の TextInput（68–76行） | verticalAlignment 未設定 → AlignTop | 暗黙の高さを使用しており、はみ出しは起きにくい |
| ShadowInputControl の contentHeight（57–58行） | shadowInput.contentHeight を Flickable に反映 | フォールバック時に contentHeight が変化する |
| テストコード（tst_inputpanel.qml:2308行） | TextInput の padding バグを手動補正済み | 既知の TextInput バグへの対処実績あり |

**qtvirtualkeyboard に原因となるコードはない。**

---

## 8. 影響範囲

Qt の TextInput / TextField を使う **すべてのアプリケーション** で、
以下の条件が揃うと発生する:

1. プライマリフォントで描画できない文字を入力する
2. フォールバックフォントの ascent/descent がプライマリフォントより大きい

発生しないケースは限られる:

- AlignTop かつ、フォールバック後の lineHeight でも利用可能高さに収まる場合
- フォールバックフォントのメトリクスがプライマリフォント以下の場合
- フォントフォールバック自体が発生しない場合

---

## 9. 対処方法の方向性

### A. Qt 本体への修正依頼（推奨）

1. **updateVerticalScroll でフォールバックによる高さ増加を考慮する**
   — vscroll 計算が delta を打ち消すよう修正
2. **cursorRectangle に delta 補償を追加する**
3. **updateBaselineOffset に vscroll を反映する**

### B. アプリケーション側での回避

- TextField の高さを、フォールバックフォントの行高さでも収まるサイズに設定する
- プライマリフォントに CJK グリフを含むフォントを指定する（フォールバック自体を回避）

---

## 付録: 検証に使用したソースコード

| ファイル | 行番号 | 内容 |
|----------|--------|------|
| `qtbase/src/gui/text/qtextengine.cpp` | 1440–1461 | qMax 集約（shapeText 内） |
| `qtdeclarative/.../qquicktextinput.cpp` | 1798–1846 | updateVerticalScroll |
| 同上 | 1914–1921 | updatePaintNode 内のベースライン補償 |
| 同上 | 860–883 | cursorRectangle |
| 同上 | 3086–3101 | updateBaselineOffset |
| 同上 | 3044–3077 | updateLayout（contentSize 計算） |
| `qtvirtualkeyboard/.../ShadowInputControl.qml` | 68–76 | ShadowInput の TextInput |
| `qtvirtualkeyboard/.../tst_inputpanel.qml` | 2308 | TextInput padding バグの回避コード |
