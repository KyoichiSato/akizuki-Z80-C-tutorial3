# 秋月電子の Z80 Cコンパイラ XCC-Vで CP/M / MSX-DOSの標準入出力とファイル入出力を使えるようにする
[秋月電子で売っている Z80用 Cコンパイラ XCC-Vで CP/M / MSX-DOS で動くプログラムを作る](https://github.com/KyoichiSato/akizuki-Z80-C-tutorial2)の続きです。
前回、アセンブリ言語で CP/Mのシステムコール関数を作成して、C言語からシステムコールを使えるようになりました。今回は XCC-Vに付属の標準ライブラリを CP/Mで使えるようにします。

MSX-DOSのファンクションコールは CP/Mと上位互換で、
MSX-DOSで CP/Mのプログラムが動作します。この文章本文では CP/Mと表記していますが、
MSX-DOSでも同じように動作します。


## 目次
1. [ビルド方法](#ビルド方法)
    * [ソースコードのダウンロード方法](#ソースコードのダウンロード方法)
2. [main関数にコマンドライン引数を渡せるようにする](#main関数にコマンドライン引数を渡せるようにする)
3. [標準入出力を使えるようにする](#標準入出力とファイル入出力を使えるようにする)
    * [標準入出力を使ってみる](#標準入出力を使ってみる)
    * [ファイル入出力を使ってみる](#ファイル入出力を使ってみる)
4. [この文章のライセンス](#ライセンス)

### 別頁
* [秋月電子で売っている Z80用 Cコンパイラ XCC-Vの使い方](https://github.com/KyoichiSato/akizuki-Z80-C-tutorial1)
* [CP/Mのシステムコールを C言語から使えるようにする](https://github.com/KyoichiSato/akizuki-Z80-C-tutorial2)


## ビルド方法
ビルド用のバッチファイルがありますので、`build.bat` を実行するだけでビルドできます。
そのまま実行できる簡単なサンプルもついています。

コンパイラを `"C:\akiz80\"` 以外にインストールしている場合は、`build.bat` を書き換えてください。

Intel HEXを CP/Mの COM形式に変換するために Pythonを使用しています。[秋月電子の Z80 Cコンパイラ XCC-Vで CP/M / MSX-DOSで動くプログラムを作る](https://github.com/KyoichiSato/akizuki-Z80-C-tutorial2?tab=readme-ov-file#Intel-HEX%E5%BD%A2%E5%BC%8F%E3%82%92-COM%E5%BD%A2%E5%BC%8F%E3%81%AB%E5%A4%89%E6%8F%9B)
も読んでみてください。

### ソースコードのダウンロード方法
gitを使っていない人は
[このリポジトリ](https://github.com/KyoichiSato/akizuki-Z80-C-tutorial3)
の上のほう、ファイルが並んでいるところの右上にある **[ <> Code ]** で「Download ZIP」
を選ぶとソースコード一式をダウンロードすることができます。

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
標準入力とファイル入出力の使用例 `tutorial_3.c` 付き。


## main関数にコマンドライン引数を渡せるようにする
前回まで使っていたスタートアッププログラムでは、引数なしで main関数を起動していました。
CP/Mではシステムスクラッチエリアの 0x0080から 0x00ff番地に、
 コマンドラインがそのまま文字列としてセットされてプログラムが起動するので、
これを C言語形式の文字列にして argc, argvにセットして main関数を起動するように変更します。

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

    argc++;
    main(argc, argv); /* ユーザーメイン関数を呼び出す */

    CPMRESET(); /* OSに戻る */
}
```
スタートアッププログラムで main関数を呼び出していた代わりに、この CALL_MAIN関数を呼び出すように変更しました。

CP/Mでは OSに終了コードを返す仕組みが無いので、main関数の戻り値は無視しています。

```
    /* コマンドライン引数を表示する */
    print("argc:");
    puthex(argc);
    print("\r\n");
    for (c = 0; c < argc; c++)
    {
        print("argv[");
        puthex(c);
        print("]:");
        print(argv[c]);
        print("\r\n");
    }
```
ためしにこんな感じで実行してみると

```
> ./cpm tutorial_3 abcd efgh 1234 5678
argc:05
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
コンパイラに付属している標準ライブラリを使って標準入出力とファイル入出力を使えるようにするには、
コンパイラのマニュアルの 「ライブラリ - ユーザ作成関数のインプリメント方法」
に書かれているように、ユーザーが以下の関数を作成する必要があります。

* abort
* exit
* sbrk
* create
* open
* close
* lseek
* read
* write
* _iob[] 構造体

そこで、これらの関数を `"cpmstdio.c"` に自分で書いてみました。

### sbrk
指定されたサイズのメモリを確保する関数で mallocなどから呼び出されるのですが、
このコンパイラの標準ライブラリの getsや putsのようにとてもよく使う関数でも使われているので、
標準入出力を使うにはユーザーが sbrkを作成しなければなりません。

CP/Mではメモリを管理するシステムコールがないので
 TPAの空きエリアからメモリ領域を確保するように作りました。

### _iob[]構造体
```
XCC-Vの stdio.hで定義されている入出力バッファの管理構造体
struct	_iobuf {
    int     _cnt;   入力時はバッファに入っているデータの残り文字数で使用し，
                    出力時はバッファリングすべき残り文字数で使用している
    char	*_ptr;  バッファリングされているデータの現バッファポインタ
    char	*_base; バッファの先頭ポインタ。省略時は，内部でバッファを割り付けている
    short	_flag;  _IOREAD 入力モードでオープンされた
                    _IOWRT  出力および更新モードでオープンされた
                    _IONBF  バッファリングしない
                            （setbuf 関数 および内部でバッファが獲得できない）
                    _IOBUF  内部でバッファを獲得した
                    _IOEOF  ファイルの終了
                    _IOERR  ファイルにエラーがあったとき
    char	_file;  ユーザーが作成する create や open 関数 より受け取ったファイルディスクリプタ番号
};

struct _iobuf _iob[_MAXFILE]; /* _iob[]の実体 9*20=180byte */
```
構造体は stdio.hで定義されていて、`cpmstdio.c` の中で実体を用意しています。

_iob[]を使って CP/Mの DTA (DMA) を直接操作できないかなと思って、コンパイラに付属している標準ライブラリ `FILLBUF` `FLUSHBUF` のソースコードを眺めてみたのですが、どうも無理そうだったので、 DTAの管理は別に構造体を用意することにしました。
コンパイラに付属しているライブラリのソースコードが、 Z80のアセンブリ言語で読むのが大変で、大雑把に読んだだけなので理解が間違っているかもしれません。
 コメントの感じから、Cコンパイラが出力したアセンブリのようでした。

* _iob[0] 標準入力
* _iob[1] 標準出力
* _iob[2] 標準エラー出力

が決められていて、それ以外がファイル入出力に使われます。

### write / read
ファイルディスクリプタ番号で指定したファイルに CP/Mのシステムコールを使ってデータを読み書きします。任意の位置で読み書きする必要があるので、 CP/Mのランダムアクセスを使用しました。

ファイルディスクリプタ番号は、 `_job[]` 構造体のテーブル番号。 `_iob[fd]` の `fd` がファイルディスクリプタ番号。

### lseek
ファイルの読み書きする位置を変更する関数で、起点からのバイト数で位置を指定します。負の値も指定可能。

起点は

* 0 (SEEK_SET)
* 1 (SEEK_CUR)
* 2 (SEEK_END)

を指定できるのですが、CP/Mではファイルが 128バイトごと取り扱われていて、データを1バイトだけ書いたファイルでもファイルサイズが 128バイトになってしまって、起点を SEEK_ENDにすると読み書き位置がズレてしまいますので、SEEK_ENDは使用しないでください。

MSX-DOSにしかないシステムコールを使うと SEEK_ENDでも正しい動作にできるのだけど、CP/Mで動かなくなるのでやっていません。

### create / open
ファイルをオープンします。引数に filenameと modeを与えるのですが、createと openでは modeの意味が違って、
```
createの mode （ファイルパーミッション）

0000000*** *** ***
       ||| ||| |||
       ||| ||| ||+- その他ユーザー 実行可能
       ||| ||| |+-- その他ユーザー 書込可能
       ||| ||| +--- その他ユーザー 読込可能
       ||| ||+----- グループ 実行可能
       ||| |+------ グループ 書込可能
       ||| +------- グループ 読込可能
       ||+--------- 所有者 実行可能
       |+---------- 所有者 書込可能
       +----------- 所有所 読込可能
```
```
openの mode 

0:入力
1:出力
2:更新（入出力）
```
となっています。

標準ライブラリが UNIX由来なのでファイルパーミッションが使われているようです。XCC-Vで createが呼ばれるときは、必ず読み書き可能モード 0664 (110 110 100) になっているようでした。

コンパイラのマニュアルに openの modeは説明があったのですが、createの modeの説明がなくて、コンパイラに付属している標準ライブラリのソースコードを見て調べました。ライブラリのソースはアセンブリでした。

### close
CPMのシステムコールを使用して、ファイルを閉じます。


## 標準入出力を使ってみる
```
#include <stdio.h>

main()
{
    puts("Hello, world!");
    printf("int:%d float:%f\r\n", 1234, (float)-987.654);
    printf("CPM Version %04X\r\n", CPMVER());
    printf("Please enter a string : ");
    gets(BUFF);
    printf("\r\n%s\r\n", BUFF);
}
```

printfは floatも表示できて高機能なのですが、
printfを使うと実行ファイルのサイズが 20Kbyteくらい増えてしまうので、
CP/Mのシステムコールを直接使うか、putsや putcだけを使うようにしたほうが効率が良いです。

putsや putcならサイズの増大はわずかでした。

printfを使う場合は、スタックを多めに 2048byte程度確保する必要があります。スタックが不足してもエラーなんかは出ず、プログラムが不可解な動作をするようになるので注意。


## ファイル入出力を使ってみる
```
#include <stdio.h>

/* ファイルの内容を表示する。
ファイル終端か、size分表示したら終了する */
void dispfile(FILE *fp, unsigned short size)
{
    char c;
    /* テキストファイル終端まで表示する */
    while (size--)
    {
        c = fgetc(fp);
        if (EOF == c)
        {
            print(" EOF ");
            break;
        }

        if (0x1a == c)
        {
            print(" 0x1A EOF ");
            break;
        }
        putchar(c);
    }
}

main(int argc, char *argv[])
{
    FILE *fp1, *fp2;

    fp1 = fopen("test.txt", "r");
    if (NULL != fp1)
    {
        /* テキストファイル終端まで表示する */
        dispfile(fp1, 0xffff);
        print("\r\n");
    }

    fp2 = fopen("write.$$$", "w");
    if (NULL != fp2)
    {
        /* ファイルに適当書き込み */
        fputs("TEST1 TEXT", fp2);
    }
 
    if (NULL != fp1)
    {
        if (0 == fclose(fp1))
            print("fp1 fclose OK\r\n");
    }

    if (NULL != fp2)
    {
        if (0 == fclose(fp2))
            print("fp2 fclose OK\r\n");
    }
}
```
のように cの標準ライブラリをつかってファイルの入出力を行うことができます。

せっかく標準ライブラリでファイル入出力を使えるようにしたのですが、まどろっこしいので CP/Mのシステムコールを直接使用したほうが効率が良いです。
CP/Mの FCBを使ったファイルアクセスも、使ってみれば簡単です。

## ライセンス
この文章とサンプルコードは、CC0 Public Domain License で提供します。 https://creativecommons.org/publicdomain/zero/1.0/

2025年2月7日 佐藤恭一 kyoutan.jpn.org
