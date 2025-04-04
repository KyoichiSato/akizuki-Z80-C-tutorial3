/*
コマンドライン引数をセットして main 関数を呼び出す
CP/M 2.2 / MSX-DOS 用

This code is provided under a CC0 Public Domain License.
http://creativecommons.org/publicdomain/zero/1.0/

2025年1月29日 作成 佐藤恭一 kyoutan.jpn.org
*/

#include <string.h>

#define ARG_MAX 20                            /* コマンドライン引数の最大値 */
#define ARG_LENGTH (*(unsigned char *)0x0080) /* 0x0080にコマンドライン引数の文字数が入っている */

int argc;
char *argv[ARG_MAX];
char ARG_BUFF[0x7F]; /* コマンドライン引数を格納するバッファ
 （CP/Mでは最大0x0081 - 0x00FF の126バイト）だけど、終端文字を追加するかもしれないので127バイト確保する */

/* argc、argVをセットしてmain関数を呼び出す。
戻ってきたらCP/MのファンクションコールでOSに戻る */
void CALL_MAIN(void)
{
    char *ptr;

    memset(ARG_BUFF, 0, sizeof(ARG_BUFF)); /* バッファを終端文字で初期化 */
    argv[0] = "";                          /* CP/Mでは argv[0]（実行ファイル名）を取得できないので、空文字列にしておく */
    argc = 0;

    /* コマンドライン引数があるか */
    if (1 < ARG_LENGTH)
    {
        /* コマンドライン引数がある （文字数が1より多い。1の時は' 'のみ）*/
        /* CP/Mのコマンドライン 0x0080 - 0x00FF （DTAのデフォルトアドレス）は
        ユーザーが書き換える可能性があるので、別の場所にコピーする */
        memcpy(ARG_BUFF, (void *)0x0081, 0x7e);
        /* 文字列を走査して' 'で分割する */
        for (ptr = ARG_BUFF; ptr < (ARG_BUFF + ARG_LENGTH); ptr++)
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
