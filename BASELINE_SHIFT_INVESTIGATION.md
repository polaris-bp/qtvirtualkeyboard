# TextField ベースラインシフト問題 調査報告 (Qt 5.12)

## 概要

TextFieldにおいて、特定の文字やフォントで入力文字のベースラインが変動する事象について、
Qt 5.12 を対象に調査した。

## 再現条件

TextFieldに文字を入力する際、特定の文字（例: CJK文字、絵文字、特殊記号など）が入力されると、
テキスト全体のベースライン位置が上下にジャンプする。特に以下の状況で発生しやすい:

1. プライマリフォントに含まれない文字を入力した場合（フォントフォールバック発生時）
2. プリエディットテキスト（変換中テキスト）の入力・確定時
3. 異なるスクリプト体系（ラテン文字 → CJK文字など）が混在する場合

---

## 根本原因 (Qt 5.12 ソースコードに基づく詳細分析)

### 原因の概要

Qt 5.12 のテキストレイアウトエンジンにおける**フォントメトリクスの max 集約設計**と、
**`baselineOffset` 計算と描画補正の不一致**が根本原因である。

### 原因箇所1: `QTextEngine::shapeText()` のフォールバックメトリクス max 集約

**ファイル**: `qtbase/src/gui/text/qtextengine.cpp` (v5.12.0)

テキストシェーピング時、プライマリフォントにグリフが存在しない文字があると
`QFontEngineMulti` がフォールバックフォントを選択する。このとき、フォールバックフォントの
メトリクスがスクリプトアイテムに `qMax` で集約される:

```cpp
// qtextengine.cpp (Qt 5.12.0, shapeText 内)
if (fontEngine->type() == QFontEngine::Multi) {
    uint lastEngine = ~0u;
    for (int i = 0, glyph_pos = 0; i < itemLength; ++i, ++glyph_pos) {
        const uint engineIdx = initialGlyphs.glyphs[glyph_pos] >> 24;
        if (lastEngine != engineIdx) {
            itemBoundaries.append(i);
            itemBoundaries.append(glyph_pos);
            itemBoundaries.append(engineIdx);

            if (engineIdx != 0) {
                QFontEngine *actualFontEngine =
                    static_cast<QFontEngineMulti *>(fontEngine)->engine(engineIdx);
                si.ascent  = qMax(actualFontEngine->ascent(),  si.ascent);   // ← ここ
                si.descent = qMax(actualFontEngine->descent(), si.descent);  // ← ここ
                si.leading = qMax(actualFontEngine->leading(), si.leading);  // ← ここ
            }
            lastEngine = engineIdx;
        }
    }
}
```

**問題**: フォールバックフォントの ascent がプライマリフォントより大きい場合、
**1文字でも**フォールバックが発生するとスクリプトアイテム全体の ascent が拡大される。
例えば、プライマリフォント (ascent=14) から CJK フォント (ascent=18) にフォールバックすると、
`si.ascent = qMax(18, 14) = 18` となる。

### 原因箇所2: `QTextLayout::layout_helper()` の行レベル max 集約

**ファイル**: `qtbase/src/gui/text/qtextlayout.cpp` (v5.12.0)

行レイアウト時に、全スクリプトアイテムのメトリクスがさらに行レベルで max 集約される:

```cpp
// qtextlayout.cpp (Qt 5.12.0, layout_helper 内)
lbh.tmpData.leading = qMax(lbh.tmpData.leading + lbh.tmpData.ascent,
                           current.leading + current.ascent)
                      - qMax(lbh.tmpData.ascent, current.ascent);
lbh.tmpData.ascent  = qMax(lbh.tmpData.ascent,  current.ascent);
lbh.tmpData.descent = qMax(lbh.tmpData.descent, current.descent);
```

結果として `QTextLine::ascent()` はフォールバックフォントを含む**最大の ascent** を返す。

### 原因箇所3: `updateBaselineOffset()` と `updatePaintNode()` の不一致

**ファイル**: `qtdeclarative/src/quick/items/qquicktextinput.cpp` (v5.12.0)

#### `updateBaselineOffset()` (プライマリフォントのみ使用)

```cpp
// qquicktextinput.cpp (Qt 5.12.0, line ~3086)
void QQuickTextInputPrivate::updateBaselineOffset()
{
    Q_Q(QQuickTextInput);
    if (!q->isComponentComplete())
        return;
    QFontMetricsF fm(font);       // ← プライマリフォントの QFontMetrics
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
    //                   ^^^^^^^^^^ プライマリフォントの ascent のみ
}
```

