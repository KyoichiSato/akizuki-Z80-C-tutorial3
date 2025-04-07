/*
XCC-Vで標準入出力を使用するためのユーザー作成関数
CP/M 2.2 / MSX-DOS 用

This code is provided under a CC0 Public Domain License.
http://creativecommons.org/publicdomain/zero/1.0/

2024年12月11日 作成 佐藤恭一 kyoutan.jpn.org

標準入出力を使えるようにするところまで参考になる
ルネサス スタートアップ＞コーディング例
https://tool-support.renesas.com/autoupdate/support/onlinehelp/ja-JP/csp/V4.01.00/CS+.chm/Compiler-CCRX.chm/Output/ccrx08c0400y.html


参考になる書籍

MSX-DOS / CP/Mのシステムコール一覧
MSX2 Technical Hand book
MSX-DOS TOOLS
MSX-Datapack
SHARP X1 turbo用 turbo CP/M ユーザーズマニュアル

CP/Mのシステムコールの実際の使用例
応用 CP/M
*/

// #define _DEBUG /* デバッグ用の出力を有効にする */

#include "cpm.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/*
XCC-Vの stdio.hで定義されている入出力バッファの管理テーブル
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
*/

//#define DEBUG /* デバッグ用の出力を有効にする */

#define DTASIZE 0x80       /* DTA (DMA) のサイズ CP/Mは 128byte */
#define FPATH_STDIN "CON"  /* 標準入力のデバイス名 */
#define FPATH_STDOUT "CON" /* 標準出力のデバイス名 */
#define FPATH_STDERR "CON" /* 標準エラー出力のデバイス名 */

/* ファイルディスクリプタ番号 */
#define FD_INVALID -1
#define FD_STDIN 0
#define FD_STDOUT 1
#define FD_STDERR 2
#define FD_UNALLOCATED 0x7f /* 一度も使われていない */
#define FD_EMPTY 0x7e       /* 一度使われたが今は使われていない */

/* open()の mode (createのmodeは別のようだ) */
#define O_RDONLY 0x00
#define O_WRONLY 0x01
#define O_RDWR 0x02
// #define O_CREAT 0x40
// #define O_APPEND 0x400

#define OPENMODE_CREATE 0
#define OPENMODE_OPEN 1

/* ヒープメモリ操作用 */
static struct stheap
{
    void *start;
    void *end;
    void *point;
} HEAPMEMORY; /* staticなのでこのファイル内だけ参照できる */

/* ファイル操作用 */
struct _iobuf _iob[_MAXFILE]; /* _iob[]の実体 9*20=180byte */

/* CP/M 互換のFCB
MSX-DatapackのCP/M互換FCBの表は誤りあり。
MSX-DOSTOOLSか応用CP/Mを参照。 */
struct stcpmfcb
{
    unsigned char drive;         /*  +0  1 ドライブ番号 0:デフォルト 1:A 2:B ... */
    char filename[11];           /*  +1 11 ファイル名 8+3 隙間は0x20で埋める */
    unsigned char currentblock;  /* +12  1 カレントブロック シーケンシャルアクセス時、参照中のブロック (CP/M用語でロジカルエクステント)*/
    unsigned char dummy1;        /* +13  1 （システムが内部で使用） */
    unsigned char dummy2;        /* +14  1 （システムが内部で使用） */
    unsigned char recordsize;    /* +15  1 レコードサイズ CP/Mでは最終ブロック以外常に 128 */
    unsigned long filesize;      /* +16  4 ファイルサイズ （システムが内部で使用）*/
    unsigned short date;         /* +20  2 日付 （システムが内部で使用）*/
    unsigned short time;         /* +22  2 時刻 （システムが内部で使用）*/
    unsigned char deviceid;      /* +24  1 デバイスID 周辺装置/ディスクファイルの区別 （システムが内部で使用）*/
    unsigned char dirlocation;   /* +25  1 ディレクトリロケーション 何番目のディレクトリエントリに該当するか （システムが内部で使用）*/
    unsigned short firstcluster; /* +26  2 先頭クラスタ （システムが内部で使用）*/
    unsigned short lastcluster;  /* +28  2 最終クラスタ （システムが内部で使用）*/
    unsigned short relativepos;  /* +30  2 相対位置 （システムが内部で使用）*/
    unsigned char currentrecord; /* +32  1 カレントレコード シーケンシャルアクセス時、参照中のレコード番号 */
    unsigned short rndrecordL;   /* +33  2 ランダムレコード 下位2バイト */
    unsigned char rndrecordH;    /* +35  1 ランダムレコード 上位1バイト ランダムアクセス時、アクセスしたいレコード番号をセットする */
    unsigned char rndrecordHH;   /* +36  1 ランダムレコード MSX-DOSのみ */
    /* CP/Mでユーザーが操作するのは
        ドライブ番号
        ファイル名
        カレントブロック
        カレントレコード
        ランダムレコード

    1レコード           128バイト
    1ブロック           128レコード=128*128=16384バイト（ブロックのサイズはシステムにより異なるみたい）
    カレントレコード    現在のレコード位置 バイト/128
    カレントブロック    現在のブロック位置 バイト/128/128（ブロックサイズはシステムにより異なるので、ユーザーは操作しない。任意の位置をアクセスするときはランダムブロックを使用する。）
    ランダムレコードで扱える最大サイズ 128*65535*255=2GB
    ロジカルエクステントの最大値がシステムにより異なるようだ。（X1 turboのCP/Mで 0x3F、応用CP/Mで 0x1F、MSX-DOSで 0xFF？）
    */
};

