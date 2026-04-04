# Qt フォント管理ガイド - 入門編

本ドキュメントはQtフレームワークにおけるフォントの基本概念と、独自フォントを追加する最も簡単な方法を解説します。

## 目次

1. [はじめに](#はじめに)
2. [Qtのフォントシステムとは](#qtのフォントシステムとは)
3. [サポートされるフォント形式](#サポートされるフォント形式)
4. [重要な用語を理解する](#重要な用語を理解する)
5. [QMLでフォントを使う基本](#qmlでフォントを使う基本)
6. [独自フォントを追加する（最も簡単な方法）](#独自フォントを追加する最も簡単な方法)
7. [よく使うフォントプロパティ](#よく使うフォントプロパティ)
8. [次のステップ](#次のステップ)

---

## はじめに

### このドキュメントの対象者

- Qtでフォントを扱うのが初めての方
- 独自フォントをアプリケーションに追加したい方
- フォント設定の基本を理解したい方

### 前提知識

- QMLの基本的な文法
- Qtプロジェクトの基本的な構成

---

## Qtのフォントシステムとは

Qtは、Windows、macOS、Linux、組み込みシステムなど、様々なプラットフォームで動作するアプリケーションを開発できるフレームワークです。各プラットフォームにはそれぞれ独自のフォントシステムがありますが、Qtはこれらを抽象化し、統一された方法でフォントを扱えるようにしています。

```
┌─────────────────────────────────────────┐
│         あなたのアプリケーション          │
│                                         │
│    Text { font.family: "Arial" }        │
└────────────────┬────────────────────────┘
                 │
                 ▼
┌─────────────────────────────────────────┐
│            Qt フォントエンジン            │
│  （プラットフォームの違いを吸収）          │
└────────────────┬────────────────────────┘
                 │
        ┌────────┼────────┐
        ▼        ▼        ▼
   ┌────────┐┌────────┐┌────────┐
   │Windows ││ macOS  ││ Linux  │
   └────────┘└────────┘└────────┘
```

### フォントを指定する2つの方法

| 方法 | 説明 | 主な使用場面 |
|------|------|-------------|
| システムフォント | OSにインストールされているフォントを使用 | 一般的なテキスト表示 |
| 独自フォント | アプリケーションにフォントファイルを同梱 | ブランド統一、特殊なデザイン |

---

## サポートされるフォント形式

Qtは以下のフォント形式をサポートしています：

| 形式 | 拡張子 | 説明 | 推奨度 |
|------|--------|------|--------|
| TrueType | `.ttf` | 最も広くサポートされる標準形式 | ★★★ |
| OpenType | `.otf` | 高度なタイポグラフィ機能をサポート | ★★★ |
| TrueType Collection | `.ttc` | 複数フォントを1ファイルに格納 | ★★☆ |

**初心者へのアドバイス**: 迷ったら `.ttf` 形式を選んでください。最も互換性が高く、問題が起きにくいです。

---

## 重要な用語を理解する

Qtのフォント管理を理解するために、以下の4つの用語を押さえておきましょう。

### 用語の関係図

```
開発者のリクエスト
       │
       ▼
┌──────────────────────────────────────────────────────────────┐
│                    Font Matching                             │
│                 （フォントマッチング）                         │
│                                                              │
│  ① Font Family で探す                                        │
│       │                                                      │
│       ├─→ 見つかった → そのフォントを使用                      │
│       │                                                      │
│       └─→ 見つからない                                        │
│              │                                               │
│              ▼                                               │
│  ② Style Hint を参考に代替を探す                              │
│       │                                                      │
│       ├─→ SansSerif → ゴシック系から選択                      │
│       ├─→ Serif → 明朝系から選択                              │
│       └─→ Monospace → 等幅フォントから選択                    │
│              │                                               │
│              ▼                                               │
│  ③ Fallback リストを順に試す                                  │
│       │                                                      │
│       └─→ 最終的に何かしらのフォントを決定                     │
└──────────────────────────────────────────────────────────────┘
       │
       ▼
   実際の描画
```

### 1. Font Family（フォントファミリー）

**フォントの「名前」や「グループ」のことです。**

同じデザインコンセプトで作られたフォントのまとまりを指します。

```qml
Text {
    font.family: "Noto Sans JP"  // ← これがフォントファミリー名
}
```

一つのファミリーには複数のスタイル（Regular, Bold, Italic等）が含まれることがあります：

| ファミリー名 | 含まれるスタイル例 |
|-------------|-------------------|
| Noto Sans JP | Regular, Bold, Light, Medium, ... |
| Arial | Regular, Bold, Italic, Bold Italic |

### 2. Style Hint（スタイルヒント）

**フォントの「カテゴリ」を示すヒントです。**

指定したフォントが見つからない場合に、「せめてこの系統のフォントを使って」とQtに伝えるためのものです。

| Style Hint | 意味 | 該当フォント例 |
|------------|------|---------------|
| `SansSerif` | ゴシック体系（飾りなし） | Arial, Helvetica, Noto Sans |
| `Serif` | 明朝体系（飾りあり） | Times New Roman, Noto Serif |
| `Monospace` | 等幅フォント | Courier, Consolas, Source Code Pro |
| `Decorative` | 装飾的なフォント | Comic Sans, Impact |

```cpp
// C++での設定例
QFont font;
font.setFamily("存在しないフォント");
font.setStyleHint(QFont::SansSerif);  // 見つからなければゴシック系を使って
```

### 3. Fallback（フォールバック）

**「代替」という意味で、2種類あります。**

#### ① ファミリーフォールバック（Family Fallback）

指定したフォントが見つからない場合に使う代替フォントのリストです。

```cpp
// Qt 5.13以降
QFont font;
font.setFamilies({"第1候補", "第2候補", "第3候補"});
// 第1候補がなければ第2候補、それもなければ第3候補...
```

#### ② グリフフォールバック（Glyph Fallback / Font Merging）

フォントは見つかったが、**表示したい文字（グリフ）がそのフォントに存在しない**場合に、別のフォントから文字を借りてくる仕組みです。

```
例: "Hello世界" を Arial で表示しようとした場合

"Hello" → Arial に含まれる → Arial で描画
"世界"  → Arial に含まれない → 日本語フォントから借りて描画
```

この「借りてくる」処理はQtが自動的に行います（Font Merging）。

### 4. Font Matching（フォントマッチング）

**開発者のリクエストから実際に使うフォントを決定するプロセス全体のことです。**

上の関係図で示したように、Font Family → Style Hint → Fallback の順で検索し、最終的に使用するフォントを決定します。

### 用語の使い分けまとめ

| 用語 | 一言で言うと | 使う場面 |
|------|-------------|---------|
| Font Family | フォントの名前 | 「このフォントを使いたい」 |
| Style Hint | カテゴリのヒント | 「見つからなければこの系統で」 |
| Fallback | 代替フォント/処理 | 「これがダメならこれ」 |
| Font Matching | 最適フォントを決める処理 | Qt内部で自動実行 |

---

## QMLでフォントを使う基本

### システムフォントを使う場合

最もシンプルな方法は、システムにインストールされているフォントを直接指定することです：

```qml
import QtQuick

Text {
    text: "Hello, World!"
    font.family: "Arial"      // フォント名
    font.pixelSize: 24        // サイズ（ピクセル）
}
```

### よく使われるシステムフォント

| プラットフォーム | よく使われるフォント |
|-----------------|---------------------|
| Windows | Arial, Segoe UI, MS Gothic |
| macOS | Helvetica, San Francisco |
| Linux | DejaVu Sans, Noto Sans |

**注意**: システムフォントはプラットフォームによって利用可能なフォントが異なります。指定したフォントがない場合、Qtは自動的に似たフォントを選択します。

---

## 独自フォントを追加する（最も簡単な方法）

### 手順概要

1. フォントファイルを用意する
2. プロジェクトにフォントファイルを配置する
3. QMLでFontLoaderを使ってロードする
4. Textコンポーネントで使用する

### Step 1: フォントファイルの準備

まず、使用したいフォントファイル（`.ttf` または `.otf`）を用意します。

**フォントの入手先例**:
- [Google Fonts](https://fonts.google.com/) - 無料でオープンソース
- [Adobe Fonts](https://fonts.adobe.com/) - 商用利用可能（ライセンス確認要）

### Step 2: プロジェクトへの配置

フォントファイルをプロジェクト内に配置します：

```
my-project/
├── main.qml
├── fonts/
│   └── MyCustomFont.ttf    ← ここに配置
└── CMakeLists.txt
```

### Step 3: FontLoaderでロード

QMLファイルで `FontLoader` を使ってフォントをロードします：

```qml
import QtQuick

Item {
    // フォントをロード
    FontLoader {
        id: myFont
        source: "fonts/MyCustomFont.ttf"
    }

    // ロードしたフォントを使用
    Text {
        text: "カスタムフォントで表示！"
        font.family: myFont.font.family
        font.pixelSize: 32
    }
}
```

### Step 4: 動作確認

アプリケーションを実行して、フォントが正しく表示されることを確認します。

### 完全なサンプルコード

```qml
import QtQuick
import QtQuick.Window

Window {
    width: 400
    height: 300
    visible: true
    title: "カスタムフォントのサンプル"

    // カスタムフォントをロード
    FontLoader {
        id: customFont
        source: "fonts/NotoSansJP-Regular.ttf"

        // ロード状態を確認（デバッグ用）
        onStatusChanged: {
            if (status === FontLoader.Ready) {
                console.log("フォントロード成功:", font.family)
            } else if (status === FontLoader.Error) {
                console.log("フォントロード失敗")
            }
        }
    }

    Column {
        anchors.centerIn: parent
        spacing: 20

        Text {
            text: "システムフォント (Arial)"
            font.family: "Arial"
            font.pixelSize: 24
        }

        Text {
            text: "カスタムフォント"
            font.family: customFont.font.family
            font.pixelSize: 24
        }
    }
}
```

---

## よく使うフォントプロパティ

### 基本プロパティ

| プロパティ | 説明 | 例 |
|-----------|------|-----|
| `font.family` | フォント名 | `"Arial"` |
| `font.pixelSize` | サイズ（ピクセル単位） | `24` |
| `font.pointSize` | サイズ（ポイント単位） | `12` |
| `font.bold` | 太字にする | `true` / `false` |
| `font.italic` | 斜体にする | `true` / `false` |

### pixelSize vs pointSize

| プロパティ | 説明 | 推奨場面 |
|-----------|------|---------|
| `pixelSize` | 画面上のピクセル数で指定 | UI部品、固定サイズが必要な場合 |
| `pointSize` | 印刷単位（1pt = 1/72インチ）で指定 | ドキュメント、DPI考慮が必要な場合 |

**初心者へのアドバイス**: UIを作る場合は `pixelSize` を使うことをお勧めします。挙動が予測しやすく、デバッグが容易です。

### 使用例

```qml
// 基本的な使い方
Text {
    text: "Normal Text"
    font.family: "Arial"
    font.pixelSize: 16
}

// 太字
Text {
    text: "Bold Text"
    font.family: "Arial"
    font.pixelSize: 16
    font.bold: true
}

// 斜体
Text {
    text: "Italic Text"
    font.family: "Arial"
    font.pixelSize: 16
    font.italic: true
}

// 組み合わせ
Text {
    text: "Bold Italic Text"
    font.family: "Arial"
    font.pixelSize: 16
    font.bold: true
    font.italic: true
}
```

---

## FontLoaderのステータス

FontLoaderはフォントの読み込み状態を `status` プロパティで教えてくれます：

| ステータス | 意味 |
|-----------|------|
| `FontLoader.Null` | ソースが設定されていない |
| `FontLoader.Loading` | 読み込み中 |
| `FontLoader.Ready` | 読み込み完了、使用可能 |
| `FontLoader.Error` | 読み込み失敗 |

### エラーハンドリングの例

```qml
FontLoader {
    id: myFont
    source: "fonts/MyFont.ttf"
}

Text {
    text: "サンプルテキスト"

    // フォントが読み込めた場合はカスタムフォント、
    // 失敗した場合はArialを使用
    font.family: myFont.status === FontLoader.Ready
        ? myFont.font.family
        : "Arial"

    font.pixelSize: 24
}
```

---

## よくある間違いと解決法

### 1. フォントファイルが見つからない

**症状**: フォントが適用されず、デフォルトフォントで表示される

**原因**: パスが間違っている

```qml
// ❌ 間違い: パスが間違っている
FontLoader {
    source: "MyFont.ttf"  // fontsフォルダを指定していない
}

// ✅ 正しい: 正確なパスを指定
FontLoader {
    source: "fonts/MyFont.ttf"
}
```

### 2. フォント名の指定ミス

**症状**: FontLoaderは成功しているが、Textに適用されない

**原因**: `font.family` に誤ったフォント名を指定している

```qml
FontLoader {
    id: myFont
    source: "fonts/NotoSansJP-Regular.ttf"
}

// ❌ 間違い: ファイル名を指定している
Text {
    font.family: "NotoSansJP-Regular"
}

// ✅ 正しい: FontLoaderから取得した名前を使う
Text {
    font.family: myFont.font.family
}
```

### 3. フォントファイルの形式が非対応

**症状**: FontLoaderのステータスがErrorになる

**確認事項**:
- ファイル形式が `.ttf`、`.otf`、`.ttc` のいずれかであること
- ファイルが破損していないこと

---

## 次のステップ

入門編の内容を理解したら、次は **スタンダード編** に進んでください。

スタンダード編では以下を学べます：
- Qtリソースシステムを使った本格的なフォント管理
- C++でのフォント追加方法
- フォールバック（代替フォント）の設定
- プロジェクト構成のベストプラクティス

---

## 参考リンク

- [FontLoader QML Type | Qt Quick](https://doc.qt.io/qt-6/qml-qtquick-fontloader.html)
- [Text QML Type | Qt Quick](https://doc.qt.io/qt-6/qml-qtquick-text.html)
- [Google Fonts](https://fonts.google.com/)
