# TextField ベースラインシフト問題 調査報告

## 概要

TextFieldにおいて、特定の文字やフォントで入力文字のベースラインが変動する事象について調査した。

## 現象

TextFieldに文字を入力する際、特定の文字（例: CJK文字、絵文字、特殊記号など）が入力されると、
テキスト全体のベースライン位置が上下にジャンプする。特に以下の状況で発生しやすい:

1. プライマリフォントに含まれない文字を入力した場合（フォントフォールバック発生時）
2. プリエディットテキスト（変換中テキスト）の入力・確定時
3. 異なるスクリプト体系（ラテン文字 → CJK文字など）が混在する場合

## 根本原因

### フォントメトリクスの max 集約メカニズム

Qt のテキストレイアウトエンジンは、行内の全てのスクリプトアイテムの**最大値**をメトリクスとして採用する。
この設計が根本原因である。

#### ベースライン計算チェーン

```
QFontEngine::ascent()              ← フォントファイルから生のメトリクス取得
        |
        v
QTextEngine::shapeText()           ← フォールバックフォント含む全エンジンの qMax を取得
        |                             si.ascent = qMax(actualFontEngine->ascent(), si.ascent)
        v
QTextLayout::layout_helper()       ← 行内全スクリプトアイテムの qMax
        |                             line.ascent = qMax(line.ascent, item.ascent)
        v
QTextLine::ascent()                ← 集約後の行 ascent を返す
        |
        v
QQuickTextInputPrivate::           ← baselineOffset = ascent + vAlign_offset + topPadding
  updateBaselineOffset()
        |
        v
QQuickTextInput::                  ← 描画補正: offset += lineAt(0).ascent() - fm.ascent()
  updatePaintNode()
```

#### 問題の核心

`baselineOffset` とアイテムの高さは**プライマリフォント**の `QFontMetrics` から計算されるが、
実際のテキスト描画は**フォールバックフォントを含む** `QTextLine` メトリクスを使用する。

例:
- プライマリフォント (Arial): ascent=14, descent=3, height=17
- フォールバックフォント (Noto Sans CJK): ascent=18, descent=5, height=23

CJK文字が1文字でも入力されると:
1. `QTextEngine::shapeText()` で `si.ascent = qMax(14, 18) = 18` に拡大
2. `QTextLine::ascent()` が 14 → 18 に変化
3. 行の高さが 17 → 23 に拡大
4. テキストが視覚的に上下にジャンプ

### プリエディットテキスト固有の問題

Virtual Keyboard のプリエディットテキスト処理にも関連する問題がある:

1. `QTextLayout::setPreeditArea()` が呼ばれるとレイアウトが無効化される
2. プリエディットテキストがレイアウト文字列に挿入される
3. `QTextEngine::itemize()` が統合文字列を再セグメント化
4. プリエディット文字が異なるスクリプトに属する場合、フォントフォールバックが発生
5. フォールバックフォントのメトリクスが異なると `baselineOffset` が再計算される
6. テキスト確定時にメトリクスが元に戻り、再度ジャンプが発生

本リポジトリ (`qtvirtualkeyboard`) では、`qvirtualkeyboardinputcontext.cpp:103-118` の
`setPreeditText()` でデフォルトの下線フォーマット属性が付与される。プリエディットテキストの
送出自体は正しく実装されているが、フォントフォールバックによるメトリクス変動は
Qt の `QTextEngine` / `QTextLayout` レイヤーで発生するため、Virtual Keyboard 側での
直接的な制御は困難である。

## 既存の報告

