# 秋月電子の Z80 Cコンパイラ XCC-Vで CP/M / MSX-DOSの標準入出力とファイル入出力を使えるようにする
[秋月電子で売っている Z80用 Cコンパイラ XCC-Vで CP/M / MSX-DOS で動くプログラムを作る](https://github.com/KyoichiSato/akizuki-Z80-C-tutorial2)の続きです。
前回、アセンブリ言語で CP/Mのシステムコール関数を作成して、C言語からシステムコールを使えるようになりました。今回は XCC-Vに付属の標準ライブラリを CP/Mで使えるようにします。

MSX-DOSは CP/Mとファンクションコールが上位互換で、
MSX-DOSで CP/Mのプログラム動作します。この文章本文では CP/Mと表記していますが、
MSX-DOSでも同じように動作します。

## 目次
1. [ビルド方法](#ビルド方法)
1. [この文章のライセンス](#ライセンス)

### 別頁
* [秋月電子で売っている Z80用 Cコンパイラ XCC-Vの使い方](https://github.com/KyoichiSato/akizuki-Z80-C-tutorial1)
* [CP/Mのシステムコールを C言語から使えるようにする](https://github.com/KyoichiSato/akizuki-Z80-C-tutorial2)

## ビルド方法

## ライセンス
この文章とサンプルコードは、CC0 Public Domain License で提供します。 https://creativecommons.org/publicdomain/zero/1.0/

2025年2月7日 佐藤恭一 kyoutan.jpn.org