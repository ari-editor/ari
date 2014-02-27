/*
 *   ari -- Japanese fat ae with mock SKK
 *   Copyright (C) 1991 Yuji Nimura  (nimura@isl.melco.co.jp)
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 1, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *----------------------------------------------------------------------------*
 * kanakan.c -- roman to kana & kana to kanji convertor for ari editor
 *
 * version 1.00,  04.12.1992
 */

#include "config.h"
#include <sys/types.h>
#ifndef NO_STDLIB_H
# include <stdlib.h>
#endif
#ifdef NO_STRING_H
# include <strings.h>
#else
# include <string.h>
#endif
#include <signal.h>
#include <stdio.h>
#include "lazy.h"

#ifdef NO_STDLIB_H
extern char *getenv();
#endif
extern void stoics();
extern int row, col;
extern int index, page, epage;
extern ic_t buf[BUF], *ebuf, *gap, *egap;
extern int sys_line;
extern char *codedef;

static ic_t mode_J[] = {0xa1da, 0xa4a2, 0xa1db, 0};	/* hiragana */
static ic_t mode_q[] = {0xa1da, 0xa5a2, 0xa1db, 0};	/* katakana */
static ic_t mode_L[] = {0xa1da, 0xa3c1, 0xa1db, 0};	/* JIS ALPHA */
static ic_t mode_a[] = {0xa1da,    'a', 0xa1db, 0};	/* ascii */
static ic_t mode_n[] = {0};				/* no indicator */

static int katakana = 0;	/* 0: hirakana, 1: katakana */

#define MODE_A         0   /* ascii */
#define MODE_J         1   /* kana */
#define MODE_L         2   /* JIS ALPHA */
#define MODE_V         3   /* escape 1 char */
static int mode = MODE_A;  /* input mode */

#define HENKAN_Y      1    /* inputting yomi */
#define HENKAN_O      2    /* inputting okuri-gana */
#define HENKAN_L      3    /* henkan with ascii */
#define HENKAN_K      4    /* henkan with hankaku-kana */
#define HENKAN_J   0x10    /* henkan ji_kouho */
#define HENKAN_C   0x20    /* inputting kanji code -- offset */
#define HENKAN_MASK 0xf
static int henkan = 0;     /* status of henkan */

void initkkf(), closekkf(), kinsert();
char **kk_conf();
int lookup();
static void setmsg(), getkcode(), kana(), kana_care(),
            in_yomi(), in_okuri(), out_k(), cparea(), brkpipe();
static int input_ics(), l_ascii(), rtok(), kanakan(), ji_kouho();
#ifdef H_KANA
static int hktok();
#endif
#ifdef UPDATE_DIC
extern void movegap();
extern int pos(), icwidth();
extern ic_t *ptr();
int toroku();
#endif

#define MSG_INCODE 6
static ic_t msg_incode[] = {'c','o','d','e',':',' ',0};

struct kigo {
    char c;
    ic_t ic;
}
  kigo_tbl[] = {
    ',', 0xa1a2, '.', 0xa1a3, '?', 0xa1a9, '!', 0xa1aa,
    '[', 0xa1d6, ']', 0xa1d7, '{', 0xa1d0, '}', 0xa1d1,
    '<', 0xa1e3, '>', 0xa1e4, '-', 0xa1bc,
    0, 0
},
  zkigo_tbl[] = {
    '<', 0xa1e5, '>', 0xa1e6, '[', 0xa1d8, ']', 0xa1d9,
    '-', 0xa1c1, '/', 0xa1a6, '.', 0xa1c4, ',', 0xa1c5,
    0, 0
},
  lkigo_tbl[] = {
    ' ', 0xa1a1, '!', 0xa1aa, '"', 0xa1c9, '#', 0xa1f4, '$', 0xa1f0,
    '%', 0xa173, '&', 0xa175, '\'',0xa1c7, '(', 0xa1ca, ')', 0xa1cb,
    '*', 0xa1f6, '+', 0xa1dc, ',', 0xa1a4, '-', 0xa1dd, '.', 0xa1a5,
    '/', 0xa1bf,
    ':', 0xa1a7, ';', 0xa1a8, '<', 0xa1e3, '=', 0xa1e1, '>', 0xa1e4,
    '?', 0xa1a9,
    '@', 0xa1f7, '[', 0xa1ce, '\\',0xa1ef, ']', 0xa1cf, '^', 0xa1b0,
    '_', 0xa1b2,
    '`', 0xa1ae, '{', 0xa1d0, '|', 0xa1c3, '}', 0xa1d1, '~', 0xa1b1,
    0, 0
};

char boin[] = "aiueo";
char siin[] = "kgszjtdnhfbpmyrwvxc";

