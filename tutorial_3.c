/*
秋月電子で売っている Z80用 Cコンパイラ XCC-Vのサンプル

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

#include <stdio.h>
#include <string.h>

char BUFF[128];

/* ファイルに内容を表示する。
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
    unsigned char a, b;
    int c;
    unsigned int record;
    unsigned char pos;

    puts("Hello, world!");
    printf("int:%d float:%f\r\n", 1234, (float)-987.654);
    printf("printf %d %d %02X %04X\r\n", 123, 456, 0x0c, 0x12b);
    printf("printf %s\r\n", "ABCDEFGHIJKLMNOPQRSTUVWXYZ");
    printf("CPM Version %04X\r\n", CPMVER());
    printf("Please enter a string : ");
    gets(BUFF);
    printf("\r\n%s\r\n", BUFF);
    /* printfを使うと実行ファイルのサイズが 20kbくらい増える */

    /* テキストファイルを読むテスト */
    // fp1 = fopen("startup.lis", "r");
    fp1 = fopen("test.txt", "r");
    print("fopen fp:");
    puthexshort((unsigned short)fp1);
    putchar(' ');
    if (NULL != fp1)
    {
        print("fopen OK: read\r\n");
        // put_iob();

        /* テキストファイル終端まで表示する */
        dispfile(fp1, 0xffff);
        print("\r\n");

        /* 読み出し位置変更のテスト */
        if (0 == rewind(fp1))
        {
            print("rewind ");
            dispfile(fp1, 5); /* 5文字表示 */
        }
        else
            print("rewind NG");

        if (0 == fseek(fp1, 8, SEEK_SET))
        {
            print("\r\nfseek SEEK_SET 8 ");
            dispfile(fp1, 5);
        }
        else
            print("fseek NG");

        if (0 == fseek(fp1, 3, SEEK_CUR))
        {
            print("\r\nfseek SEEK_CUR 3 ");
            dispfile(fp1, 5);
        }
        else
            print("fseek NG");

        if (0 == fseek(fp1, -126, SEEK_END))
        {
            print("\r\nfseek SEEK_END -126 ");
            dispfile(fp1, 5);
        }
        else
            print("fseek NG");

        print("\r\n");
    }
    else
        print("fopen NG: read\r\n");

    // put_iob();

    /* 読み込みファイルと同時に別のファイルを開いて書き込んでみる */
    // fp2 = fopen("CON", "w");
    fp2 = fopen("write.$$$", "w"); /* "w"でも読み込みできてしまう */
    if (NULL != fp2)
    {
        print("fopen OK: write\r\n");
        // put_iob();
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
                    fputc('0' + record % 10, fp2);
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

        /* 書き込み位置変更のテスト */
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
        fputs("TEST6 TEXT", fp2);
        fseek(fp2, -128 + 16, SEEK_END); /* ファイル終端からシーク */
        fputs("TEST7 TEXT", fp2);
        /* 書き込んだファイル WRITE.$$$を vscodeのを16新エディタなどで開いて確認する */

        print("\r\n");
        fseek(fp2, 0x100, SEEK_SET);
        dispfile(fp2, 16);
        print("\r\n");
    }
    else
    {
        print("fopen NG: write\r\n");
    }
    // put_iob();

    /*開かれていないファイルポインタをクローズする動作は未定義なので、
    クローズ前に開かれているファイルポインタであることをチェックする必要がある*/
    if (NULL != fp1)
    {
        if (0 == fclose(fp1))
            print("fp1 fclose OK\r\n");
        else
            print("fp1 fclose NG\r\n");
    }

    if (NULL != fp2)
    {
        if (0 == fclose(fp2))
            print("fp2 fclose OK\r\n");
        else
            print("fp2 fclose NG\r\n");
    }
    // put_iob();

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

    CPMPUTS("SP:$");
    puthexshort(_GETSPREG());
}