struct stfilebuff
{
    struct stcpmfcb FCB;
    unsigned char *DTA_ptr;     /* DTAの読み書き位置 */
    unsigned char DTA_written;  /* レコードが書き換えられたフラグ 0:書き込み無し 1:書き込み有り */
    unsigned char DTA[DTASIZE]; /* CP/Mのファイルアクセスに使うレコードバッファ */
};
/* ファイル入出力バッファ
    空のポインタの配列を宣言 */
static struct stfilebuff *_FILEBUFF[_MAXFILE];

/* --------------------------------------------------------------- */
/* このファイル内で使うヘルパ関数 */
/* --------------------------------------------------------------- */

/* 下位4ビットの値を16進1文字のキャラクタコードで返す */
char hexchar(unsigned char a)
{
    a &= 0x0f;
    if (10 > a)
    {
        return a + '0';
    }
    return a - 10 + 'A';
}

/* 1バイトの値を16進で表示する */
void puthex(unsigned char a)
{
    char code;
    code = hexchar(a >> 4);
    CPMPUTC(code);
    code = hexchar(a);
    CPMPUTC(code);
}

/* unsigned short型を16進4桁で表示する */
void puthexshort(unsigned short a)
{
    puthex(a >> 8);
    puthex(a);
}

/* C形式の文字列を表示する */
void print(char *str)
{
    unsigned char size;
    size = 0xff; /* 最大サイズ */
    while (size)
    {
        if (NULL == *str)
            return;
        CPMPUTC(*str++);
        size--;
    }
}

/* 指定アドレスからsize分ダンプする ASCII表示付き */
void memdump(unsigned short address, unsigned short size)
{
    unsigned char *p;
    unsigned short count;
    unsigned char code;

    p = (unsigned char *)address;
    for (count = 0; size > count; count++)
    {
        /* 16進表示 */
        if (0 == (count % 16))
        {
            puthexshort((unsigned short)p);
            CPMPUTC(' ');
        }
        puthex(*p++);
        CPMPUTC(' ');

        /* ASCII表示 */
        if (0 == ((count + 1) % 16))
        {
            p = p - 16;
            CPMPUTC(' ');
            for (code = 0; 16 > code; code++)
            {
                if (isprint(*p))
                {
                    CPMPUTC(*p);
                }
                else
                {
                    CPMPUTC('.');
                }
                p++;
            }
            CPMPUTC('\r');
            CPMPUTC('\n');
        }
    }

    /* 最後の一行のASCII表示 */
    if (0 != (count % 16))
    {
        for (code = 0; (16 - (count % 16)) > code; code++)
        {
            print("   ");
        }
        CPMPUTC(' ');
        p = p - ((count % 16));
        for (code = 0; (count % 16) > code; code++)
        {
            if (isprint(*p))
            {
                CPMPUTC(*p);
            }
            else
            {
                CPMPUTC('.');
            }
            p++;
        }
        CPMPUTC('\r');
        CPMPUTC('\n');
    }
}

/* _iob[]の内容を表示する */
void put_iob(void)
{
    unsigned char count;
    print(" cnt  ptr base flag file\r\n");
    for (count = 0; _MAXFILE > count; count++)
    {
        puthexshort(_iob[count]._cnt);
        CPMPUTC(' ');
        puthexshort((short)_iob[count]._ptr);
        CPMPUTC(' ');
        puthexshort((short)_iob[count]._base);
        CPMPUTC(' ');
        puthexshort(_iob[count]._flag);
        print("   ");
        puthex(_iob[count]._file);
        print("\r\n");
    }
}

/* _iob[]初期化 */
void init_iob(void)
{
    unsigned char count;
    /* I/Oバッファのインデックスを初期化 */
    for (count = 0; _MAXFILE > count; count++)
    {
        _iob[count]._cnt = 0;
        _iob[count]._ptr = (void *)NULL;
        _iob[count]._base = (void *)NULL;
        _iob[count]._flag = 0;
        _iob[count]._file = FD_UNALLOCATED;
    }
    /* 標準出力 */
    stdout->_cnt = 0; /* バッファの空きサイズ */
    stdout->_ptr = (void *)NULL;
    stdout->_base = (void *)NULL;
    stdout->_flag = _IOWRT + _IONBF; /*ライトモード＋バッファしない*/
    stdout->_file = FD_STDOUT;
    /* 標準エラー出力 */
    stderr->_cnt = 0;
    stderr->_ptr = (void *)NULL;
    stderr->_base = (void *)NULL;
    stderr->_flag = _IOWRT + _IONBF; /*ライトモード＋バッファしない*/
    stderr->_file = FD_STDERR;
    /* 標準入力 */
    stdin->_cnt = 0;
    stdin->_ptr = (void *)NULL;
    stdin->_base = (void *)NULL;
    stdin->_flag = _IOREAD + _IONBF; /*リードモード＋バッファしない*/
    stdin->_file = FD_STDIN;
}