**呼び出し元**:
- `setVAlign()` -- 垂直アライメント変更時
- `geometryChanged()` -- 高さ変更時 (vAlign != AlignTop の場合)
- `updateLayout()` -- **レイアウト更新の最後**（毎回呼ばれる）

#### `updatePaintNode()` (行 ascent とプライマリ ascent の差分で描画補正)

```cpp
// qquicktextinput.cpp (Qt 5.12.0, updatePaintNode 内)
QPointF offset(leftPadding(), topPadding());
if (d->autoScroll && d->m_textLayout.lineCount() > 0) {
    QFontMetricsF fm(d->font);
    // コメント: "the y offset is there to keep the baseline constant
    //           in case we have script changes in the text."
    offset += -QPointF(d->hscroll,
        d->vscroll + d->m_textLayout.lineAt(0).ascent() - fm.ascent());
    //                 ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    //                 行ascent(フォールバック込み) - プライマリascent = 補正値
} else {
    offset += -QPointF(d->hscroll, d->vscroll);
}
```

### 不一致の具体的シナリオ

以下の数値例で不一致を説明する:

| 状態 | プライマリ ascent | 行 ascent | baselineOffset | 描画 y 補正 |
|------|------------------|-----------|----------------|-------------|
| ラテン文字のみ | 14 | 14 | 14 + yoff + padding | 0 |
| CJK文字入力後 | 14 | 18 | 14 + yoff + padding (変化なし) | -4 (上方シフト) |

`baselineOffset` はプライマリフォントの ascent (14) に基づくため**変化しない**。
一方、`updatePaintNode()` は `lineAt(0).ascent() - fm.ascent() = 18 - 14 = 4` の
補正を加えて描画位置を上にシフトする。

しかし、**`contentSize.height()`** は行の高さ `QTextLine::height()` から計算されるため、
フォールバック発生時に `contentSize` が変化する。この `contentSize` の変化が
`updateBaselineOffset()` 内の `surplusHeight` 計算に影響し、`AlignVCenter` や
`AlignBottom` の場合に `yoff` が変動する。これが**ベースラインジャンプ**の直接的原因である。

**具体的な数値例** (AlignVCenter、TextField height=48、padding=6 の場合):

```
[ラテン文字のみ]
  contentSize.height = 17 (ascent 14 + descent 3)
  surplusHeight = 48 - 17 - 6 - 6 = 19
  yoff = 19 / 2 = 9.5
  baselineOffset = 14 + 9.5 + 6 = 29.5

[CJK文字入力後]
  contentSize.height = 23 (ascent 18 + descent 5)  ← 6px 増加
  surplusHeight = 48 - 23 - 6 - 6 = 13
  yoff = 13 / 2 = 6.5                              ← 3px 減少
  baselineOffset = 14 + 6.5 + 6 = 26.5             ← 3px 上方にジャンプ
```

### 原因箇所4: プリエディットテキストでの動的なメトリクス変動

Virtual Keyboard のプリエディットテキスト処理で、メトリクスが動的に変動する:

1. `QTextLayout::setPreeditArea(position, text)` が呼ばれるとレイアウトが無効化
2. `QTextEngine::validate()` がプリエディットテキストをレイアウト文字列に挿入:
   ```cpp
   layoutData->string.insert(specialData->preeditPosition, specialData->preeditText);
   ```
3. `QTextEngine::itemize()` が統合文字列を再セグメント化
4. プリエディット文字が異なるスクリプトに属する場合、フォントフォールバックが発生
5. `shapeText()` の max 集約で行 ascent が拡大 → `contentSize` 変化 → `baselineOffset` 変動
6. テキスト確定時にプリエディットが消えると行 ascent が元に戻り → 再度ジャンプ

本リポジトリの `qvirtualkeyboardinputcontext.cpp:103-118` の `setPreeditText()` では
デフォルトの下線フォーマット属性を付与してプリエディットを送出しているが、
フォントフォールバックによるメトリクス変動は `QTextEngine` / `QTextLayout` レイヤーで
発生するため、Virtual Keyboard 側での直接的な制御は困難である。

### 原因箇所5: `QFontEngineMulti::ascent()` の設計

```cpp
// qfontengine.cpp (Qt 5.12.0)
QFixed QFontEngineMulti::ascent()  const { return engine(0)->ascent(); }
QFixed QFontEngineMulti::descent() const { return engine(0)->descent(); }
QFixed QFontEngineMulti::leading() const { return engine(0)->leading(); }
```