struct kana {
    char *s;
    int l;
    ic_t ic[5][2];
} kana_tbl[] = {
 "*xk",2,0xa5f5,     0,     0,     0,     0,     0,0xa5f6,     0,     0,     0,
 "**k",1,0xa4ab,     0,0xa4ad,     0,0xa4af,     0,0xa4b1,     0,0xa4b3,     0,
 "**g",1,0xa4ac,     0,0xa4ae,     0,0xa4b0,     0,0xa4b2,     0,0xa4b4,     0,
 "xts",3,     0,     0,     0,     0,0xa4c3,     0,     0,     0,     0,     0,
 "*ts",2,     0,     0,     0,     0,0xa4c4,     0,     0,     0,     0,     0,
 "**s",1,0xa4b5,     0,0xa4b7,     0,0xa4b9,     0,0xa4bb,     0,0xa4bd,     0,
 "**z",1,0xa4b6,     0,0xa4b8,     0,0xa4ba,     0,0xa4bc,     0,0xa4be,     0,
 "**j",1,0xa4b8,0xa4e3,0xa4b8,     0,0xa4b8,0xa4e5,0xa4b8,0xa4a7,0xa4b8,0xa4e7,
 "*xt",2,     0,     0,     0,     0,0xa4c3,     0,     0,     0,     0,     0,
 "**t",1,0xa4bf,     0,0xa4c1,     0,0xa4c4,     0,0xa4c6,     0,0xa4c8,     0,
 "**d",1,0xa4c0,     0,0xa4c2,     0,0xa4c5,     0,0xa4c7,     0,0xa4c9,     0,
 "**n",1,0xa4ca,     0,0xa4cb,     0,0xa4cc,     0,0xa4cd,     0,0xa4ce,     0,
 "*sh",2,0xa4b7,0xa4e3,0xa4b7,     0,0xa4b7,0xa4e5,0xa4b7,0xa4a7,0xa4b7,0xa4e7,
 "*ch",2,0xa4c1,0xa4e3,0xa4c1,     0,0xa4c1,0xa4e5,0xa4c1,0xa4a7,0xa4c1,0xa4e7,
 "**h",1,0xa4cf,     0,0xa4d2,     0,0xa4d5,     0,0xa4d8,     0,0xa4db,     0,
 "**f",1,0xa4d5,0xa4a1,0xa4d5,0xa4a3,0xa4d5,     0,0xa4d5,0xa4a7,0xa4d5,0xa4a9,
 "**b",1,0xa4d0,     0,0xa4d3,     0,0xa4d6,     0,0xa4d9,     0,0xa4dc,     0,
 "**p",1,0xa4d1,     0,0xa4d4,     0,0xa4d7,     0,0xa4da,     0,0xa4dd,     0,
 "**m",1,0xa4de,     0,0xa4df,     0,0xa4e0,     0,0xa4e1,     0,0xa4e2,     0,
 "*ky",2,0xa4ad,0xa4e3,0xa4ad,0xa4a3,0xa4ad,0xa4e5,0xa4ad,0xa4a7,0xa4ad,0xa4e7,
 "*gy",2,0xa4ae,0xa4e3,0xa4ae,0xa4a3,0xa4ae,0xa4e5,0xa4ae,0xa4a7,0xa4ae,0xa4e7,
 "*sy",2,0xa4b7,0xa4e3,0xa4b7,0xa4a3,0xa4b7,0xa4e5,0xa4b7,0xa4a7,0xa4b7,0xa4e7,
 "*jy",2,0xa4b8,0xa4e3,0xa4b8,0xa4a3,0xa4b8,0xa4e5,0xa4b8,0xa4a7,0xa4b8,0xa4e7,
 "*zy",2,0xa4b8,0xa4e3,0xa4b8,0xa4a3,0xa4b8,0xa4e5,0xa4b8,0xa4a7,0xa4b8,0xa4e7,
 "*ty",2,0xa4c1,0xa4e3,0xa4c1,0xa4a3,0xa4c1,0xa4e5,0xa4c1,0xa4a7,0xa4c1,0xa4e7,
 "*ny",2,0xa4cb,0xa4e3,0xa4cb,0xa4a3,0xa4cb,0xa4e5,0xa4cb,0xa4a7,0xa4cb,0xa4e7,
 "*dy",2,0xa4c2,0xa4e3,0xa4c2,0xa4a3,0xa4c2,0xa4e5,0xa4c2,0xa4a7,0xa4c2,0xa4e7,
 "*hy",2,0xa4d2,0xa4e3,0xa4d2,0xa4a3,0xa4d2,0xa4e5,0xa4d2,0xa4a7,0xa4d2,0xa4e7,
 "*by",2,0xa4d3,0xa4e3,0xa4d3,0xa4a3,0xa4d3,0xa4e5,0xa4d3,0xa4a7,0xa4d3,0xa4e7,
 "*py",2,0xa4d4,0xa4e3,0xa4d4,0xa4a3,0xa4d4,0xa4e5,0xa4d4,0xa4a7,0xa4d4,0xa4e7,
 "*my",2,0xa4df,0xa4e3,0xa4df,0xa4a3,0xa4df,0xa4e5,0xa4df,0xa4a7,0xa4df,0xa4e7,
 "*ry",2,0xa4ea,0xa4e3,0xa4ea,0xa4a3,0xa4ea,0xa4e5,0xa4ea,0xa4a7,0xa4ea,0xa4e7,
 "*xy",2,0xa4e3,     0,     0,     0,0xa4e5,     0,     0,     0,0xa4e7,     0,
 "**y",1,0xa4e4,     0,     0,     0,0xa4e6,     0,     0,     0,0xa4e8,     0,
 "**r",1,0xa4e9,     0,0xa4ea,     0,0xa4eb,     0,0xa4ec,     0,0xa4ed,     0,
 "*xw",2,0xa4ee,     0,     0,     0,     0,     0,     0,     0,     0,     0,
 "**w",1,0xa4ef,     0,0xa4f0,     0,     0,     0,0xa4f1,     0,0xa4f2,     0,
 "**v",1,0xa5f4,0xa5a1,0xa5f4,0xa5a3,0xa5f4,     0,0xa5f4,0xa5a7,0xa5f4,0xa5a9,
 "**x",1,0xa4a1,     0,0xa4a3,     0,0xa4a5,     0,0xa4a7,     0,0xa4a9,     0,
  (char *)0,0,0xa4a2,0,0xa4a4,     0,0xa4a6,     0,0xa4a8,     0,0xa4aa,     0,
};