/* DTAに有効なデータがあるかチェックする（DTA読み書きポインタがDTA内にあるか）
    引数 int fd ファイルディスクリプタ番号
    戻り値 0:データなし 0以外:データあり */
int DTAvalid(int fd)
{
    if (_FILEBUFF[fd]->DTA_ptr >= _FILEBUFF[fd]->DTA + DTASIZE)
    {
        return 0; /* バッファの範囲外 */
    }
    if (_FILEBUFF[fd]->DTA_ptr < _FILEBUFF[fd]->DTA)
    {
        return 0; /* バッファの範囲外 */
    }
    return !0; /* バッファにデータあり */
}

/* ランダムレコード番号をインクリメントする
    引数 int fd ファイルディスクリプタ番号 */
void rndrecordinc(int fd)
{
    _FILEBUFF[fd]->FCB.rndrecordL++;
    if (0 == _FILEBUFF[fd]->FCB.rndrecordL)
    {
        /* 繰り上がり */
        _FILEBUFF[fd]->FCB.rndrecordH++;
    }
}

/* ランダムレコード番号をデクリメントする（ランダムレコード番号が 0の時はなにもしない）
    引数 int fd ファイルディスクリプタ番号 */
void rndrecorddec(int fd)
{
    _FILEBUFF[fd]->FCB.rndrecordL--;
    if (0xffff == _FILEBUFF[fd]->FCB.rndrecordL)
    {
        if (0 == _FILEBUFF[fd]->FCB.rndrecordH)
        {
            /* rndrecordLと rndrecordH両方0だった時 */
            _FILEBUFF[fd]->FCB.rndrecordL = 0;
        }
        else
        {
            /* 繰り下がり */
            _FILEBUFF[fd]->FCB.rndrecordH--;
        }
    }
}

/* name "d:8.3" 形式から、ドライブ番号、ファイル名を取り出す
戻り値 0:失敗 0以外:成功
FCB->drive    ドライブ番号
FCB->filename[11] ファイルネーム 8文字 + 拡張子 3文字 隙間は0x20で埋める
CP/Mなので nameにディレクトリ名は含まれないものとして扱う */
char extractfilename(char name[], struct stcpmfcb *FCB)
{
    unsigned char src, dst;

    /* FCBの filename[11]を 0x20で初期化 */
    memset(FCB->filename, 0x20, 11);

    /* 入力文字列を大文字にする */
    for (src = 0; NULL != name[src]; src++)
    {
        if (20 < src)
            return 0; /* 変に長かったらエラー */
        name[src] = toupper(name[src]);
    }

    /* ドライブ番号を取り出す */
    FCB->drive = 0; /* ドライブ指定が無いときは、デフォルトドライブ */
    src = 0;
    if (':' == name[1])
    {
        /* ドライブ指定がある */
        if (('A' <= name[0]) && ('P' >= name[0]))
        {
            /* A-P のとき
             (MSX-Datapackによると、ドライブはAからHの間。応用 CP/MによるとAからPの間。)
            FCBのドライブ番号 デフォルトドライブ:0 A:1 B:2 ... H:8
            ファンクションコールのデフォルトドライブ設定で使う番号とは別 */
            FCB->drive = name[0] - 'A' + 1;
            src = 2; /* ファイル名の先頭 */
        }
        else
        {
            return 0; /* ドライブ指定があったが不正な指定だった */
        }
    }

    /* ファイル名をセット */
    for (dst = 0; dst < 8; dst++)
    {
        if (NULL == name[src])
            return !0; /* 正常終了 */
        if ('.' == name[src])
        {
            /* '.'は無視 */
            src++;
            break;
        }
        FCB->filename[dst] = name[src++];
    }

    /* 拡張子をセット */
    for (dst = 8; dst < 11; dst++)
    {
        if (NULL == name[src])
            return !0; /* 正常終了 */
        if ('.' == name[src])
        {
            /* '.'は無視 */
            src++;
        }
        FCB->filename[dst] = name[src++];
    }
    return !0; /* 正常終了 */
}

/* 発行済で使われていないファイルディスクリプタ番号があるかしらべる
あればファイルディスクリプタ番号、無ければ FD_INVALIDを返す */
char fdempty(void)
{
    char fd;
    for (fd = 3; _MAXFILE > fd; fd++) /* 0-2は標準入出力 */
    {
        /*_iobを先頭からスキャンして、
        はじめに見つかった空のファイルディスクリプタ番号を返す */
        if (FD_EMPTY == _iob[fd]._file)
            return fd;
    }
    return FD_INVALID;
}

