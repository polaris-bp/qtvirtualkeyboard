# Qt Virtual Keyboard 5.12.10 ドキュメント

このディレクトリには、Qt Virtual Keyboard 5.12.10に関するドキュメントが格納されています。

---

## 📁 ディレクトリ構成

```
docs/
├── README.md                    # このファイル
├── original/                    # Qt VKB 5.12.10の元々の仕様ドキュメント
│   ├── QVK_5.12_ARCHITECTURE_OVERVIEW.md
│   ├── QVK_5.12_COMPONENT_SPECIFICATIONS.md
│   ├── QVK_5.12_DATA_FLOW_SEQUENCE.md
│   ├── QVK_5.12_STYLING_GUIDE.md
│   └── QVK_5.12_LAYOUT_GUIDE.md
└── custom/                      # カスタムオーバーレイキーボードの改造仕様書
    ├── DESIGN_CUSTOM_KEYBOARD_OVERLAY_5.12.md
    └── IMPLEMENTATION_SPEC_CUSTOM_OVERLAY_KEYBOARD.md
```

---

## 📚 original/ - Qt VKB 5.12.10 元々の仕様ドキュメント

Qt Virtual Keyboard 5.12.10の実際のソースコードを精査してドキュメント化した、元々の仕様書です。

### 📖 [QVK_5.12_ARCHITECTURE_OVERVIEW.md](original/QVK_5.12_ARCHITECTURE_OVERVIEW.md)

**概要**: システム全体のアーキテクチャ概要

**内容**:
- システム概要
- 全体アーキテクチャ（MVC設計パターン）
- モジュール構成
- コンポーネント階層
- ビルドシステム（qmake）
- 設計原則とパターン

**対象読者**: システム設計者、開発者

**ページ数**: 約600行

---

### 📖 [QVK_5.12_COMPONENT_SPECIFICATIONS.md](original/QVK_5.12_COMPONENT_SPECIFICATIONS.md)

**概要**: 各コンポーネントの詳細仕様

**内容**:
- **QMLコンポーネント**:
  - InputPanel, Keyboard, KeyboardLayout/KeyboardRow
  - BaseKey, 各種キー型（EnterKey, BackspaceKey等）
  - ShadowInputControl, SelectionControl

- **C++クラス**:
  - QVirtualKeyboardInputContext（プロパティ、メソッド、シグナル）
  - QVirtualKeyboardInputEngine（入力モード、キーイベント処理）
  - Settings (QtVirtualKeyboard::Settings)
  - AbstractInputMethod（入力メソッドプラグイン基底クラス）

**対象読者**: 開発者、カスタマイズ実装者

**ページ数**: 約1,300行

**特徴**: 実際のソースコードと照合済み、完全なAPIリファレンス

---

### 📖 [QVK_5.12_DATA_FLOW_SEQUENCE.md](original/QVK_5.12_DATA_FLOW_SEQUENCE.md)

**概要**: データフローとシーケンス図

**内容**:
- 入力処理フロー（キー入力の全体フロー）
- PreeditとCommitの違い
- 候補選択フロー
- シーケンス図:
  - 通常の文字入力
  - 予測変換入力
  - フルスクリーンモード
  - 手書き入力
- 状態遷移
- 通信パターン

**対象読者**: 開発者、システムインテグレーター

**ページ数**: 約1,100行

**特徴**: ASCII図によるビジュアル説明

---

### 📖 [QVK_5.12_STYLING_GUIDE.md](original/QVK_5.12_STYLING_GUIDE.md)

**概要**: スタイリングシステムガイド

**内容**:
- スタイリングシステム概要（Component Delegateパターン）
- KeyboardStyleの構造:
  - 設計サイズとscaleHint
  - マージン設定
  - Component Delegateプロパティ
  - 制御オブジェクト (control)
- カスタムスタイルの作成:
  - 基本テンプレート
  - ステップバイステップガイド
  - 実装例（複数パターン）
- ベストプラクティス

**対象読者**: UI/UXデザイナー、フロントエンド開発者

**ページ数**: 約480行

---

### 📖 [QVK_5.12_LAYOUT_GUIDE.md](original/QVK_5.12_LAYOUT_GUIDE.md)

**概要**: レイアウトシステムガイド

**内容**:
- レイアウトシステム概要
- レイアウトの種類（main, symbols, numbers, digits, dialpad, handwriting）
- レイアウトディレクトリ構造
- レイアウトの検索順序（.fallbackマーカー）
- レイアウトの構造:
  - KeyboardLayout/KeyboardRowプロパティ
  - Weight（重み）システム
  - キー種別
- カスタムレイアウトの作成:
  - 新しい言語レイアウトの追加
  - ステップバイステップガイド
  - 完全なサンプルコード
- 多言語対応
- 高度な機能

**対象読者**: 開発者、多言語対応実装者

**ページ数**: 約490行

---

## 🔧 custom/ - カスタムオーバーレイキーボード改造仕様書

要件「①キーボード立ち上がり時キーボードで画面全体を覆う、②キーボードの上半分はキーボードに内包された入力フィールドとなる、③入力を確定後、エンターによって入力内容が覆う前の入力フィールドに反映される」を実装するための改造仕様書です。

### 📖 [DESIGN_CUSTOM_KEYBOARD_OVERLAY_5.12.md](custom/DESIGN_CUSTOM_KEYBOARD_OVERLAY_5.12.md)

