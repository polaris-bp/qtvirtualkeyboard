# Qt フォント管理ドキュメント

Qtフレームワークにおけるフォント管理の仕組みと、独自フォントを追加するための実践的なガイドです。

## 対象バージョン

本ドキュメントは **Qt 6（主に6.5以降）** を対象として記述されています。

Qt 5.12/5.13を使用する場合は、[04. バージョン別差分ガイド](./04-version-differences.md)を参照してください。

## ドキュメント構成

本ドキュメントは3つのレベルに分かれています。ご自身のスキルレベルと目的に合わせてお読みください。

### [01. 入門編](./01-beginner.md)

**対象者**: Qtでフォントを扱うのが初めての方

**内容**:
- Qtのフォントシステムとは
- サポートされるフォント形式（TTF, OTF, TTC）
- QMLでフォントを使う基本
- FontLoaderの基本的な使い方
- よく使うフォントプロパティ
- よくある間違いと解決法

### [02. スタンダード編](./02-standard.md)

**対象者**: 実際のプロジェクトでフォントを管理したい方

**内容**:
- Qtリソースシステムの活用
- フォント解決メカニズム
- C++でのフォント管理（QFontDatabase）
- フォールバックの設定
- フォントの一元管理パターン（シングルトン）
- ベストプラクティス

### [03. 応用編](./03-advanced.md)

**対象者**: 多言語対応やパフォーマンス最適化が必要な方

**内容**:
- Qtフォントシステムのアーキテクチャ詳細
- マルチ言語対応（CJK、RTL言語）
- プラットフォーム固有の考慮事項（Windows, macOS, Linux, 組み込み）
- パフォーマンス最適化（サブセット化、遅延ロード）
- 詳細なトラブルシューティング
- Qt Virtual Keyboardでの実装例

### [04. バージョン別差分ガイド](./04-version-differences.md)

**対象者**: Qt 5.12/5.13からの移行、または複数バージョン対応が必要な方

**内容**:
- Qt 5.12 vs Qt 5.13 の差分（setFamilies追加等）
- Qt 5.x vs Qt 6.x の差分（FontLoader、QFontDatabase、QFont::Weight）
- バージョン互換コードの書き方
- 移行ガイド

## クイックスタート

### 最も簡単な独自フォントの追加（QML）

```qml
import QtQuick

Item {
    // フォントをロード
    FontLoader {
        id: customFont
        source: "qrc:/fonts/MyFont.ttf"
    }

    // フォントを使用
    Text {
        text: "カスタムフォント"
        font.family: customFont.font.family
        font.pixelSize: 24
    }
}
```

### C++での追加

```cpp
#include <QFontDatabase>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    // フォントを追加
    int id = QFontDatabase::addApplicationFont(":/fonts/MyFont.ttf");
    if (id != -1) {
        QStringList families = QFontDatabase::applicationFontFamilies(id);
        qDebug() << "Loaded:" << families;
    }

    // QMLエンジン起動...
}
```

## 関連リンク

### Qt公式ドキュメント

- [QFontDatabase Class](https://doc.qt.io/qt-6/qfontdatabase.html)
- [FontLoader QML Type](https://doc.qt.io/qt-6/qml-qtquick-fontloader.html)
- [QFont Class](https://doc.qt.io/qt-6/qfont.html)

### フォントリソース

- [Google Fonts](https://fonts.google.com/)
- [Google Noto Fonts](https://fonts.google.com/noto)