/* 新規ファイルディスクリプタ番号を返す
割り当てできなければ FD_INVALIDを返す */
char fdallocate(void)
{
    char fd;
    for (fd = 3; _MAXFILE > fd; fd++) /* 0-2は標準入出力 */
    {
        /*_iobを先頭からスキャンして、
        はじめに見つかった新規ファイルディスクリプタ番号を返す */
        if (FD_UNALLOCATED == _iob[fd]._file)
            return fd;
    }
    return FD_INVALID;
}

/* 使えるファイルディスクリプタ番号を返す
使われたことがない番号ならバッファも確保する
割り当てできなければ FD_INVALIDを返す */
char fdget(void)
{
    char fd;
    fd = fdempty();
    if (FD_INVALID != fd)
    {
        /* 開放されたファイルディスクリプタがあった
        FCBとDTAは割り当て済み */
        return fd;
    }

    fd = fdallocate();
    if (FD_INVALID != fd)
    {
        /* 新規にFCBとDTAを割り当て */
        if (NULL != (_FILEBUFF[fd] = malloc(sizeof(*_FILEBUFF[0]))))
        {
            /* バッファを確保できた */
            return fd;
        }
    }
    /* 割り当てできない */
    // puts("fdget NG");
    return FD_INVALID;
}

/* コンソール出力 write()から呼ばれる */
void conout(char buf[], int size)
{
    while (size)
    {
        CPMPUTC(*buf++);
        size--;
    }
}


/* --------------------------------------------------------------- */
/* XCC-Vのライブラリ関数を使用するためにユーザーが作成する必要がある関数 */
/* --------------------------------------------------------------- */

/* スタートアップルーチンから呼び出す初期化処理 */
void INIT_CPMSTDIO(void)
{
#ifdef DEBUG
    unsigned short heapsize, SP;

    print("INIT_CPMSTDIO\r\n");
    print("SP:");
    SP = _GETSPREG();
    puthexshort(SP);
    CPMPUTC(' ');
#endif

    /* CP/M の TPA空きエリアからヒープメモリを確保する */
    HEAPMEMORY.start = _GETENDADDR();
    // HEAPMEMORY.start = (void *)0xA000;
    HEAPMEMORY.end = _GETTPAADDR();
    HEAPMEMORY.point = HEAPMEMORY.start;

#ifdef DEBUG
    heapsize = (unsigned short)HEAPMEMORY.end - (unsigned short)HEAPMEMORY.start;

    print("HEAP ");
    puthexshort((unsigned short)HEAPMEMORY.start);
    print(" - ");
    puthexshort((unsigned short)HEAPMEMORY.end);
    print(" SIZE ");
    puthexshort(heapsize);
    print("\r\n");
#endif

    init_iob(); /* _iob[]初期化 */

#ifdef DEBUG
    print("_iob init SP:");
    puthexshort(_GETSPREG());
    print("\r\n");

    put_iob();
    print("_FILEBUFF SIZE ");
    puthexshort(sizeof(*_FILEBUFF[0]));
    print("\r\n");

    print("init end SP:");
    SP = _GETSPREG();
    puthexshort(SP);
    print("\r\n");
    memdump(0, 0x100);
#endif
    /*プログラム実行中にだんだんSPレジスタの値が減少していく。
    スタックが開放されず消費されているようだ。
    最適化ONでのみ症状が出る */
    /* 最適化OFFならここまでスタック一定 */
}

/* アプリケーションの異常終了 */
void abort(void)
{
    print("abort ");
    /* 開かれているファイルがあればクローズしたほうがいいかも */
    CPMRESET();
}

/* アプリケーションの終了
status 0:正常終了 */
void _exit(int status)
{
    print("_exit ");
    /* 開かれているファイルがあればクローズしたほうがいいかも */
    CPMRESET(); /* CP/Mではたぶん終了コードを返す方法がない */
}

/* sizeで指定された大きさの記憶領域を確保して、その先頭アドレスを返す */
void *sbrk(unsigned int size)
{
    void *result;

    print("sbrk: size ");
    puthexshort(size);
    CPMPUTC(' ');

    if (((unsigned int)(HEAPMEMORY.end - HEAPMEMORY.point)) >= size)
    {
        /* 空きエリアがある */
        result = HEAPMEMORY.point;
        HEAPMEMORY.point = (void *)((char *)HEAPMEMORY.point + size);
        /* voidポインタのままだと計算ができない
        （short型だと1加算するとアドレスが2増えるなど、型がわからないと計算できない）
        ので char型のポインタにキャストしてから計算している */

        print("address ");
        puthexshort((unsigned short)result);
        print(" point ");
        puthexshort((unsigned short)HEAPMEMORY.point);
        print("\r\n");
    }
    else
    {
        /* 空きエリアがない */
        result = (void *)NULL;
        print("sbrk: memory full\r\n");
    }

    return result;
}