#ifdef H_KANA
struct h_kana {
	ic_t n, d, h;
} h_kana_tbl[] = {
 ' ', 0, 0,
 0xa1a3, 0, 0, 0xa1d6, 0, 0, 0xa1d7, 0, 0, 0xa1a2, 0, 0, 0xa1a6, 0, 0,
 0xa4f2, 0, 0, /* wo */
 0xa4a1, 0, 0, 0xa4a3, 0, 0, 0xa4a5, 0, 0, 0xa4a7, 0, 0, 0xa4a9, 0, 0, /* xa */
 0xa4e3, 0, 0, 0xa4e5, 0, 0, 0xa4e7, 0, 0, 0xa4c3, 0, 0, /* xya - xtsu */
 0xa1bc, 0, 0, /* - */
 /* a  */
 0xa4a2, 0, 0, 0xa4a4, 0, 0, 0xa4a6, 0xa5f4, 0, 0xa4a8, 0, 0, 0xa4aa, 0, 0,
 /* ka */
 0xa4ab, 0xa4ac, 0,    0xa4ad, 0xa4ae, 0,    0xa4af, 0xa4b0, 0,
 0xa4b1, 0xa4b2, 0,    0xa4b3, 0xa4b4, 0,
 /* sa */
 0xa4b5, 0xa4b6, 0,    0xa4b7, 0xa4b8, 0,    0xa4b9, 0xa4ba, 0,
 0xa4bb, 0xa4bc, 0,    0xa4bd, 0xa4be, 0,
 /* ta */
 0xa4bf, 0xa4c0, 0,    0xa4c1, 0xa4c2, 0,    0xa4c4, 0xa4c5, 0,
 0xa4c6, 0xa4c7, 0,    0xa4c8, 0xa4c9, 0,
 /* na */
 0xa4ca, 0, 0, 0xa4cb, 0, 0, 0xa4cc, 0, 0, 0xa4cd, 0, 0, 0xa4ce, 0, 0,
 /* ha */
 0xa4cf, 0xa4d0, 0xa4d1,   0xa4d2, 0xa4d3, 0xa4d4,   0xa4d5, 0xa4d6, 0xa4d7,
 0xa4d8, 0xa4d9, 0xa4da,   0xa4db, 0xa4dc, 0xa4dd,
 /* ma */
 0xa4de, 0, 0, 0xa4df, 0, 0, 0xa4e0, 0, 0, 0xa4e1, 0, 0, 0xa4e2, 0, 0,
 /* ya */
 0xa4e4, 0, 0, 0xa4e6, 0, 0, 0xa4e8, 0, 0,
 /* ra */
 0xa4e9, 0, 0, 0xa4ea, 0, 0, 0xa4eb, 0, 0, 0xa4ec, 0, 0, 0xa4ed, 0, 0,
 /* wa,n */
 0xa4ef, 0, 0, 0xa4f3, 0, 0,
 0, 0, 0 /* terminator */
};
#endif

static int c0 = 0, c1 = 0, c2 = 0;    /* history of input char */
static ic_t *bp,    /* base position */
            *cp,    /* current position */
            *ep,    /* end postion -- bp <= cp < ep */
            *mark,  /* henkan mark position */
            *okuri; /* start postion of okuri-gana */