**概要**: カスタムオーバーレイキーボードの初期設計書

**内容**:
- 要件定義
- 設計アプローチ
- コンポーネント設計
- 実装方針（4フェーズ）
- 既存機能との統合
- サンプルコード（QML）

**作成日**: 2026-01-10 (初版)

**ページ数**: 約730行

**特徴**: 初期構想、コンセプト設計

---

### 📖 [IMPLEMENTATION_SPEC_CUSTOM_OVERLAY_KEYBOARD.md](custom/IMPLEMENTATION_SPEC_CUSTOM_OVERLAY_KEYBOARD.md)

**概要**: カスタムオーバーレイキーボードの実装仕様書（詳細版）

**内容**:
- 要件定義（機能要件・非機能要件）
- 設計方針（既存機能の最大活用）
- 既存機能の活用:
  - ShadowInputContext
  - ShadowInputControl.qml
  - VirtualKeyboardSettings
- アーキテクチャ設計:
  - コンポーネント構成
  - 実装パターン（2案比較）
  - モード切替設計
  - データフロー
- **コンポーネント仕様**（完全なQMLコード）:
  - CustomOverlayKeyboard.qml（新規）
  - CustomInputField.qml（新規）
  - InputPanel.qml（既存・改造）
  - Keyboard.qml（既存・微調整）
  - EnterKey.qml（既存・改造）
- **実装手順**（3フェーズ）:
  - フェーズ1: 基本実装（2-3日）
  - フェーズ2: テスト・調整（1-2日）
  - フェーズ3: 最適化・統合（1日）
- テスト計画:
  - 単体テスト
  - 統合テスト
  - 受け入れテスト
- リスクと対策
- 実装上の注意事項
- 実装完了基準

**作成日**: 2026-01-10

**ページ数**: 約890行

**特徴**:
- 実装可能な詳細レベル
- Qt VKB 5.12.10のソースコード分析に基づく
- 完全なコンポーネント実装コード付き
- ステップバイステップ実装ガイド

---

## 📊 ドキュメント作成プロセス

### 1. ソースコード精査

Qt Virtual Keyboard 5.12.10の実際のソースコードを読み込み、以下を確認：

- **C++ヘッダーファイル**:
  - `qvirtualkeyboardinputcontext.h` (lines 48-132)
  - `qvirtualkeyboardinputcontext_p.h` (lines 59-175)
  - `qvirtualkeyboardinputengine.h` (lines 45-177)
  - `settings_p.h` (lines 49-110)
  - `qvirtualkeyboardabstractinputmethod.h` (lines 40-90)

- **QMLファイル**:
  - `InputPanel.qml` (147行)
  - `Keyboard.qml` (1,621行)
  - `KeyboardLayout.qml` (148行)
  - `KeyboardRow.qml` (62行)
  - `BaseKey.qml` (250行)
  - `ShadowInputControl.qml` (138行)
  - `KeyboardStyle.qml`

- **ビルドシステム**:
  - `virtualkeyboard.pro` (qmake)

### 2. ドキュメント化

ソースコードの実際の実装を基に、5つの包括的なドキュメントを作成：
1. Architecture Overview
2. Component Specifications
3. Data Flow & Sequence
4. Styling Guide
5. Layout Guide

### 3. 改造仕様書作成

元々の仕様ドキュメントの知見を活用し、実装可能な改造仕様書を作成：
1. Design Document (初期設計)
2. Implementation Specification (詳細仕様)

---

## 🎯 使い方

### 新規開発者向け

1. **まず読むべきドキュメント**:
   - `original/QVK_5.12_ARCHITECTURE_OVERVIEW.md`
   - システム全体像を理解

2. **次に読むべきドキュメント**:
   - `original/QVK_5.12_COMPONENT_SPECIFICATIONS.md`
   - 各コンポーネントの詳細を理解

3. **実装時に参照**:
   - `original/QVK_5.12_DATA_FLOW_SEQUENCE.md`
   - データの流れを理解

### カスタマイズ実装者向け

- **スタイル変更**: `original/QVK_5.12_STYLING_GUIDE.md`
- **レイアウト変更**: `original/QVK_5.12_LAYOUT_GUIDE.md`
- **オーバーレイキーボード実装**: `custom/IMPLEMENTATION_SPEC_CUSTOM_OVERLAY_KEYBOARD.md`

---

## 📝 ドキュメント更新履歴

| 日付 | 変更内容 | 担当者 |
|------|---------|--------|
| 2026-01-10 | 初版作成 - Qt VKB 5.12.10ソースコード分析 | Claude |
| 2026-01-10 | Component Specifications精査・改善 | Claude |
| 2026-01-10 | カスタムオーバーレイキーボード改造仕様書作成 | Claude |
| 2026-01-10 | ドキュメント階層化 (original/ と custom/) | Claude |

---

## 📖 参考リンク

- [Qt Virtual Keyboard公式ドキュメント](https://doc.qt.io/qt-5/qtvirtualkeyboard-index.html)
- [Qt公式リポジトリ](https://github.com/qt/qtvirtualkeyboard)
- プロジェクトリポジトリ: polaris-bp/qtvirtualkeyboard

---

## 📧 お問い合わせ

ドキュメントに関する質問や改善提案は、プロジェクトのissueトラッカーまでお願いします。

---

**最終更新**: 2026-01-10
**バージョン**: 1.0