/* fd（ファイルディスクリプタ番号）に対応するファイルをクローズします．
正常にクローズされたときは 0 を返し，エラーが起こったときは -1 を返します．
ファイルディスクリプタ番号については， create 関数 を参照してください．*/
int close(int fd)
{
    print("close fd:");
    puthex(fd);
    print(" ");
    /* 標準入出力をクローズしようとしたときは、何もせずに正常終了 */
    if (FD_STDIN == fd)
    {
        return 0;
    }
    if (FD_STDOUT == fd)
    {
        return 0;
    }
    if (FD_STDERR == fd)
    {
        return 0;
    }

    if (fd != _iob[fd]._file)
    {
        /* 開かれていないファイルディスクリプタ番号だった */
        print("close NG invfd\r\n");
        return -1;
    }

    /* CPMCLOSEはFCBの管理情報をディスクに書き込むだけで、
    DTAの内容はディスクに書き込まないので、前もって書き込んでおく必要がある */
    /* 現在のDTAに書き込みがあったか */
    if (0 != _FILEBUFF[fd]->DTA_written)
    {
        rndrecorddec(fd); /* 次のランダムレコード番号を指しているので一つ戻す */
        /* 現在のレコードをディスクに書き出す */
        CPMSETDTA(_FILEBUFF[fd]->DTA); /* ディスクアクセスの前には毎回DTAアドレスをセットする */
        if (0x00 != CPMRNDWRITE(_FILEBUFF[fd]))
        {
            rndrecordinc(fd); /* 引いた分戻す */
            return -1;        /*エラー*/
        }
        rndrecordinc(fd);               /* 引いた分戻す */
        _FILEBUFF[fd]->DTA_written = 0; /* 書き込みありフラグクリア */
    }

    if (0x00 == CPMCLOSE(_FILEBUFF[fd]))
    {
        /* close成功 */
        _iob[fd]._file = FD_EMPTY;
        /*_iob[fd]._flag = _IOERR;*/ /*fclose()で_flagはゼロクリアされる*/
        print("CPMCLOSE OK\r\n");
        return 0;
    }
    /* close失敗 */
    print("CPMCLOSE NG\r\n");
    return -1;
}

/* openmodeに応じて、openか createを行う
*name ファイル名
mode は openと createで異なる
openmodeは OPENMODE_CREATEまたは OPENMODE_OPEN
戻り値 失敗:FD_INVALID 成功:ファイルディスクリプタ番号
*/
int createopen(char *name, int mode, char openmode)
{
    unsigned char pos;
    char fd;

    /* ファイル名にドライブ名が含まれるか */
    if (name[1] == ':')
    {
        pos = 2; /* ドライブ名がある */
    }
    else
    {
        pos = 0; /* ドライブ名がない */
    }

    /* 標準入出力の時はなにもしない */
    if (0 == strcmp(&name[pos], FPATH_STDOUT))
    {
        if (O_WRONLY == mode)
        {
            /* 標準出力 */
            return FD_STDOUT;
        }
    }
    if (0 == strcmp(&name[pos], FPATH_STDERR))
    {
        if (O_WRONLY == mode)
        {
            /* 標準エラー出力 */
            return FD_STDERR;
        }
    }
    if (0 == strcmp(&name[pos], FPATH_STDIN))
    {
        if (O_RDONLY == mode)
        {
            /* 標準入力 */
            return FD_STDIN;
        }
    }

    /* ファイルディスクリプタ番号を決定する */
    fd = fdget(); /* fdget()でバッファは確保されている */
    print("fd:");
    puthex(fd);
    print("\r\n");

    if (FD_INVALID == fd)
        return FD_INVALID;

    /* FCBを 0で初期化 */
    // printf("FCB size %d ", sizeof(*_FILEBUFF[fd])); /* 164 */
    memset(_FILEBUFF[fd], 0, sizeof(*_FILEBUFF[fd]));

    /* FCBにファイル名をセット */
    if (!extractfilename(name, _FILEBUFF[fd]))
    {
        /* nameがおかしい */
        puts("INV NAME");
        return FD_INVALID;
    }
    _FILEBUFF[fd]->FCB.currentblock = 0;
    memdump((unsigned short)_FILEBUFF[fd], 36);

    switch (openmode)
    {
    case OPENMODE_CREATE:
        /* ファイル作成 */
        if (0xff == CPMCREATE(_FILEBUFF[fd]))
        {
            /* エラー */
            puts("CPMCREATE NG");
            return FD_INVALID;
        }
        puts("CPMCREATE OK");
        break;

    case OPENMODE_OPEN:
        /* ファイルオープン */
        if (0xff == CPMOPEN(_FILEBUFF[fd]))
        {
            /* エラー */
            puts("CPMOPEN NG");
            return FD_INVALID;
        }
        puts("CPMOPEN OK");
        break;
    default:
        /* openmode不正 */
        return FD_INVALID;
    }

    /* _flagはfdopen()でセットされるのでここでは_IOREAD _IOWRTフラグを0にしておく */
    _iob[fd]._flag = _IONBF; /* iobのバッファ無し、_IOERRクリア　*/

    /* modeに ”+” をつけても読み書き可能フラグが立たないので、ここで立てておく */
    switch (mode)
    {
    case O_RDONLY:
        _iob[fd]._flag |= _IOREAD; /*リードモード*/
        break;
    case O_WRONLY:
        _iob[fd]._flag |= _IOWRT; /*ライトモード*/
        break;
    case O_RDWR:
        _iob[fd]._flag |= _IOREAD + _IOWRT; /*リードライトモード*/
        break;
    }

    /*DTAの読み書きポインタをバッファ終端の次にセット（バッファにデータなし）*/
    _FILEBUFF[fd]->DTA_ptr = _FILEBUFF[fd]->DTA + DTASIZE;
    _FILEBUFF[fd]->DTA_written = 0; /* レコードが書き換えられたフラグクリア */

    print("_iob[");
    puthex(fd);
    print("] ");
    puthexshort((unsigned short)&_iob[fd]);
    putchar(' ');
    puthexshort(_iob[fd]._cnt);
    putchar(' ');
    puthexshort((unsigned short)_iob[fd]._ptr);
    putchar(' ');
    puthexshort((unsigned short)_iob[fd]._base);
    putchar(' ');
    puthexshort(_iob[fd]._flag);
    putchar(' ');
    puthex(_iob[fd]._file);
    print("\r\n");
    memdump((unsigned short)_FILEBUFF[fd], 36);
    put_iob();

    return fd; /* ファイルディスクリプタ番号を返す */
}