static int tailc;   /* last char of yomi -- not indecate */

static void
setmode()
{
    ic_t *p;
    ic_t *q = sysbuf;
    if ((henkan & ~HENKAN_MASK) == HENKAN_C) {
        p = msg_incode;
    }
    else {
        switch (mode) {
        case MODE_A:
        case MODE_V: p = (ctrlflag&F_ASCII_INDICATE)?mode_a:mode_n; break;
        default:
        case MODE_J: p = (katakana)? mode_q: mode_J; break;
        case MODE_L: p = mode_L; break;
        }
    }
    while ((*q++ = *p++)) ;
}

void
kinsert()
{
    int c;

    bp = (ctrlflag&F_BRKFENCE)?buf:gap;
    cp = gap;
    ep = egap;
    c0 = c1 = c2 = tailc = 0;
    mark = okuri = (ic_t *)0;
    while (1) {
        gap = cp;
        index = pos(egap);
        setmode();
        display();
        c = getch();
        if ((henkan & ~HENKAN_MASK) == HENKAN_J) {
            if ((c = ji_kouho(c)) == ' ' || c == 0)
                continue;
        }
        if (!input_ics(c))
            break;
        if ((henkan & ~HENKAN_MASK) == HENKAN_C) {
            getkcode();
            henkan &= HENKAN_MASK;
        }
    }
    gap = cp;
    index = pos(egap);
    sysbuf[0] = 0;
}

static int
input_ics(c)
    register int c;
{
    if (c == '\033' && mode != MODE_V) {
        /* end of input */
        kana_care();
        out_k();
        return 0;
    }
    switch (mode) {
    case MODE_A: /* ascii */
    case MODE_L: /* JIS ALPHA */
        if (c == '\b') {
            if (bp < cp)
                --cp;
            else
                bell();
        }
        else if (c == '\026' && mode == MODE_A) {    /* ^V */
            if (cp < ep)
                *cp++ = '^';
            mode = MODE_V;
        }
        else if (c == '\012') {    /* ^J */
            mode = MODE_J;
        }
        else {
            if (mode == MODE_L) c = l_ascii(c);
            if (cp < ep)
                *cp++ = c=='\r'? '\n': c;
        }
        break;
    case MODE_V: /* escape 1 char */
        *cp = c;
        mode = MODE_A;
        break;
    case MODE_J: /* kana */
        kana(c);
        break;
    }
    return 1;
}

static int
l_ascii(c)
    register int c;
{
    struct kigo *p;

    if ( 'a' <= c && c <= 'z')
        c += 0xa3e1 - 'a';
    else if ( 'A' <= c && c <= 'Z')
        c += 0xa3c1 - 'A';
    else if ( '0' <= c && c <= '9')
        c += 0xa3b0 - '0';
    else if ( 0x20 <= c && c < 0x7f) {
        p = lkigo_tbl;
        while (p->c != c && p->c)
            p++;
        c = p->c ? p->ic : c;
    }
    return c;
}

