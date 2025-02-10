# 秋月電子の Z80 Cコンパイラ XCC-Vで CP/M / MSX-DOSの標準入出力とファイル入出力を使えるようにする
[秋月電子で売っている Z80用 Cコンパイラ XCC-Vで CP/M / MSX-DOS で動くプログラムを作る](https://github.com/KyoichiSato/akizuki-Z80-C-tutorial2)の続きです。
前回、アセンブリ言語で CP/Mのシステムコール関数を作成して、C言語からシステムコールを使えるようになりました。今回は XCC-Vに付属の標準ライブラリを CP/Mで使えるようにします。

MSX-DOSは CP/Mとファンクションコールが上位互換で、
MSX-DOSで CP/Mのプログラム動作します。この文章本文では CP/Mと表記していますが、
MSX-DOSでも同じように動作します。

## 目次
1. [ビルド方法](#ビルド方法)
    * [ソースコードのダウンロード方法](#ソースコードのダウンロード方法)
1. [main関数にコマンドライン引数を渡せるようにする](#main関数にコマンドライン引数を渡せるようにする)
1. [標準入出力を使えるようにする](#標準入出力を使えるようにする)
1. [この文章のライセンス](#ライセンス)

### 別頁
* [秋月電子で売っている Z80用 Cコンパイラ XCC-Vの使い方](https://github.com/KyoichiSato/akizuki-Z80-C-tutorial1)
* [CP/Mのシステムコールを C言語から使えるようにする](https://github.com/KyoichiSato/akizuki-Z80-C-tutorial2)

## ビルド方法
ビルド用のバッチファイルがありますので、
`build.bat` を実行するだけでビルドできます。

Intel HEXを CP/Mの COM形式に変換するために Pythonを使用しています。[秋月電子の Z80 Cコンパイラ XCC-Vで CP/M / MSX-DOSで動くプログラムを作る](https://github.com/KyoichiSato/akizuki-Z80-C-tutorial2?tab=readme-ov-file#Intel-HEX%E5%BD%A2%E5%BC%8F%E3%82%92-COM%E5%BD%A2%E5%BC%8F%E3%81%AB%E5%A4%89%E6%8F%9B)
も読んでみてください。

### ソースコードのダウンロード方法
gitを使っていない人は
[このリポジトリ](https://github.com/KyoichiSato/akizuki-Z80-C-tutorial3)
の上のほう、ファイルが並んでいるところの右上にある **[ <> Code ]** で「Download ZIP」
でソースコード一式をダウンロードすることができます。

### ファイル一覧
```
README.md       この文書
build.bat       ビルドするバッチファイル
clean.bat       ビルドで生成された中間ファイルを削除するバッチファイル
startup.xas     Z80のアセンブリで書かれたスタートアッププログラム
call_main.c     コマンドライン引数をセットして main関数を起動する関数
cpm.xas         CP/Mのシステムコールを行うアセンブリ関数
cpm.h           CP/Mのシステムコール関数のヘッダファイル
cpmstdio.c      標準ライブラリで標準入出力とファイル入出力を使うためのユーザー作成関数
tutorial_3.xls  リンクパラメータファイル
hex2com.py      Intel HEX形式を CP/Mの COM形式に変換する Pythonプログラム
tutorial_3.c    動作確認用の main関数
```

## main関数にコマンドライン引数を渡せるようにする
前回まで使っていたスタートアッププログラムでは、引数なしで main関数を起動していました。
CP/Mではコマンドライン引数を取得できるので、コマンドライン引数を argc, argvにセットして main関数を起動するように変更します。

Z80のアセンブリで書くのは大変なので Cで書いて、Cからmain関数を起動するようにしました。

#### call_main.c
```
/*
コマンドライン引数をセットして main 関数を呼び出す
CP/M 2.2 / MSX-DOS 用

This code is provided under a CC0 Public Domain License.
http://creativecommons.org/publicdomain/zero/1.0/

2025年1月29日 作成 佐藤恭一 kyoutan.jpn.org
*/

#include <string.h>

#define ARG_MAX 20 /* コマンドライン引数の最大値 */

int argc;
char *argv[ARG_MAX];
char ARG_BUFF[0x7F]; /* コマンドライン引数のバッファ
 （CP/Mでは最大0x0081 - 0x00FF の126バイト）だけど、終端文字を追加するかもしれないので127バイト確保する */

 
/* argc、argVをセットしてmain関数を呼び出す。
戻ってきたらCP/MのファンクションコールでOSに戻る */
void CALL_MAIN(void)
{
    char *ptr;

    memset(ARG_BUFF, 0, sizeof(ARG_BUFF)); /* バッファを終端文字で初期化 */
    argv[0] = "";                          /* CP/Mでは argv[0]（実行ファイル名）を取得できないので、0番目は空文字列 */
    argc = 0;

    /* コマンドライン引数があるか */
    if (1 < *(unsigned char *)0x0080) /* 0x0080にコマンドライン引数の文字数が入っている */
    {
        /* コマンドライン引数がある （文字数が1より多い。1の時は' 'のみ）*/
        /* CP/Mのコマンドライン 0x0080 - 0x00FF （DTAのデフォルトアドレス）は
        ユーザーが書き換える可能性があるので、別の場所にコピーする */
        memcpy(ARG_BUFF, (void *)0x0081, 0x7e);
        /* 文字列を走査して' 'で分割する */
        for (ptr = ARG_BUFF; ptr < (ARG_BUFF + *(unsigned char *)0x0080); ptr++)
        {
            if (' ' == *ptr)
            {
                *ptr = 0; /* ' 'を終端コードに置き換える */
                /* ' 'が連続していないか調べる */
                if (' ' == *(ptr + 1))
                {
                    /* スペースが連続していてもOSがスペース一つに詰めてくれるようなので、この処理無くてよさそう */
                    continue; /* 連続していたら次の文字へ */
                }
                /* 次が' 'ではないとき */
                argc++;
                argv[argc] = ptr + 1;
                if (ARG_MAX <= argc)
                {
                    break; /* 最大数を超えたら終了 （ARG_MAX-1まで繰り返す）*/
                }
            }
        }
    }

    main(argc, argv); /* ユーザーメイン関数を呼び出す */

    CPMRESET(); /* OSに戻る */
}
```
スタートアッププログラムで main関数を呼び出していた代わりに、この CALL_MAIN呼び出すように変更しました。

CP/Mでは OSに終了コードを返す仕組みが無いので、main関数の戻り値は無視しています。

```
main(int argc, char *argv[])
{
    int c;

    print("argc:");
    puthex(argc);
    puts("");
    for (c = 0; c <= argc; c++)
    {
        print("argv[");
        puthex(c);
        print("]:");
        puts(argv[c]);
    }
}
```
ためしにこんな感じで実行してみると

```
> ./cpm tutorial_3 abcd efgh 1234 5678
argc:04
argv[00]:
argv[01]:ABCD
argv[02]:EFGH
argv[03]:1234
argv[04]:5678
> 
```
コマンドライン引数を読みだすことができました。
CP/Mでは argv[0]  （このプログラムの名前）を知る方法がないので、argv[0]はカラになります。


## 標準入出力とファイル入出力を使えるようにする
コンパイラに付属している標準ライブラリを使って、
標準入出力とファイル入出力を使えるようにする方法は、
コンパイラのマニュアルの 「ライブラリ - ユーザ作成関数のインプリメント方法」
に書かれているように以下の関数を作成する必要があります。

* abort
* exit
* sbrk
* close
* create
* lseek
* open
* read
* write
* iob[]構造体

そこで、 `"cpmstdio.c"` に自分で書いてみました。

printfを使うと実行ファイルのサイズが 20Kbyteくらい増えてしまうので、
CP/Mのシステムコールを直接使うか、putsや putcだけを使うようにしたほうが効率が良いです。

putsや putcならサイズの増大はわずかです。

## ファイル入出力
せっかく標準ライブラリでファイル入出力を使えるようにしたのですが、まどろっこしいので CP/Mのシステムコールを直接使用したほうが効率が良いです。

CP/Mの FCBを使ったファイルアクセスも使ってみれば簡単です。

## ライセンス
この文章とサンプルコードは、CC0 Public Domain License で提供します。 https://creativecommons.org/publicdomain/zero/1.0/

2025年2月7日 佐藤恭一 kyoutan.jpn.org