/* "name" で渡されたファイルを新しく作成し，正常ならばファイルディスクリプタ番号を返し，エラーなら -1 を返します．
ファイルディスクリプタ番号とは，ユーザーが作成する低水準入出力関数を管理する構造体のテーブル番号のことを表しています．
引数 "mode" は，ファイルに対するアクセス権限であり， fopen 関数 および freopen 関数 からは定数でパラメータを渡しています．
ファイルディスクリプタ番号 （-1:エラー 0:標準入力 1:標準出力 2:標準エラー 3:ファイル ...）
ファイルディスクリプタ番号は _job[]構造体のテーブル番号
modeはファイルパーミッション （openと異なる）
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
なので、CP/Mでは所有者のビットを見て読み書きの判断をするとよい。
（XCC-Vで createが呼ばれるときは、必ず読み書き可能モード 0664 (110 110 100) になっているようだ。）
*/
int create(char *name, unsigned int mode)
{
    print("create(");
    print(name);
    print(", 0x");
    puthexshort(mode);
    print(")\r\n");

    /* modeをopenに与える形式に変換する */
#define P_READ 0x0100  /*0000 0001 0000 0000*/
#define P_WRITE 0x0080 /*0000 0000 1000 0000*/
    if (0 != ((P_READ | P_WRITE) & mode))
    {
        mode = O_RDWR; /* 読み書きモード */
    }
    else if (0 != (P_READ & mode))
    {
        mode = O_RDONLY; /* 読み込みモード */
    }
    else if (0 != (P_WRITE & mode))
    {
        mode = O_WRONLY; /* 書き込みモード */
    }
    else
    {
        mode = O_RDONLY; /* 読み込みモード */
    }
    /* ファイルを作成 */
    return createopen(name, mode, OPENMODE_CREATE);
}

/* 引数 "name" で渡されたファイルをオープンし，正常ならばファイルディスクリプタ番号を返し，エラーなら -1 を返します．
ファイルディスクリプタ番号については create 関数 を参照してください．
mode 0:入力 1:出力 2:更新（入出力）
*/
int open(char *name, int mode)
{
    char fd;

    print("open(");
    print(name);
    print(", 0x");
    puthexshort(mode);
    print(")\r\n");
    /* ファイルを開く */
    return createopen(name, mode, OPENMODE_OPEN);
}

/* 引数 "fd" ファイルディスクリプタ番号 に対応するファイルポインタを任意の位置へ移動させ，その位置を返します．
エラー時は -1 を返します．
引数 "origin" で指定されたファイルの基点より， "offset" で指定された相対位置へファイルを移動させます．
offsetが正の値の場合はファイルの後方へ移動し，負の値の場合はファイルの前方へ移動します．

origin
0 (SEEK_SET) ファイルの先頭
1 (SEEK_CUR) ファイルの現在位置
2 (SEEK_END) ファイルの最終
 （CP/Mではファイルの終端が不明確なので、SEEK_ENDは使わないでください。
    fopen()でも "a"を含むモードを使わないでください。オフセットがずれます。）*/