`QFontEngineMulti` (フォントフォールバックを管理するエンジン) の `ascent()` は
**常にプライマリエンジン (engine(0)) の値のみ**を返す。
つまり `QFontMetrics::ascent()` はフォールバックフォントのメトリクスを反映しない。

これにより:
- `updateBaselineOffset()` の `fm.ascent()` = プライマリのみ (例: 14)
- `QTextLine::ascent()` = フォールバック込み max (例: 18)

という**構造的な不一致**が生じる。

---

## ベースライン計算チェーン全体図 (Qt 5.12)

```
[フォントファイル OS/2 テーブル]
  Win メトリクス: usWinAscent / usWinDescent (Qt 5.12 はこちらを使用)
        |
        v
[QFontEngine::ascent()/descent()]
  各フォントエンジンが生のメトリクスを返す
  ※ QFontEngineMulti::ascent() は engine(0) = プライマリのみ
        |
        v
[QTextEngine::shapeText()]  ← 原因箇所1
  フォールバック発生時に si.ascent = qMax(fallback.ascent, si.ascent)
        |
        v
[QTextLayout::layout_helper()]  ← 原因箇所2
  行レベルで line.ascent = qMax(line.ascent, item.ascent)
        |
        +----> [QTextLine::ascent()]  フォールバック込み最大値
        |              |
        |              v
        |      [updatePaintNode()]  ← 原因箇所3 (描画)
        |        描画補正: -(lineAscent - fontAscent) で上方シフト
        |        ※ autoScroll=true の場合のみ適用
        |
        +----> [QTextLine::height()]  行の高さ (ascent + descent + leading)
                       |
                       v
               [contentSize.height]  行高さの合計
                       |
                       v
               [updateBaselineOffset()]  ← 原因箇所3 (baselineOffset)
                 surplusHeight = itemHeight - contentSize.height - padding
                 yoff = surplusHeight / 2  (AlignVCenter の場合)
                 baselineOffset = fm.ascent() + yoff + topPadding
                                  ^^^^^^^^^ プライマリのみ
                                  → contentSize 変動で yoff が変動
                                  → baselineOffset がジャンプ
```

---

## 既存の報告

