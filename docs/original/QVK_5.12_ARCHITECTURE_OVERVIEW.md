# Qt Virtual Keyboard 5.12.10 - アーキテクチャ概要

**バージョン**: 5.12.10
**最終更新**: 2026-01-10
**対象読者**: システム設計者、開発者

---

## 目次

1. [システム概要](#1-システム概要)
2. [全体アーキテクチャ](#2-全体アーキテクチャ)
3. [モジュール構成](#3-モジュール構成)
4. [コンポーネント階層](#4-コンポーネント階層)
5. [ビルドシステム](#5-ビルドシステム)
6. [設計原則とパターン](#6-設計原則とパターン)

---

## 1. システム概要

### 1.1 Qt Virtual Keyboardとは

Qt Virtual Keyboard (QVK) は、タッチスクリーンデバイス向けの仮想キーボードソリューションです。Qtアプリケーションに統合可能で、以下の特徴を持ちます：

- **マルチプラットフォーム**: Linux、Windows、macOS、組み込みLinux、Android、iOS対応
- **多言語対応**: 40種類以上の言語レイアウト
- **カスタマイズ可能**: 完全なスタイル/レイアウトカスタマイズ
- **入力方式**: キーボード入力、手書き入力、予測変換
- **Qt統合**: Qtの標準入力メソッドフレームワークに統合

### 1.2 主要機能

| 機能 | 説明 |
|------|------|
| **通常モード** | 画面下部に配置されるキーボード |
| **フルスクリーンモード** | キーボード上部に入力フィールドを表示 |
| **手書き入力** | TraceInputAreaによる手書き認識 |
| **予測変換** | 入力メソッドプラグインによる候補表示 |
| **テキスト選択** | ドラッグ可能な選択ハンドル |
| **多言語切り替え** | ランタイムでの言語変更 |

### 1.3 技術スタック

```
┌─────────────────────────────────────────┐
│           アプリケーション              │
│         (QLineEdit, QTextEdit)          │
└─────────────────────────────────────────┘
                    ↕
┌─────────────────────────────────────────┐
│      Qt Input Method Framework          │
│      (QPlatformInputContext)            │
└─────────────────────────────────────────┘
                    ↕
┌─────────────────────────────────────────┐
│    QVirtualKeyboardInputContext (C++)   │
│    - 入力状態管理                        │
│    - イベント変換                        │
└─────────────────────────────────────────┘
                    ↕
┌─────────────────────────────────────────┐
│    QVirtualKeyboardInputEngine (C++)    │
│    - 入力メソッド管理                    │
│    - キーイベント処理                    │
└─────────────────────────────────────────┘
                    ↕
┌─────────────────────────────────────────┐
│       Input Method Plugins (C++)        │
│  (Hunspell, Pinyin, OpenWnn, etc.)      │
└─────────────────────────────────────────┘
        ↕                       ↕
┌──────────────────┐  ┌──────────────────┐
│   Keyboard UI    │  │  Layout Files    │
│     (QML)        │  │     (QML)        │
└──────────────────┘  └──────────────────┘
```

---

## 2. 全体アーキテクチャ

### 2.1 MVC設計パターン

Qt Virtual Keyboardは、Model-View-Controllerパターンを採用しています。

```
┌─────────────────────────────────────────────────────┐
│                     View Layer                      │
│  ┌─────────────┐  ┌──────────────┐  ┌────────────┐ │
│  │ InputPanel  │  │  Keyboard    │  │  Styles    │ │
│  │   (.qml)    │  │   (.qml)     │  │  (.qml)    │ │
│  └─────────────┘  └──────────────┘  └────────────┘ │
└─────────────────────────────────────────────────────┘
                          ↕
┌─────────────────────────────────────────────────────┐
│                  Controller Layer                   │
│  ┌──────────────────────────────────────────────┐   │
│  │   QVirtualKeyboardInputEngine                │   │
│  │   - virtualKeyPress/Release/Click()          │   │
│  │   - Input mode management                    │   │
│  │   - Input method routing                     │   │
│  └──────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────┘
                          ↕
┌─────────────────────────────────────────────────────┐
│                    Model Layer                      │
│  ┌──────────────────────────────────────────────┐   │
│  │   QVirtualKeyboardInputContext               │   │
│  │   - Preedit/Surrounding text                 │   │
│  │   - Cursor position                          │   │
│  │   - Shift/CapsLock state                     │   │
│  │   - Input method hints                       │   │
│  └──────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────┘
```

### 2.2 レイヤー責務

| レイヤー | 責務 | 実装技術 |
|---------|------|---------|
| **View** | UI表示、ユーザーインタラクション | QML (QtQuick 2.x) |
| **Controller** | 入力イベント処理、ルーティング | C++ (QVirtualKeyboardInputEngine) |
| **Model** | 入力状態管理、アプリケーション連携 | C++ (QVirtualKeyboardInputContext) |
| **Plugin** | 言語固有の入力処理 | C++ (AbstractInputMethod) |

### 2.3 データフロー概要

```
[ユーザータッチ]
    ↓
[MultiPointTouchArea (QML)]
    ↓
[InputEngine.virtualKeyPress()] ← C++
    ↓
[InputMethod.keyEvent()] ← プラグイン
    ↓
[InputContext.setPreeditText()] or [commit()]
    ↓
[QInputMethodEvent] → Qt Framework
    ↓
[アプリケーション (QLineEdit等)]
```

---

## 3. モジュール構成

### 3.1 ディレクトリ構造

```
qtvirtualkeyboard/
├── src/
│   ├── virtualkeyboard/          # コアライブラリ
│   │   ├── *.cpp, *.h            # C++実装
│   │   ├── content/              # QMLコンポーネント
│   │   │   ├── InputPanel.qml
│   │   │   ├── components/
│   │   │   │   ├── Keyboard.qml
│   │   │   │   ├── Key.qml
│   │   │   │   └── ...
│   │   │   ├── layouts/          # キーボードレイアウト
│   │   │   │   ├── fallback/
│   │   │   │   ├── en_GB/
│   │   │   │   ├── ja_JP/
│   │   │   │   └── ...
│   │   │   └── styles/           # スタイル定義
│   │   │       ├── default/
│   │   │       └── retro/
│   │   └── virtualkeyboard.pro   # qmakeビルド設定
│   │
│   ├── import/                   # QMLインポートモジュール
│   ├── settings/                 # 設定管理
│   ├── styles/                   # スタイルベースクラス
│   ├── plugin/                   # プラットフォーム統合
│   └── plugins/                  # 入力メソッドプラグイン
│       ├── hunspell/             # スペルチェック
│       ├── pinyin/               # 中国語拼音
│       ├── openwnn/              # 日本語
│       ├── hangul/               # 韓国語
│       ├── t9write/              # 手書き認識
│       └── ...
│
├── examples/                     # サンプルアプリケーション
├── tests/                        # ユニットテスト
└── dist/                         # 配布用ファイル
```

### 3.2 モジュール依存関係

```
                  ┌─────────────┐
                  │ Application │
                  └─────────────┘
                         ↓
                  ┌─────────────┐
                  │   plugin    │ ← プラットフォーム統合
                  └─────────────┘
                         ↓
          ┌──────────────┴──────────────┐
          ↓                             ↓
   ┌─────────────┐              ┌─────────────┐
   │   import    │              │  settings   │
   │ (QML module)│              │             │
   └─────────────┘              └─────────────┘
          ↓                             ↓
   ┌──────────────────────────────────────────┐
   │         virtualkeyboard (core)           │
   │  ┌────────┐  ┌─────────┐  ┌───────────┐ │
   │  │  C++   │  │   QML   │  │  Styles   │ │
   │  └────────┘  └─────────┘  └───────────┘ │
   └──────────────────────────────────────────┘
          ↓                             ↓
   ┌─────────────┐              ┌─────────────┐
   │   plugins   │              │   QtQuick   │
   │  (IME etc.) │              │   QtCore    │
   └─────────────┘              └─────────────┘
```

### 3.3 コアモジュール詳細

#### virtualkeyboard (コアライブラリ)

**ビルド設定**: `src/virtualkeyboard/virtualkeyboard.pro`

```qmake
TARGET = QtVirtualKeyboard
MODULE = virtualkeyboard
QT += qml quick gui gui-private core-private
CONFIG += qtquickcompiler
```

**主要クラス**:
- `QVirtualKeyboardInputContext`: 入力コンテキスト管理
- `QVirtualKeyboardInputEngine`: 入力エンジン
- `QVirtualKeyboardAbstractInputMethod`: 入力メソッド基底クラス
- `AbstractInputPanel`: 入力パネル抽象クラス
- `PlatformInputContext`: プラットフォーム統合

**QMLコンポーネント**: 34ファイル（リソースに埋め込み）

#### import (QMLインポートモジュール)

**役割**: QMLから`import QtQuick.VirtualKeyboard 2.3`で利用可能にする

**提供する型**:
- `InputPanel`: QML型としてのInputPanel
- `EnterKeyAction`: Attached type for enter key customization
- `VirtualKeyboardSettings`: 設定オブジェクト

#### settings (設定管理)

**クラス**: `QVirtualKeyboardSettings`

**主要プロパティ**:
```cpp
Q_PROPERTY(QString style READ style WRITE setStyle)
Q_PROPERTY(QString styleName READ styleName)
Q_PROPERTY(QString locale READ locale WRITE setLocale)
Q_PROPERTY(QStringList availableLocales READ availableLocales)
Q_PROPERTY(QStringList activeLocales READ activeLocales WRITE setActiveLocales)
Q_PROPERTY(bool fullScreenMode READ fullScreenMode WRITE setFullScreenMode)
Q_PROPERTY(QString layoutPath READ layoutPath)
```

#### plugins (入力メソッドプラグイン)

各プラグインは`QVirtualKeyboardAbstractInputMethod`を継承:

| プラグイン | 言語 | 機能 |
|----------|------|------|
| **hunspell** | 欧州言語 | スペルチェック、予測変換 |
| **pinyin** | 中国語 | 拼音入力 |
| **tcime** | 繁体字中国語 | 注音、倉頡 |
| **openwnn** | 日本語 | かな漢字変換 |
| **hangul** | 韓国語 | ハングル入力 |
| **t9write** | 多言語 | 手書き認識 (商用) |
| **lipi-toolkit** | 多言語 | 手書き認識 (OSS) |
| **myscript** | 多言語 | 手書き認識 (商用) |

---

## 4. コンポーネント階層

### 4.1 QML階層構造

```
InputPanel.qml (最上位)
├── SelectionControl.qml
│   ├── Loader (anchorHandle)
│   └── Loader (cursorHandle)
│
└── Keyboard.qml
    ├── Rectangle (keyboardBackground)
    │
    ├── ListView (wordCandidateView)
    │   └── Delegates (単語候補)
    │
    ├── ShadowInputControl.qml (フルスクリーンモード)
    │   └── Flickable
    │       └── TextInput (shadowInput)
    │
    ├── MultiPointTouchArea (keyboardInputArea)
    │   └── タッチイベント処理
    │
    ├── Loader (keyboardLayoutLoader)
    │   └── KeyboardLayout.qml
    │       └── ColumnLayout
    │           ├── KeyboardRow × N
    │           │   └── RowLayout
    │           │       ├── Key × N
    │           │       ├── BackspaceKey
    │           │       ├── EnterKey
    │           │       ├── ShiftKey
    │           │       └── SpaceKey
    │           └── ...
    │
    ├── AlternativeKeys.qml (長押しポップアップ)
    ├── CharacterPreviewBubble.qml
    └── NavigationHighlight (キーボードナビゲーション)
```

### 4.2 キーコンポーネントの役割

| コンポーネント | ファイル | 役割 |
|--------------|---------|------|
| **InputPanel** | InputPanel.qml | アプリケーション統合、公開API |
| **Keyboard** | Keyboard.qml | キーボードロジック、状態管理 |
| **KeyboardLayout** | KeyboardLayout.qml | レイアウトコンテナ (ColumnLayout) |
| **KeyboardRow** | KeyboardRow.qml | キー行コンテナ (RowLayout) |
| **BaseKey** | BaseKey.qml | 全キーの基底クラス |
| **Key** | Key.qml | 標準文字キー |
| **EnterKey** | EnterKey.qml | Enterキー (動的ラベル) |
| **BackspaceKey** | BackspaceKey.qml | バックスペースキー |
| **ShiftKey** | ShiftKey.qml | Shiftキー (大文字切替) |
| **SpaceKey** | SpaceKey.qml | スペースキー (言語表示) |
| **ShadowInputControl** | ShadowInputControl.qml | フルスクリーン入力エリア |
| **SelectionControl** | SelectionControl.qml | テキスト選択ハンドル |
| **AlternativeKeys** | AlternativeKeys.qml | 長押し代替キーポップアップ |

### 4.3 C++クラス階層

```
QObject
├── QVirtualKeyboardInputContext
│   └── 入力状態管理、アプリケーション連携
│
├── QVirtualKeyboardInputEngine
│   └── キーイベント処理、入力メソッド管理
│
├── QVirtualKeyboardSettings
│   └── グローバル設定
│
├── AbstractInputPanel
│   ├── AppInputPanel (モバイル/組み込み)
│   └── DesktopInputPanel (デスクトップ)
│
├── QVirtualKeyboardAbstractInputMethod
│   ├── PlainInputMethod (デフォルト)
│   ├── HunspellInputMethod
│   ├── PinyinInputMethod
│   ├── OpenWnnInputMethod
│   └── ... (各種プラグイン)
│
└── PlatformInputContext (QPlatformInputContext継承)
    └── Qtフレームワークとの統合
```

---

## 5. ビルドシステム

### 5.1 qmake構成

Qt 5.12.10ではqmakeを使用します。

**トップレベル**: `qtvirtualkeyboard.pro`
```qmake
TEMPLATE = subdirs
CONFIG += ordered
SUBDIRS = src examples tests
```

**モジュールレベル**: `src/src.pro`
```qmake
TEMPLATE = subdirs
SUBDIRS = virtualkeyboard import settings styles plugin plugins
```

**ライブラリレベル**: `src/virtualkeyboard/virtualkeyboard.pro`

主要な設定:
```qmake
# モジュール定義
TARGET = QtVirtualKeyboard
MODULE = virtualkeyboard

# Qt依存
QT += qml quick gui gui-private core-private

# コンパイラ最適化
CONFIG += qtquickcompiler

# リソース
RESOURCES += \
    virtualkeyboard_content.qrc \
    virtualkeyboard_layouts.qrc \
    virtualkeyboard_default_style.qrc

# 条件付きコンパイル
contains(CONFIG, lang-ja.*): DEFINES += HAVE_JA_LAYOUT
```

### 5.2 リソース管理 (.qrc)

**virtualkeyboard_content.qrc** (QMLコンポーネント):
```xml
<RCC version="1.0">
<qresource prefix="/QtQuick/VirtualKeyboard/content">
    <file>InputPanel.qml</file>
    <file>components/Keyboard.qml</file>
    <file>components/Key.qml</file>
    <!-- 34ファイル -->
</qresource>
</RCC>
```

**virtualkeyboard_layouts.qrc** (レイアウトファイル):
```xml
<qresource prefix="/QtQuick/VirtualKeyboard/content/layouts">
    <file>fallback/main.qml</file>
    <file>en_GB/main.qml</file>
    <file>ja_JP/main.qml</file>
    <!-- 200+ファイル -->
</qresource>
```

リソースは実行ファイルに埋め込まれ、`qrc:/`パスでアクセス可能。

### 5.3 言語サポートのコンパイルオプション

```qmake
# CONFIG変数で制御
CONFIG += lang-en_GB    # 英語（イギリス）
CONFIG += lang-ja_JP    # 日本語
CONFIG += lang-zh_CN    # 中国語（簡体字）
CONFIG += lang-all      # 全言語

# 対応するDEFINEが設定される
contains(CONFIG, lang-en.*): DEFINES += HAVE_EN_LAYOUT
```

これにより、不要な言語リソースを除外してバイナリサイズを削減可能。

### 5.4 プラグインのビルド

入力メソッドプラグインは動的ライブラリとしてビルド:

```qmake
# src/plugins/hunspell/plugin/plugin.pro
TEMPLATE = lib
CONFIG += plugin
TARGET = qtvirtualkeyboard_hunspell

SOURCES += hunspellinputmethod_plugin.cpp
```

配置先: `$QTDIR/plugins/virtualkeyboard/`

---

## 6. 設計原則とパターン

### 6.1 設計原則

#### 1. 関心の分離 (Separation of Concerns)

- **UI (QML)**: ビジュアル表現とインタラクション
- **ロジック (C++)**: 入力処理とビジネスロジック
- **スタイル (QML Component)**: 見た目のカスタマイズ
- **レイアウト (QML)**: キー配置定義

#### 2. 拡張性 (Extensibility)

- **プラグインアーキテクチャ**: 新しい入力メソッドを追加可能
- **スタイルシステム**: Component delegateでカスタマイズ
- **レイアウトシステム**: QMLファイルで新言語追加

#### 3. 再利用性 (Reusability)

- **BaseKey**: すべてのキー型の基底クラス
- **KeyboardLayout**: レイアウト記述の共通基盤
- **AbstractInputMethod**: 入力メソッドの共通インターフェース

### 6.2 適用デザインパターン

#### MVC (Model-View-Controller)

```
View (QML)
    ↕
Controller (InputEngine)
    ↕
Model (InputContext)
```

#### Plugin Pattern

```cpp
class QVirtualKeyboardAbstractInputMethod {
    virtual bool keyEvent(...) = 0;
    virtual QList<InputMode> inputModes(...) = 0;
};

// プラグインが実装
class HunspellInputMethod : public QVirtualKeyboardAbstractInputMethod {
    bool keyEvent(...) override { /* 実装 */ }
};
```

#### Delegate Pattern

スタイルシステムでComponent delegateを使用:

```qml
KeyboardStyle {
    property Component keyPanel: KeyPanel {
        // カスタムUI実装
    }
}
```

#### Observer Pattern

Qt SignalsとSlotsで実装:

```cpp
class QVirtualKeyboardInputContext : public QObject {
signals:
    void shiftActiveChanged();
    void cursorPositionChanged();
};
```

QMLから監視:
```qml
Connections {
    target: InputContext
    onCursorPositionChanged: { /* 処理 */ }
}
```

#### Singleton Pattern

設定とInputContextはシングルトン:

```qml
// QMLからグローバルアクセス
VirtualKeyboardSettings.fullScreenMode = true
InputContext.commit("text")
```

### 6.3 コーディング規約

#### C++

- **命名**: Qt規約に従う
  - クラス: `QVirtualKeyboard*`
  - プライベートヘッダー: `*_p.h`
  - メンバー変数: `m_variableName`

- **プロパティ**: Q_PROPERTYマクロで公開
  ```cpp
  Q_PROPERTY(bool shift READ isShiftActive NOTIFY shiftActiveChanged)
  ```

- **シグナル**: 状態変更時に発行
  ```cpp
  emit shiftActiveChanged();
  ```

#### QML

- **命名**: camelCase
  ```qml
  property bool fullScreenMode: false
  ```

- **オブジェクトID**: 簡潔な名前
  ```qml
  Keyboard { id: keyboard }
  ```

- **インポート**: 必要最小限
  ```qml
  import QtQuick 2.0
  import QtQuick.VirtualKeyboard 2.3
  ```

---

## 補足資料

### A. 主要ファイル一覧

| ファイル | 行数 | 説明 |
|---------|-----|------|
| Keyboard.qml | 1621 | メインキーボードロジック |
| KeyboardStyle.qml | 531 | スタイルベースクラス |
| qvirtualkeyboardinputcontext.cpp | 1500+ | 入力コンテキスト実装 |
| qvirtualkeyboardinputengine.cpp | 800+ | 入力エンジン実装 |
| BaseKey.qml | 250 | キー基底クラス |
| InputPanel.qml | 147 | トップレベルパネル |

### B. 主要Qt依存

```qmake
QT += qml          # QML engine
QT += quick        # QtQuick 2.x
QT += gui          # GUI基盤
QT += gui-private  # 内部API (QPlatformInputContext)
QT += core-private # 内部API
```

### C. ビルド成果物

```
$QTDIR/
├── lib/
│   └── libQt5VirtualKeyboard.so (or .dll/.dylib)
├── qml/QtQuick/VirtualKeyboard/
│   ├── libdeclarative_qtvirtualkeyboard.so
│   ├── qmldir
│   └── plugins.qmltypes
└── plugins/virtualkeyboard/
    ├── libqtvirtualkeyboard_hunspell.so
    ├── libqtvirtualkeyboard_pinyin.so
    └── ...
```

---

**このドキュメントは、Qt Virtual Keyboard 5.12.10の全体像を理解するための概要ガイドです。**
**詳細な実装については、各コンポーネントの仕様書を参照してください。**