long lseek(int fd, long offset, int origin)
{
    signed long size;
    unsigned char DTA_cnt;

    /*標準入出力ならなにもしない*/
    if ((FD_STDIN == fd) || (FD_STDOUT == fd) || (FD_STDERR == fd))
    {
        return offset; /*何もせず正常終了*/
    }

    /* 書き込み中にシークすると書き込み中のレコードが失われるので、
    現在のレコードをディスクに書き出す */
    /* 現在のDTAに書き込みがあったか */
    if (0 != _FILEBUFF[fd]->DTA_written)
    {
        rndrecorddec(fd); /* 次のランダムレコード番号を指しているので一つ戻す */
        /* 現在のレコードをディスクに書き出す */
        CPMSETDTA(_FILEBUFF[fd]->DTA); /* ディスクアクセスの前には毎回DTAアドレスをセットする */
        if (0x00 != CPMRNDWRITE(_FILEBUFF[fd]))
        {
            rndrecordinc(fd); /* 引いた分戻す */
            return -1;        /*エラー*/
        }
        rndrecordinc(fd);               /* 引いた分戻す */
        _FILEBUFF[fd]->DTA_written = 0; /* 書き込みありフラグクリア */
    }

    /* バッファ先頭からの現在の読み書き位置を計算 */
    DTA_cnt = _FILEBUFF[fd]->DTA_ptr - _FILEBUFF[fd]->DTA;
    if (DTASIZE <= DTA_cnt)
    {
        /* バッファの範囲外 */
        DTA_cnt = 0;
    }

    size = 0;
    /* ファイル先頭からのオフセットを計算する */
    switch (origin)
    {
    case SEEK_SET:
        /*ファイルの先頭からoffsetバイト目に移動*/
        /* offsetをそのまま使う */
        break;

    case SEEK_CUR:
        /* ファイルの現在位置（前回読み書きした位置の1バイト後ろ）からoffsetバイト目に移動
        fseek(fp, 0, SEEK_CUR); を実行すると、前回読み書きした位置の次の位置に移動する。*/
        /* ファイルの先頭から現在位置までのサイズを計算する */
        rndrecorddec(fd); /* 次のランダムレコード番号を指しているので1戻す */
        size = _FILEBUFF[fd]->FCB.rndrecordH * 0x10000 * DTASIZE;
        size += _FILEBUFF[fd]->FCB.rndrecordL * DTASIZE;
        rndrecordinc(fd); /* 引いた分戻す */
        size += DTA_cnt;
        break;

    case SEEK_END:
        /* ファイルの最終からoffsetバイト目に移動
        fseek(fp, 0, SEEK_END); を実行すると、ファイルの最終位置の次の位置に移動する。
        CP/Mはファイルサイズが128バイトの倍数になるので正しい位置にシークできない */
        /*ファイルサイズを取得。サイズはFCBのランダムレコードにセットされる（ファイル終端にシーク）*/
        if (0xff == CPMFILESIZE(_FILEBUFF[fd]))
        {
            return -1; /*エラー*/
        }
        size = _FILEBUFF[fd]->FCB.rndrecordH * 0x10000 * DTASIZE;
        size += _FILEBUFF[fd]->FCB.rndrecordL * DTASIZE;
        break;

    default:
        /*originが不正*/
        return -1;
        break;
    }

    offset += size;
    if (offset < 0)
    {
        /*ファイル先頭より前へシークしようとしている*/
        return -1; /*エラー*/
    }

    /*ファイルサイズを取得。サイズはFCBのランダムレコードにセットされる（ファイル終端にシーク）*/
    if (0xff == CPMFILESIZE(_FILEBUFF[fd]))
    {
        return -1; /*エラー*/
    }
    size = _FILEBUFF[fd]->FCB.rndrecordH * 0x10000 * DTASIZE;
    size += _FILEBUFF[fd]->FCB.rndrecordL * DTASIZE;

    /* シーク先のランダムレコードをセット */
    _FILEBUFF[fd]->DTA_ptr = _FILEBUFF[fd]->DTA + offset % DTASIZE;
    _FILEBUFF[fd]->FCB.rndrecordL = offset / DTASIZE;
    _FILEBUFF[fd]->FCB.rndrecordH = offset / 0x10000 / DTASIZE;

    /* バッファにデータを読み込む */
    /* ファイル終端より先へシークするか */
    if (size <= offset)
    {
        /* ファイル終端より後ろへシークするときはDTAを0で埋めてからCPMRNDWRITEZ()で書き込み
        CPMRNDWRITEZはファイル終端からシーク先の間をゼロで埋めてくれる。 */
        memset(_FILEBUFF[fd]->DTA, 0, DTASIZE);
        CPMSETDTA(_FILEBUFF[fd]->DTA);
        if (0x00 != CPMRNDWRITEZ(_FILEBUFF[fd]))
        {
            rndrecordinc(fd); /* 次のレコード番号をセット */
            return -1;        /*エラー*/
        }
    }
    else
    {
        /* ファイル終端より前ならDTAにデータを読み込む */
        CPMSETDTA(_FILEBUFF[fd]->DTA);
        if (0x00 != CPMRNDREAD(_FILEBUFF[fd]))
        {
            rndrecordinc(fd); /* 次のレコード番号をセット */
            return -1;        /*エラー*/
        }
    }
    rndrecordinc(fd);               /* 次のレコード番号をセット */
    _FILEBUFF[fd]->DTA_written = 0; /* 書き込みありフラグクリア */
    /*正常終了*/
    return offset; /* 戻り値は、originがどれでもファイル先頭からのoffset */
}