| Bug ID | 概要 | 関連度 |
|--------|------|--------|
| [QTBUG-95461](https://bugreports.qt.io/browse/QTBUG-95461) | QQuickTextInput のプリエディットテキスト管理不正 | 高 |
| [QTBUG-43226](https://bugreports.qt.io/browse/QTBUG-43226) | ベースラインが verticalAlignment に追従しない (Qt 5.6 で修正) | 中 |
| [QTBUG-17000](https://bugreports.qt.io/browse/QTBUG-17000) | TextInput のプリエディット中マイクロフォーカス位置が不正 | 中 |
| [QTBUG-30412](https://bugreports.qt.io/browse/QTBUG-30412) | フォールバック時の shapeTextWithHarfbuzz クラッシュ | 低 |
| [QTBUG-96735](https://bugreports.qt.io/browse/QTBUG-96735) | フォールバック時の別フォントサイズ指定の要望 | 低 |
| [QTBUG-109400](https://bugreports.qt.io/browse/QTBUG-109400) | Qt 5/6 間のフォント行間隔差異 | 参考 |

Qt Forum 関連スレッド:
- [TextField Vertical Alignment Problem](https://forum.qt.io/topic/98109/textfield-vertical-alignment-problem)
  - padding のデフォルト値と高さ不足によるテキスト位置変動
- [Multilingual QPlainTextEdit different font height](https://forum.qt.io/topic/45706/multilingual-qplaintextedit-different-font-height)
  - CJK フォールバックによる行高さ変動の報告

**注**: 「TextField + フォントフォールバックでベースラインがジャンプする」という
正確な事象に対応する QTBUG は見つからなかった。新規バグ報告の対象となりうる。

---

## 対策手段 (Qt 5.12 で利用可能なもの)

### Qt 5.12 で利用不可の機能

以下は Qt 6.8+ でのみ利用可能であり、Qt 5.12 では使用できない:

- `font.contextFontMerging` (Qt 6.8+)
- `font.preferTypoLineMetrics` (Qt 6.8+)
- `QFontDatabase::setApplicationFallbackFontFamilies()` (Qt 6.8+)
- `font.fallbackFamilies` QML プロパティ (Qt 6.x+)

### 対策1: 固定高さ + AlignVCenter + padding=0 (QML レベル、最も簡単)

```qml
TextField {
    height: 48  // フォールバック時のメトリクス変動を吸収できる十分な高さ
    verticalAlignment: TextInput.AlignVCenter
    topPadding: 0
    bottomPadding: 0
}
```

**効果**: ジャンプの視覚的影響を軽減する。`AlignVCenter` により `contentSize` 変動が
上下に均等分配されるため、ジャンプ量が約50%に減少する。

**制限**: 根本解決ではなく、ジャンプは残る。上記の数値例では 3px のジャンプが発生する。

### 対策2: メトリクスが一致するフォントの選択と fontconfig 設定 (Linux)

プライマリフォントとフォールバックフォントの ascent/descent を揃える:

```xml
<!-- ~/.config/fontconfig/fonts.conf -->
<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "fonts.dtd">
<fontconfig>
  <alias>
    <family>YourPrimaryFont</family>
    <prefer>
      <family>MetricallyCompatibleCJKFont</family>
    </prefer>
  </alias>
</fontconfig>
```

**効果**: フォールバックフォントのメトリクスがプライマリフォントと一致していれば、
`qMax` 集約による拡大が発生しないためジャンプが解消される。

**制限**: 全スクリプトでメトリクスが一致するフォントの組み合わせを見つけるのは困難。
プラットフォーム固有の設定が必要。

### 対策3: フォントファイルのメトリクス修正 (FontForge)

FontForge を使用してアプリケーションにバンドルするフォントのメトリクスを統一する:

1. 全フォントの `OS/2.sTypoAscender` / `OS/2.sTypoDescender` を同一値に設定
2. `OS/2.fsSelection` の `USE_TYPO_METRICS` フラグ (bit 7) を有効化
3. `OS/2.usWinAscent` / `OS/2.usWinDescent` も同一値に設定

**効果**: フォールバックが発生しても `qMax` の結果が変わらないためジャンプが解消。

**制限**: カスタムフォントの配布が必要。グリフの実際の高さとメトリクスが
合わない場合、描画がクリップされる可能性がある。

### 対策4: `QFont::NoFontMerging` でフォールバック無効化

```cpp
QFont font;
font.setStyleStrategy(QFont::NoFontMerging);
```

**効果**: フォントフォールバックを完全に無効化。メトリクス変動が発生しない。

**制限**: プライマリフォントに含まれない文字は豆腐 (□) で表示される。
多言語対応が必要な場合は使用不可。

### 対策5: qtbase パッチ — `QTextEngine::shapeText()` のメトリクス集約を抑制

`qtbase/src/gui/text/qtextengine.cpp` を修正し、フォールバックフォントの
メトリクスがスクリプトアイテムを拡大しないようにする:

```cpp
// 変更前:
if (engineIdx != 0) {
    QFontEngine *actualFontEngine =
        static_cast<QFontEngineMulti *>(fontEngine)->engine(engineIdx);
    si.ascent  = qMax(actualFontEngine->ascent(),  si.ascent);
    si.descent = qMax(actualFontEngine->descent(), si.descent);
    si.leading = qMax(actualFontEngine->leading(), si.leading);
}

// 変更後 (案: フォールバックメトリクスの集約を無効化):
if (engineIdx != 0) {
    // フォールバックフォントのメトリクスはプライマリフォントの
    // メトリクスで上書きし、行高さの変動を防ぐ
    // ※ グリフがクリップされる可能性があるため要検証
}
```

**効果**: フォールバック発生時もメトリクスが変動しないため、ベースラインジャンプが根本解消。

**制限**:
- Qt 本体のパッチが必要（カスタムビルドの維持コスト）
- フォールバックフォントのグリフがプライマリフォントのメトリクスに収まらない場合、
  グリフが上下にクリップされる可能性がある
- Vim プロジェクトが DirectWrite レンダリングで同様のアプローチを採用済み
  ([patch 9.1.2125](http://www.mail-archive.com/vim_dev@googlegroups.com/msg71548.html))

### 対策6: qtdeclarative パッチ — `updateBaselineOffset()` で行メトリクスを使用

`qtdeclarative/src/quick/items/qquicktextinput.cpp` を修正し、`baselineOffset` 計算で
プライマリフォントの ascent ではなく行の ascent を使用する:

```cpp
// 変更前:
void QQuickTextInputPrivate::updateBaselineOffset()
{
    ...
    QFontMetricsF fm(font);
    ...
    q->setBaselineOffset(fm.ascent() + yoff + q->topPadding());
}

// 変更後:
void QQuickTextInputPrivate::updateBaselineOffset()
{
    ...
    QFontMetricsF fm(font);
    qreal ascent = fm.ascent();
    // 行メトリクスが利用可能な場合はそちらを使用
    if (m_textLayout.lineCount() > 0) {
        ascent = m_textLayout.lineAt(0).ascent();
    }
    ...
    q->setBaselineOffset(ascent + yoff + q->topPadding());
}
```

**効果**: `baselineOffset` が描画位置と一致するようになる。ただし、`contentSize` も
行メトリクスに基づいて計算されるため、`surplusHeight` の変動は残る。
`baselineOffset` と実際の描画位置の一致度は改善される。

**制限**: `baselineOffset` 自体は変動するため、外部からの baseline アンカー依存の
レイアウトには影響が残る。

### 対策7: qtdeclarative パッチ — `contentSize` 計算をプライマリフォント基準に固定

`updateLayout()` の `contentSize` 計算で、`QTextLine::height()` の代わりに
プライマリフォントの `QFontMetrics::height()` を使用する:

```cpp
// 変更前:
do {
    line.setLineWidth(lineWidth);
    line.setPosition(QPointF(0, height));
    height += line.height();              // ← フォールバック込みの行高さ
    ...
} while (line.isValid());
...
contentSize = QSizeF(width, height);

// 変更後:
QFontMetricsF fm(font);
qreal fixedLineHeight = fm.height();     // プライマリフォントの行高さ
do {
    line.setLineWidth(lineWidth);
    line.setPosition(QPointF(0, height));
    height += fixedLineHeight;            // ← プライマリフォント基準で固定
    ...
} while (line.isValid());
...
contentSize = QSizeF(width, height);
```

**効果**: `contentSize` がフォールバックに影響されなくなるため、`surplusHeight` と
`yoff` が安定し、`baselineOffset` のジャンプが解消される。`updatePaintNode()` の
既存の描画補正 (`lineAscent - fontAscent`) と組み合わせることで、描画位置も安定する。

**制限**: フォールバックフォントのグリフが `contentSize` の範囲を超える場合、
クリッピングが発生する可能性がある。

### 対策の比較表

| # | 対策 | 修正対象 | 効果 | 実装難易度 | リスク |
|---|------|----------|------|-----------|--------|
| 1 | 固定高さ + AlignVCenter | QML (アプリ側) | 軽減 (~50%) | 低 | なし |
| 2 | fontconfig でフォールバック指定 | システム設定 | 解消 (条件付き) | 中 | プラットフォーム依存 |
| 3 | フォントメトリクス修正 | フォントファイル | 解消 | 中 | グリフクリッピング |
| 4 | NoFontMerging | C++ | 解消 | 低 | 多言語不可 |
| 5 | shapeText() パッチ | qtbase | 根本解消 | 高 | グリフクリッピング |
| 6 | updateBaselineOffset() パッチ | qtdeclarative | 改善 | 中 | baselineOffset 変動は残る |
| **7** | **contentSize 固定パッチ** | **qtdeclarative** | **根本解消** | **中** | **グリフクリッピング** |

### 推奨アプローチ

**短期**: 対策1 (固定高さ + AlignVCenter) をアプリケーション側で適用し、
視覚的影響を軽減する。

**中期**: 対策7 (contentSize 固定パッチ) を `qtdeclarative` に適用する。
既存の `updatePaintNode()` の描画補正と組み合わせることで、
baselineOffset と描画位置の両方を安定させることができる。
これは `qtbase` への変更が不要で、影響範囲が限定される。

**長期**: Qt 6.8+ への移行を検討する。`contextFontMerging` と
`preferTypoLineMetrics` が利用可能になり、フォールバック関連の
問題がより体系的に対処可能になる。

---

## 対象バージョン

- 再現環境: Qt 5.12
- 影響範囲: Qt 5.x 全般（フォントフォールバックが発生する全環境）
- Qt 5.12 の `QFont::StyleStrategy` にはフォールバックメトリクスを制御する
  オプションが存在しない（`NoFontMerging` による完全無効化のみ）
- Qt 5.12 の QPA レイヤー (`QPlatformFontDatabase::fallbacksForFamily()`) では
  フォールバックフォントの選択順序は制御可能だが、メトリクスの上書きは不可
