/*
秋月電子で売っている Z80用 Cコンパイラ XCC-Vのサンプル

windows11で使用するとIDEが不安定で、
プロジェクトの保存やプロジェクトからファイルの除外を行うと
IDEが異常終了したり、プロジェクトに登録してあるファイルが消えたりするので、
IDEを使わずにコマンドラインから使うと良い。

This code is provided under a CC0 Public Domain License.
http://creativecommons.org/publicdomain/zero/1.0/

2024年12月11日 佐藤恭一 kyoutan.jpn.org

プログラム実行中に徐々にスタックを消費してゆく
アセンブラからアセンブラのサブルーチンを繰り返し呼び出してもスタックは消費せず正常。
標準入出力オープンした後の_iob内容が変じゃないかな？ゴミが入っている
    ->printfでおかしなアドレスへの書き込みが行われている
コンパイル時 "-O o" オプションで最適化すると実行中スタックがおかしな変化をする。
"-O" オプション無しで最適化を行わないとスタックの変な変動は無く正常。
*/
#include "cpm.h"
#include <stdio.h>
#include <ctype.h>

void puthex(unsigned char a);
void puthexshort(unsigned short);
void print(char *str);
void *sbrk(unsigned int size);
void put_iob(void);
void memdump(unsigned short address, unsigned short size);

unsigned short func(unsigned short a, unsigned char b)
{
    return a + b;
    ;
}

char buff[0x100];

main(int argc, char *argv[])
{
    // void *ptr1, *ptr2;
    FILE *fp, *fp2;
    // signed char ch;
    unsigned short sp;
    unsigned char a, b;
    int c;
    unsigned int record;
    unsigned char pos;

    print("main start SP:");
    sp = _GETSPREG();
    puthexshort(sp);
    print("\r\n");
    put_iob();
    // putchar('-');
    puts("puts");
    put_iob();
    // printf("printf\r\n"); /*文字列だけなら異常なさそう*/
    //  printf("printf %d %x\r\n", 1234, 0xabcd); /*数字をいれると_iob壊れる*/
    put_iob();

    a = 1;
    b = 2;

    a = func(a, b);
    a++;
    b++;
    CPMPUTS("Hello! MSX-DOS!! \r\n$");
    /* printfを使うと20kbくらいオブジェクトサイズが増える */
    // printf("printf %d %d %02X %04X\r\n", 123, 456, 0x0c, 0x12b);
    // printf("printf %d %d %02X\r\n", 123, 456, 0x0c);
    // printf("CPM Version %04X\r\n", CPMVER());
    /* %dや%xを使うと_iob[9]くらいに表示する文字コードが書き込まれる。異常なのかワークとして使われているのか不明*/
    // printf("printf %s", "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    // printf("printf %s\r\n", "ABC");/* %sは_iobの変な書き込みは行われない*/
    // printf("printf %f", (float)-98765.4321);/* %fは_iobの変な書き込みは行われない */
    puts("");
    put_iob();
    fp = fopen("test.txt", "r");
    print("fopen fp:");
    puthexshort((unsigned short)fp);
    putchar(' ');
    if (NULL != fp)
    {
        puts("fopen OK");
        put_iob();

        while (1)
        {
            c = fgetc(fp);
            if (EOF == c)
            {
                puts(" EOF");
                break;
            }
            /* 0x1A（テキストファイルの終端）をコンソールに出力するとカーソルが移動するかなにかで文字が一部消えるので、
            0x1Aが来たら終了する。 */
            if (0x1a == c)
            {
                puts(" 0x1A EOF");
                break;
            }
            putchar(c);
        }
        if (0 == rewind(fp))
            putchar(fgetc(fp));
        else
            puts("rewind NG");
        if (0 == fseek(fp, 5, SEEK_SET))
            putchar(fgetc(fp));
        else
            puts("fseek NG");
        /* SEEK_CURで1レコードだけのファイルでファイルが0で埋まる 直す */
        if (0 == fseek(fp, 3, SEEK_CUR))
            putchar(fgetc(fp));
        else
            puts("fseek NG");
        if (0 == fseek(fp, -126, SEEK_END))
            putchar(fgetc(fp));
        else
            puts("fseek NG");

        put_iob();
    }
    else
        puts("fopen NG");
    put_iob();

    fp2 = fopen("write.txt", "r+"); /* "w"でも読み込みできてしまう */
    if (NULL != fp2)
    {
        puts("fopen write OK");
        put_iob();
        for (record = 0; record < 6; record++)
        {
            for (pos = 0; pos < 128; pos++)
            {
                if (0 == pos)
                {
                    putchar('0' + record % 10);
                    /* レコードの先頭ならレコード番号を三桁で出力 */
                    fputc('0' + record / 100, fp2);
                    fputc('0' + record / 10, fp2);
                    puthex(fputc('0' + record % 10, fp2));
                    pos += 2;
                }
                else if (126 == pos)
                {
                    /* レコードの最後に改行を入れる */
                    fputs("\r\n", fp2);
                    pos += 1;
                }
                else
                {
                    /* 残りのレコードを埋める */
                    fputc('#', fp2);
                }
            }
        }
        fputs("TEST1 TEXT", fp2);
        fseek(fp2, 128 * 2, SEEK_CUR); /* 現在位置からシーク */
        fputs("TEST2 TEXT", fp2);
        fseek(fp2, 0x0500, SEEK_SET); /* ファイル先頭からシーク */
        fputs("TEST3 TEXT", fp2);
        fseek(fp2, 0, SEEK_CUR); /* 現在位置からシーク */
        fputs("TEST4 TEXT", fp2);
        fseek(fp2, -16, SEEK_END); /* ファイル終端からシーク */
        fputs("TEST5 TEXT", fp2);
        fseek(fp2, 0, SEEK_END); /* ファイル終端からシーク */
        fputs("TEST5 TEXT", fp2);
        fseek(fp2, -128 + 16, SEEK_END); /* ファイル終端からシーク */
        fputs("TEST5 TEXT", fp2);
        /*シークとライト動作良好。リードモディファイライトのテストする。*/
        puts("");
        fseek(fp2, 0x100, SEEK_SET);
        for (pos = 0; pos < 16; pos++)
        {
            putchar(fgetc(fp2));
        }
        puts("");
    }
    put_iob();
    if (0 == fclose(fp))
        puts("fp fclose OK");
    else
        puts("fp fclose NG");
    if (0 == fclose(fp2))
        puts("fp2 fclose OK");
    else
        puts("fp2 fclose NG");
    put_iob();

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

    CPMPUTS("SP:$");
    sp = _GETSPREG();
    puthexshort(sp);
}