static void
kana(c)
    register int c;
{
    ic_t *p;

    if (c == ' ') {
        kana_care();
        if (henkan) {
            if (kanakan())
                henkan |= HENKAN_J;
        }
        else
            if (cp < ep) *cp++ = c;
    }
    else if (c == '\012') {	/* ^J */
        kana_care();
        out_k();
    }
    else if (c == '\b') {
        if (bp < cp) {
            c2 = c1; c1 = c0; c0 = 0;
            --cp;
        }
        else
            bell();
        if (cp <= okuri)
            okuri = (ic_t *)0;
        if (cp <= mark + 1)
            out_k();
    }
#ifdef H_KANA
    else if ((c == '/' && c2 != 'z') || c == '\013') { /* ^K */
#else
    else if (c == '/' && c2 != 'z') {
#endif
        kana_care();
        out_k();
        in_yomi();
        henkan = c=='/'? HENKAN_L: HENKAN_K;
    }
#ifdef H_KANA
    else if (0xa0 <= c && c <= 0xdf) {
        if (henkan != HENKAN_K || c <= 0xa5 || c == 0xb0) out_k();
        cp -= hktok(c, &p);
        if (cp < bp) {
            cp = bp;
            bell();
        }
        c = *p;
        if (!henkan && katakana
                && (ic_t)0xa4a1<=c && c<=(ic_t)0xa4f3) c += 0x100;
        if (cp < ep)
            *cp++ = c;
    }
#endif
    else if (henkan == HENKAN_L && 0x20 < c && c < 0x7f) {
        if (cp < ep)
            *cp++ = c;
    }
    else if (c == 'l') {
        kana_care();
        out_k();
        mode = MODE_A;
    }
    else if (c == 'q') {
        kana_care();
        out_k();
        katakana = (!katakana);
    }
    else if (c == 'L') {
        kana_care();
        out_k();
        mode = MODE_L;
    }
    else if (c == '\\') {
        kana_care();
        henkan |= HENKAN_C;
    }
    else if (0 < c && c < 0x20) {
        kana_care();
        out_k();
        if (cp < ep)
            *cp++ = c=='\r'? '\n': c;
    }
    else {
        if ('A' <= c && c <= 'Z') {
            c += 'a' - 'A';
            if (henkan)
                in_okuri(c);
            else {
                kana_care();
                in_yomi();
            }
        }
        else if (henkan && (c<'a' || 'z'<c) && !(c=='\''&&c2=='n')) {
            out_k();
        }
        cp -= rtok(c, &p);
        if (cp < bp) {
            cp = bp;
            bell();
        }
        while(*p && cp < ep) {
            if (!henkan && katakana
                && (ic_t)0xa4a1<=*p && *p<=(ic_t)0xa4f3) *p += 0x100;
            *cp++ = *p++;
        }
    }
}

static void
getkcode()
{
    int sav_row, sav_col;
    int i, n, c;
    ic_t *p;

    sys_line = 1;
    p = sysbuf;
    sav_row = row;
    sav_col = col;
    row = LINES - 1;
    col = MSG_INCODE;
    setmode();
    display();
    while ((n = getch()) != '\r') {
        if (n == '\b') {
            p[--col] = 0;
            if (col < MSG_INCODE) break;
        }
        else if (n == '\033') { /* ESC -- escape */
            col = 0;
            break;
        }
        else {
            if (col < COLS && col < SYSBUF) {
                p[col++] = n;
                p[col] = 0;
            }
            else
                bell();
        }
        display();
    }
    if (col > MSG_INCODE) {
        p += MSG_INCODE;
        for (n = 0, i = 0; i < 4 && (c = *p); p++) {
            if ('0'<=c && c<= '9')
                c -= '0';
            else if ('a'<=c && c<= 'f')
                c += 10 - 'a';
            else if ('A'<=c && c<= 'F')
                c += 10 - 'A';
            else
                break;
            n = (n<<4) + c;
        }
        n |= 0x8080;
        c = (n>>8)&0x7f;
        i = n&0x7f;
        if (cp < ep && 0x20 < c && c < 0x7f && 0x20 < i && i < 0x7f)
            *cp++ = n;
    }
    sys_line = 0;
    row = sav_row;
    col = sav_col;
}

#ifdef H_KANA
static int
hktok(c, pp)
    register int c;
    ic_t **pp;
{
    int l, n;
    static ic_t ic; /* keep value out of this scope */

    *pp = &ic;
    ic = c;
#if 0 /* debug with normal keyboard
       *    sa => ha, xa => dakuten, xtsu => handakuten
       */
    c = c==0xbb? 0xca: c==0xa7? 0xde: c==0xaf? 0xdf: c;
    ic = c;
#endif
    l = 0;
    if (0 < c && c < 0x7f) {
        c0 = c1 = c2 = 0;
        return 0;
    }
    if (0xa0 <= c && c <= 0xdd) {
        c2 = c;
        ic = h_kana_tbl[c-0xa0].n;
    }
    else if (c == 0xde || c == 0xdf) {
        ic = 0;
        if (0xa0 <= c2 && c2 <= 0xdd) {
            n = c2 - 0xa0;
            ic = c==0xde ? h_kana_tbl[n].d: h_kana_tbl[n].h;
            if (ic) {
                l = 1;
            }
        }
        if (ic == 0)
            ic = c==0xde ? 0xa1ab: 0xa1ac; /* dakuten/handakuten */
        c0 = c1 = c2 = 0;
    }
    return l;
}
#endif

static void
kana_care()
{
    if (c2 == 'n') {
        *(cp-1) = (!henkan && katakana)? 0xa5f3: 0xa4f3; /* n */
    }
    c0 = c1 = c2 = 0;
}

static void
in_yomi()
{
    henkan = HENKAN_Y;
    mark = cp;
    if (cp < ep) {
        *cp++ = 0xa2a6;
    }
}

static void
in_okuri(c)
{
    if (henkan != HENKAN_O) {
        henkan = HENKAN_O;
        okuri = cp;
        tailc = c;
    }
}

static void
out_k()
{
    int n;

    if (henkan) {
        henkan = 0;
        if (cp >= mark + 1) {
            n = cp - mark - 1;
            cparea(mark+1, mark, n);
            cp = mark + n;
        }
        else
           cp = mark;
        mark = okuri = (ic_t *)0;
    }
}

static int
rtok(c, pp)
    int c;
    ic_t **pp;
{
    struct kigo *kigop;
    struct kana *kp;
    ic_t *icp;
    static ic_t out[3];
    int i, l;

    *pp = out;
    l = 0;
    out[0] = out[1] = out[2] = 0;
    if ('a'<=c&&c<='z') {
        for (i = 0; boin[i] != c && boin[i] ; i++) ;
        if (boin[i]) {
            for ( kp = kana_tbl; kp->s ; kp++) {
                if (kp->s[2]==c2
                  &&(kp->s[1]==c1||kp->s[1]=='*')
                  &&(kp->s[0]==c0||kp->s[0]=='*'))
                    break;
            }
            icp = kp->ic[i];
            if (*icp) {
                l = kp->l;
                out[0] = *icp++;
                out[1] = *icp++;
            }
            else {
                out[0] = c;
            }
            c0 = c1 = c2 = 0;
        }
        else {
            for ( i = 0; siin[i] != c && siin[i] ; i++) ;
            if (siin[i]) {
                if (c2 == 'n' && c == 'n' && codedef[1] == 'n') {
                    l = 1;
                    out[0] = 0xa4f3; /* n */
                    c0 = c1 = c2 = 0;
                }
                else {
                    if (c2 == 'n' && c != 'y') {
                        l = 1;
                        out[0] = 0xa4f3; /* n */
                        out[1] = c;
                        c1 = c2 = 0;
                    }
                    else if (c2 == c) {
                        l = 1;
                        out[0] = 0xa4c3; /* xtsu */
                        out[1] = c;
                        c1 = c2 = 0;
                    }
                    else {
                        out[0] = c;
                    }
                    c0 = c1; c1 = c2; c2 = c;
                }
            }
            else {
                out[0] = c;
                c0 = c1 = c2 = 0;
            }
        }
    }
    else if ('0'<=c&&c<='9') {
        if (c2 == 'n')
            out[l++] = 0xa4f3; /* n */
        out[l] = c  - '0' + 0xa3b0;
        c0 = c1 = c2 = 0;
    }
    else if (0x20 < c && c < 0x7f) {
        if (c2 == 'n' && c == '\'')
            out[l++] = 0xa4f3; /* n */
        else {
            if (c2 == 'n')
                out[l++] = 0xa4f3; /* n */
            kigop = c2 == 'z' ? zkigo_tbl: kigo_tbl;
            while (kigop->c != c && kigop->c)
                kigop++;
            c = kigop->c == c ? kigop->ic : c;
            out[l] = c;
            if (c2 == 'z') l++;
        }
        c0 = c1 = c2 = 0;
    }
    return l;
}

static void
cparea(src, dst, n)
    ic_t *src, *dst;
    int n;
{
    if (src < dst) {
        src += n;
        dst += n;
        while (n-- > 0) *(--dst) = *(--src);
    }
    else if (dst < src) {
        while (n-- > 0) *dst++ = *src++;
    }
}

#define LEN 100
static char body[LEN];
static ic_t tail[LEN], work[LEN];
static char ans[LEN];
static int nbody, ntail, nans;

#define CMD_NEXT	"N\n"
#define CMD_LAST	"L\n"
#define CMD_VER		"V\n"
#define CMD_CONF	"C\n"
#define CMD_UPDATE	"U\n"
#define CMD_QUIT	"Q\n"

#ifdef MERGE
/*
 * initkkf() -- defined in skksrch.c
 */
#else

static int kana_out, kanji_in; /* fds of pipe to/from skksrch */
static char *exec_param[] = {
    "sh", "-c", (char *)buf, (char *)0
};
static int kkf_pid = 0;

static void
brkpipe()
{
    kana_out = kanji_in = (-1);
    kkf_pid = 0;
    stoics("unable to use kanakan -- process dead.", sysbuf, SYSBUF-1);
}

void
initkkf()
{
    char *p;
    int fds0[2], fds1[2];

    if ((p = getenv("ARI_KKF")) != NULL) {
        signal(SIGPIPE, brkpipe);
        pipe(fds0);
        pipe(fds1);
        if ((kkf_pid = fork()) == 0) {
            /* child */
            close(0); dup(fds0[0]);
            close(1); dup(fds1[1]);
            close(fds0[1]); close(fds1[0]);
            strncpy(exec_param[2], "exec ", BUF);
            strncat(exec_param[2], p, BUF - strlen(exec_param[2]));
            execvp(exec_param[0], exec_param);
            fprintf(stderr, "ARI_KKF: can not excute %s\n", p);
        }
        close(fds0[0]); close(fds1[1]);
        kana_out = fds0[1];
        kanji_in = fds1[0];
        if (lookup(CMD_VER, 2, ans, LEN) < 0) goto error;
        return; /* O.K. */
    }
error:
    stoics("unable to use kanakan. -- no process", sysbuf, SYSBUF-1);
    kana_out = kanji_in = (-1);
}

int
lookup(body, n, ans, len)
    char *body, *ans;
    int n, len;
{
    if (kana_out == (-1) || kanji_in == (-1))
        return (-1);
    if (write(kana_out, body, n) == n) {
        if ((n = read(kanji_in, ans, len)) > 0)
            return n;
    }

    close(kana_out);
    close(kanji_in);
    brkpipe();
    return (-1);
}
#endif

void
closekkf()
{
    if (lookup(CMD_VER, 2, ans, LEN) > 0) {
        /* process is alive */
#ifdef UPDATE_DIC
        if (lookup(CMD_UPDATE, 2, ans, LEN) < 0 || ans[0] == '0')
            fprintf(stderr, "FAIL to update local jisyo\n");
        else if (ans[0] == '1')
            fprintf(stderr, "update local jisyo\n");
#endif
        lookup(CMD_QUIT, 2, ans, LEN);
    }
    /* else: process is dead -- nothing to do */
}

static int
icstodics(p, len, s, max)
    ic_t *p;
    char *s;
    int len, max;
{
    char *sav;

    --max;
    sav = s;
    while (*p != 0 && *p != '\n' && len-- > 0 && s - sav < max) {
        if (*p < 0) *s++ = (*p>>8)&0xff;
        *s++ = (*p++)&0xff;
    }
    return (s - sav);
}

static int
dicstoics(s, len, p, max)
    char *s;
    ic_t *p;
    int len, max;
{
    ic_t *sav;

    sav = p;
    while (*s != 0 && *s != '\n' && len-- > 0 && p - sav < max) {
        if (*s & 0x80) {
            if (len-- < 1) break;
            *p++ = ((s[0]<<8)&0xff00) + (s[1]&0xff);
            s += 2;
        }
        else
            *p++ = (*s++)&0xff;
    }
    return (p - sav);
}

static int
kanakan()
{
    ic_t *p;
    char *s;
    int c, n;

    *mark = 0xa2a7;
    body[0] = 'S'; /* command search */
    n = ((okuri) ? okuri: cp) - mark - 1;
    nbody = 1;
    nbody += icstodics(mark+1, n, &body[1], LEN-5);
    if (okuri) body[nbody++] = tailc;
    body[nbody++] = ' ';
    body[nbody++] = '\n';
    body[nbody] = 0;
    if (okuri) {
        ntail = cp - okuri;
        if (ntail > LEN-2) ntail = LEN-2;
        cparea(okuri, tail, ntail);
        tail[ntail++] = 0;
    }
    else
        tail[ntail = 0] = 0;

    /* search key */
    if (lookup(body, nbody, ans, LEN) == -1 || ans[0] == '0') {
        out_k();
        return 0;
    }
    if (ji_kouho(' ') != ' ') {
        out_k();
        return 0;
    }
    return 1;
}

static int
ji_kouho(c)
    int c;
{
    ic_t *p;
    char *s;
    int n;

    if(c != ' ' && c != 'x') {
        out_k();
#ifdef UPDATE_DIC
        if(c != '\b') { /* not BS */
            body[0] = 'R'; /* replace word position */
            for (nbody = 0 ; body[nbody] && body[nbody] != '\n'; nbody++) ;
            body[nbody++] = '/';
            for (n = 1; ans[n] && ans[n] != '\n' && n < LEN-4 ; n++)
                body[nbody++] = ans[n];
            body[nbody++] = '/';
            body[nbody++] = '\n';
            body[nbody] = 0;
            nans = lookup(body, nbody, ans, LEN);
        }
#endif
        return c;
    }

    if ((nans = lookup(c==' '?CMD_NEXT:CMD_LAST, 2, ans, LEN)) == -1) {
        out_k();
        return 0;
    }
    if (ans[0] == '0') {
        cp = mark;
        in_yomi();
        s = &body[1];
        for (nans = 0; s[nans] != ' '; nans++) ;
        if (okuri) --nans;
    }
    else {
        cp = mark+1;
        s = &ans[1];
    }
    n = 0;
    while (*s != '\n' && *s && n < nans) {
        c = (*s++)&0xff;
        n++;
        if (0x80 <= c && c <= 0xff) {
            c = (c<<8) + ((*s++)&0xff);
            n++;
        }
        if (cp < ep) *cp++ = c;
    }
    p = tail;
    while (c = *p++)
        if (cp < ep) *cp++ = c;
    if (ans[0] == '0') {
        henkan &= HENKAN_MASK;
        return 0;
    }
    return ' '; /* success */
}

int
toroku()
{
#ifdef UPDATE_DIC
    static ic_t msg1[] = {0xc3b1, 0xb8ec, 0xc0df, 0xc4ea, 0xa1a7, 0},
                msg2[] = {0xc3b1, 0xb8ec, 0xa1a7, 0xa1d6, 0},
                msg3[] = {0xa1d7, 0xa1a1, 0xa4e8, 0xa4df, 0xa1a7, 0},
                msg_fail[] = {0xc5d0, 0xcfbf,
                              0xa4c7, 0xa4ad, 0xa4de, 0xa4bb, 0xa4f3, 0};
    int i, sav, len, sav_row, sav_col, sav_kana, rval, markcol;
    ic_t *wordp, *p, *q, *markp;

    rval = 1;
    movegap();
    if ((i = *ptr(index)) == ' ' || i == '\n' || i == '/')
        return 1;
    sys_line = 0;
    cparea(msg1, sysbuf, 5);   /* set msg1 */
    markp = &sysbuf[5];   /* set mark on next of msg1 */
    markcol = 10;         /* need 10 char length for msg1 */
    p = ptr(sav = index);

    /* input word */
    while(1) {
        q = ptr(index);
        i = q - p;
        cparea(p, markp, i);
        markp[i] = 0;
        display();
        if ((i = getch()) == 'l') {   /* move right */
            if ((i = *q) == '\n' || i == '/' || index >= pos(ebuf))
                bell();
            else {
                /* need 30 char length for indicator of mode, word & yomi */
                for ( i = markcol, q = markp; *q ; i += icwidth(i, *q++)) ;
                if (i - markcol < COLS - 30)
                    index++;
                else
                    bell();
            }
        }
        else if (i == 'h') {          /* move left */
            if (sav < index)
                --index;
            else
                bell();
        }
        else if (i == '\r') {         /* CR -- set region */
            break;
        }
        else if (i == '\033') {       /* ESC -- escape toroku */
            index = sav;
            return 1;
        }
    }
    if ((len = index - sav) == 0) {
        return 1;    /* null string */
    }
    wordp = ptr(sav);

    /* input yomi */
    index = sav;
    sav = mode;
    sav_kana = katakana;
    sav_row = row;
    sav_col = col;
    sys_line = 1;
    mode = MODE_J;
    katakana = 0;
    row = LINES - 1;
    henkan = 0;
    bp = cp = work;
    ep = &work[LEN];
    while(1) {
        setmode();
        for(p = sysbuf, col = 0; *p && col < 8; col += icwidth(col, *p++)) ;
        while(col < 8) {
            *p++ = ' ';
            col++;
        }
        q = p;                  /* save point */
        cparea(msg2, p, 4);     /* set msg2 */
        p += 4;
        cparea(wordp, p, len);  /* set key */
        p += len;
        cparea(msg3, p, 5);     /* set msg3 */
        p += 5;
        while ( q < p) col += icwidth(col, *q++);
        q = p;
        markp = p;              /* set markp on next of msg3 */
        markcol = col;          /* set markcol on next col of msg3 */
        cparea(bp, p, cp - bp); /* set yomi */
        p += cp - bp;
        *p = 0;
        while ( q < p) col += icwidth(col, *q++);
        display();
        i = input_ics(getch());
        out_k();
        katakana = 0;
        if (!i) goto escape;
        if ((i = *(cp-1)) == '\n')
            break;
        if (i == ' ' || i == '/' || i == '\t') { /* illegal char for yomi */
            if (bp < cp) --cp;
            bell();
        }
        else {
            /* check space */
            for (i=markcol, q=bp; q < cp && i<COLS-1; i+=icwidth(i,*q++)) ;
            if (i >= COLS-1) bell();
            cp = q;
        }
    }
    if (bp == cp-1)
        goto escape;    /* null string */

    /* entry */
    i = 0;
    body[i++] = 'E';
    i += icstodics(bp, cp - bp, &body[1], LEN-5);
    body[i++] = ' ';
    body[i++] = '/';
    i += icstodics(wordp, len, &body[i], LEN-4-i);
    body[i++] = '/';
    body[i++] = '\n';
    body[i] = 0;
    if (lookup(body, i, ans, LEN) < 0 || ans[0] == '0') {
        cparea(msg_fail, sysbuf, 8);     /* set msg of fail */
        rval = 0;
    };
escape:
    mode = sav;
    katakana = sav_kana;
    row = sav_row;
    col = sav_col;
    return rval;
#else
    return 1;
#endif
}

char **
kk_conf()
{
    static char *msg[] = {
#ifdef MERGE
        "merged ari -- stand alone type",
#else
        "ari -- standard type",
#endif
        (char *)0, /* message from kana-kanji convertor */
        (char *)0};
    int i;

    if (lookup(CMD_CONF, 2, ans, LEN) < 0)
        msg[1] = "unable to use kanakan";
    else if (ans[0] == '0')
        msg[1] = "available to use kanakan (not support to update jisyo)";
    else {
         i = strlen(ans);
         if (ans[i-1] == '\n') ans[i-1] = 0;
         msg[1] = &ans[1];
    }
    return msg;
}