/* 引数 "fd" ファイルディスクリプタ番号 に対応するファイルより，引数 "buf" で示すバッファに "size" 分データを入力します．
正常時は入力した文字数を返し，"end-of-file" のときは 0 (ゼロ) を返します．
また，エラーが起こった ときは -1 を返します．*/
int read(int fd, char *buf, int size)
{
    unsigned int count;
    /*
    getc,getcharはstdio.hで下のように定義されている
    #define	getc(X)		(--(X)->_cnt>=0\
                    ? (int)(*(unsigned char *)(X)->_ptr++)\
                    :_fillbuf(X))
    #define	getchar()	getc(stdin)

    バッファにデータがあれば (stdin._cntを -1して 0か 0より大きければ)
        バッファから 1byteデータを返し stdin._ptrを +1する
    バッファにデータが無ければ
        デバイスからバッファにデータを入れる (_fillbuf(stdin)する)
    */

    /* 標準入力 */
    if (stdin == &_iob[fd])
    {
        for (count = 0; count < size; count++)
        {
            *buf = CPMGETC();
            if ('\r' == *buf)
            {
                /* CP/Mは[Enter]で'\r'が入力される
                標準ライブラリでは'\n'で行末を判断する
                '\r'を'\n'に変換しないと、gets()が終了しない */
                *buf = '\n';
            }
            buf++;
        }
        return size;
    }

    /* ファイル */
    /*_iob[]でDTAのバッファを管理できないかとFILLBUF.XASを調べてみると
    XCC-Vの標準ライブラリ内で_cntや_ptrを操作していて無理だった。*/
    /*_FILEBUFからbufにコピーする*/
    for (count = 0; count < size; count++)
    {
        /* DTAバッファにデータがあるか */
        if (!DTAvalid(fd))
        {
            /*データがないのでディスクからデータを読む*/
            CPMSETDTA(_FILEBUFF[fd]->DTA); /* 複数ファイルを同時に開いても良いように、ディスク読み書き時は毎回DTAアドレスをセットする。 */
            if (0x00 != CPMRNDREAD(_FILEBUFF[fd]))
            {
                return count; /*エラー*/
            }
            /*リード成功*/
            _FILEBUFF[fd]->DTA_written = 0;              /* レコードが書き換えられたフラグクリア */
            rndrecordinc(fd);                            /*次のランダムレコードをセット*/
            _FILEBUFF[fd]->DTA_ptr = _FILEBUFF[fd]->DTA; /*ポインタをバッファの先頭に移動*/
        }

        *buf++ = *_FILEBUFF[fd]->DTA_ptr++; /* DTAから1バイトコピー */
    }
    return size; /*正常終了*/
}

/* 引数 "fd" ファイルディスクリプタ番号 に対応するファイルに，引数 "buf" で示すバッファの内容を "size" 分出力します．
正常時は出力した文字数を返し，エラーのときは -1 を返します．

CP/Mのシステムコールではファイルが128バイト単位で扱われて、ファイルの最後に意図しないデータ追加されます。
テキストファイルの終端には終端コード 0x1aをユーザーが書き込む必要があります。 */
int write(int fd, char *buf, int size)
{
    unsigned int count;
    /*
    putc, putcharはstdio.hで下のように定義されている
    #define putc(X, Y)	(--(Y)->_cnt >= 0\
                    ? *(Y)->_ptr++ = (X) : _flushbuf((X),Y))
    #define	putchar(X)	putc(X,stdout)

    バッファの空きがあれば（stdout._cntを -1して 0か 0より大きければ）
        バッファに 1byteのデータを書き込み stdout._ptrを +1する
    バッファの空きがなければ
        バッファの内容をデバイスに出力する（_flushbuf(X,stdout)する）
    */

    /* 標準出力、標準エラー出力 */
    if ((stdout == &_iob[fd]) || (stderr == &_iob[fd]))
    {
        conout(buf, size);
        return size;
    }

    /* ファイル */
    _FILEBUFF[fd]->DTA_written = 1; /* DTAに書き込みがあったことを記録 */
    for (count = 0; count < size; count++)
    {
        if (!DTAvalid(fd))
        {
            /* open直後とディスクに書き出した直後DTAバッファに有効なデータが無い */
            memset(_FILEBUFF[fd]->DTA, 0, DTASIZE);      /* バッファをクリア */
            _FILEBUFF[fd]->DTA_ptr = _FILEBUFF[fd]->DTA; /* ポインタをバッファの先頭に移動 */

            /* ランダムレコードが0か
            （ランダムレコードが0なのは、ファイルオープン後まだ読み書きされていないときだけ） */
            if ((0 == _FILEBUFF[fd]->FCB.rndrecordL) && (0 == _FILEBUFF[fd]->FCB.rndrecordH))
            {
                /* 次のランダムレコードをセット */
                rndrecordinc(fd);
            }
        }

        *_FILEBUFF[fd]->DTA_ptr++ = *buf++; /* DTAに1バイトコピー */

        /* バッファがいっぱいになったか */
        if (!DTAvalid(fd))
        {
            rndrecorddec(fd); /* FCBのランダムレコードは次のレコード番号を示しているので1引く */
            /* バッファをディスクに書き出す */
            CPMSETDTA(_FILEBUFF[fd]->DTA);
            if (0x00 != CPMRNDWRITE(_FILEBUFF[fd]))
            {
                rndrecordinc(fd); /* 引いた分戻す */
                return -1;        /*エラー*/
            }
            rndrecordinc(fd); /* 引いた分戻す */
            rndrecordinc(fd); /* 次のランダムレコードをセット */
        }
    }
    return size; /* 正常終了 */
}