| Bug ID | 概要 | 関連度 |
|--------|------|--------|
| [QTBUG-95461](https://bugreports.qt.io/browse/QTBUG-95461) | QQuickTextInput のプリエディットテキスト管理不正 | 高 |
| [QTBUG-43226](https://bugreports.qt.io/browse/QTBUG-43226) | ベースラインが verticalAlignment に追従しない | 中 |
| [QTBUG-17000](https://bugreports.qt.io/browse/QTBUG-17000) | TextInput のプリエディット中マイクロフォーカス位置が不正 | 中 |
| [QTBUG-30412](https://bugreports.qt.io/browse/QTBUG-30412) | フォールバック時の shapeTextWithHarfbuzz クラッシュ | 低 |
| [QTBUG-96735](https://bugreports.qt.io/browse/QTBUG-96735) | フォールバック時の別フォントサイズ指定の要望 | 低 |

Qt Forum でも複数の関連スレッドが存在:
- [TextField Vertical Alignment Problem](https://forum.qt.io/topic/98109/textfield-vertical-alignment-problem)
- [Multilingual QPlainTextEdit different font height](https://forum.qt.io/topic/45706/multilingual-qplaintextedit-different-font-height)

## 対策手段

### 対策1: `font.contextFontMerging` の使用 (Qt 6.8+)

```qml
TextField {
    font.contextFontMerging: true
}
```

**効果**: フォールバックフォントをキャラクター単位ではなくコンテキスト全体で選択し、
使用されるフォールバックフォント数を減らすことでメトリクスの不整合を軽減する。

**制限**: 計算コストが高い。根本解決ではなくフォールバックフォントの統一化に留まる。

### 対策2: `font.preferTypoLineMetrics` の使用 (Qt 6.8+)

```qml
TextField {
    font.preferTypoLineMetrics: true
}
```

**効果**: Win メトリクスの代わりに Typo メトリクスを使用する。多くのフォントで
Win メトリクスは Typo メトリクスより大きいため、プライマリフォントと
フォールバックフォント間のメトリクス差を縮小できる可能性がある。

**制限**: フォントによっては効果がないか、逆効果の場合もある。

### 対策3: フォントファミリーの明示的指定

```qml
TextField {
    font.family: "Noto Sans"
    font.fallbackFamilies: ["Noto Sans CJK JP", "Noto Color Emoji"]
}
```

**効果**: フォールバックフォントをシステム任せにせず、メトリクスが近いフォントを
明示的に指定することで、ベースラインのジャンプを最小化する。

**制限**: 全言語・全文字種への対応が必要で、フォントの可用性に依存する。

### 対策4: 固定高さ + verticalAlignment の設定

```qml
TextField {
    height: 48  // 十分な高さを確保
    verticalAlignment: TextInput.AlignVCenter
    topPadding: 0
    bottomPadding: 0
}
```

**効果**: アイテムの高さをフォールバック発生時のメトリクス変動を吸収できるサイズに固定し、
垂直中央揃えにすることでジャンプの視覚的影響を軽減する。

**制限**: 根本解決ではなく、ジャンプ幅が半減するのみ。

### 対策5: Qt コア (qtbase/qtdeclarative) 側の修正 (推奨)

根本解決には Qt のテキストレイアウトエンジン側の修正が必要:

**案A**: `QQuickTextInput` で行メトリクスではなく、想定されうる最大メトリクスを
事前に計算してベースラインに使用する

**案B**: `QTextLine` のメトリクスが変動した際に `baselineOffset` を再計算するのではなく、
プライマリフォントの ascent を常に基準として使用する（現在の `updatePaintNode()` の
補正ロジック `lineAt(0).ascent() - fm.ascent()` を `updateBaselineOffset()` にも反映する）

**案C**: `QTextEngine::shapeText()` でフォールバックフォントのメトリクスを
プライマリフォントのメトリクスにスケーリングするオプションを追加する

これらの修正は `qtbase` および `qtdeclarative` リポジトリへの変更が必要であり、
`qtvirtualkeyboard` 単体では対応できない。

### 対策6: Virtual Keyboard 側の緩和策

`qtvirtualkeyboard` リポジトリ内で可能な緩和策として:

- プリエディットテキスト送出時のフォーマット属性で、プリエディット文字のフォントを
  プライマリフォントと同じメトリクスを持つフォントに強制する
  （ただし `QInputMethodEvent::Attribute` の `TextFormat` では
  フォントファミリーの指定に制限がある）

## 結論

この問題は Qt のテキストレイアウトエンジン (`QTextEngine` / `QTextLayout`) の
「行内フォントメトリクスの max 集約」設計に起因する構造的な問題である。

`qtvirtualkeyboard` リポジトリ単体での根本解決は困難であり、アプリケーション側での
対策（対策1〜4）または Qt コア側の修正（対策5）が必要。

Qt 6.8 以降では `contextFontMerging` と `preferTypoLineMetrics` が利用可能であり、
本リポジトリ (Qt 6.12.0-alpha1) ではこれらの機能を活用できる。

## 対象バージョン

- 本リポジトリ: Qt 6.12.0-alpha1
- 影響範囲: Qt 5.x〜6.x 全般（フォントフォールバックが発生する全環境）
- 緩和策利用可能: Qt 6.8+ (`contextFontMerging`, `preferTypoLineMetrics`)
