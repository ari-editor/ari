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
 * skksrch -- kana-kanji convertor using SKK-JISYO for ari editor
 *
 * Skksrch's basic algorithm to look up SKK dictionary file is from skkserv
 * version 3.5.2 of May 13, 1991, which is a utility of SKK. Skkserv is
 * programmed and copyrighted by Yukiyoshi Kameyama (kam@sato.riec.tohoku.ac.jp).
 *
 * It is jonney@MIX's suggestion to save and restore index table of a SKK
 * disctionary. He also implemented this function as 'IDX_SAVE' option to keep
 * their binary image in files which were isolated from the SKK dictionaries.
 */
static char *version = "skksrch version 1.00,  04.12.1992\n";

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
#include <stdio.h>
#include <errno.h>

#ifdef NO_STDLIB_H
extern char *getenv();
#endif

/*  history
 *    09.23.1991 -- starting
 *    10.02.1991 -- add version command
 *    10.10.1991 -- use entrybuf and add Last command
 *    10.15.1991 -- minor changed
 *    10.27.1991 -- support MERGE mode
 *    02.23.1992 -- support indexed jisyo
 *    03.09.1992 -- support to update local dictonary
 *    03.31.1992 -- use config.h
 */

/*   input -- stdin, output -- stdout.
 *
 * input:
 *   "Seee\n"        Search key "eee"
 *   "N\n"           Next word
 *   "L\n"           Last word
 *   "Eeee /ttt/\n"  Enter word "ttt" with key "eee" -- update_dic option
 *   "Reee /ttt/\n"  Replace word "ttt" of key "eee" -- update_dic option
 *   "V\n"           Version.
 *   "U\n"           update local dictionary (old one is renamed to save.)
 *   "C\n"           Configration and Condition
 *   "Q\n"           Quit
 *
 *   --- following is debug mode only ---
 *   "p\n"           print pos table and base
 *   "f\n"           print frame list -- update_dic option
 *   "s file\n"      save new dictionary named "file" -- update_dic option
 *
 *
 * command and reply:
 *   "Seee\n"    -- "1\n"     found
 *                  "0\n"     not found
 *   "N\n","L\n" -- "1ttt\n"  next term/last term "ttt"
 *                  "0\n"     no more term
 *   "V\n"       -- "1vvv\n"  version message "vvv"
 *   "U\n"       -- "2\n"     no need to update
 *                  "1\n"     update success
 *                  "0\n"     update fail
 *   "C\n"       -- "1ccc\n"  configration and condition "ccc"
 *   others      -- "1\n"     success
 *                  "0\n"     fail
 */

#ifndef MERGE
# define Debug(x)	if (debug) fprintf x
#else
# define Debug(x)
#endif

typedef unsigned  code_t;

#define BUFSIZE		(256)
#define ENTRYBUF	(2048)
#define KANA_MIN	(0xa4a1)
#define KANA_MAX	(0xa4f3)
#define KANA_SIZE	(KANA_MAX - KANA_MIN +1)
#define POS_SIZE	(KANA_SIZE + 3)

#ifdef MSDOS
# define DEFAULT_LDIC	"\\LOCALDIC.SKK"
# define DIC_RDMODE	"rb"
# define DIC_WRMODE	"wb"
# define SAV_SUFFIX	".BAK"
#else  /* minix, bsd, sysv, etc */
# define DEFAULT_LDIC	"/.skk-jisyo"
# define DIC_RDMODE	"r"
# define DIC_WRMODE	"w"
#endif

static
struct jdic_tbl {
    FILE *fp;
    long pos[POS_SIZE]; /* pos[0]         less than KANA_MIN    */
                        /* pos[1]           KANA_MIN            */
                        /* pos[KANA_SIZE]   KANA_MAX            */
                        /* pos[KANA_SIZE+1] more than KANA_MAX  */
                        /* pos[KANA_SIZE+2] terminator          */
    long base;  /* point of first entry */
} l_tbl, g_tbl;

static char
     cbuf[BUFSIZE],
     entrybuf[ENTRYBUF],
     *cmdname,
     *ldic_name = NULL,
     *gdic_name = NULL,
     *src_dic   = NULL,
     *start_index_mark = " Start of Index\n",
     *end_index_mark   = " End of Index\n";

static int curpos, debug;

#ifdef UPDATE_DIC
static char ldbuf[BUFSIZE]; /* keep ldic name */
typedef struct frame {
    struct frame *next, *following;
    char word[1];
} frame_t;

#define NO_FRAME ((frame_t *)0)
frame_t f_post;
/*
 * NOTE for frame_t
 *   key.next       -- point to next key
 *                     NO_FRAME means no more key
 *   key.following  -- point to first word of this key
 *   key.word       -- key string
 *   word.following -- point to next word of same key
 *                     NO_FRAME means no more word
 *   word.next      -- point to next entered word
 *                     if first enterd word then point to f_post
 *                     if NO_FRAME then not entered word
 *   word.word      -- word string
 *
 * f_post.next      -- pointer to first key
 * f_post.following -- pointer to last enterd word
 */
#endif

#ifdef MERGE
int initkkf(), lookup();
static void adds(), addc();
#else
int main();
static void main_loop(), prusage(), wr_postbl();
static int mk_index_dic();
#endif
static void skip_entry(), init_tbl();
static code_t calcode();
static int codetoidx(), find_key(), rd_index_dic();
#ifdef UPDATE_DIC
#ifndef MERGE
static void pr_frames();
#endif
static void init_frame(), sav_words();
static frame_t *get_frame(), *srch_key(), *add_key(), *srch_word();
static int cmpword(), add_word(),
       copy_entry(), find_words(), enter_word(),
       mk_newdic(), sav_dic();
#endif

#ifdef MERGE
initkkf()
#else
main(ac, av)
    int ac;
    char **av;
#endif
{
    int i;
    char *p;

#ifndef MERGE
    cmdname = av[0];

    for (i = 1 ; i < ac ; i++) {
        if (strcmp(av[i], "-debug") == 0) {
            debug++;
        }
        else if (strcmp(av[i],"-global") == 0) {
            if (++i >= ac) {
                goto error;
            }
            gdic_name = av[i];
        }
        else if (strcmp(av[i], "-index") == 0) {
            if (++i >= ac) {
                goto error;
            }
            src_dic = av[i];
        }
        else if (strcmp(av[i], "-to") == 0) {
            if (src_dic == NULL || ++i >= ac) {
                goto error;
            }
            ldic_name = av[i]; /* tmporary use */
        }
        else if (strcmp(av[i], "-help") == 0) {
            goto error;
        }
        else {
            if (ldic_name != NULL) {
                goto error;
            }
            ldic_name = av[i];
        }
    }

    if (src_dic != NULL) {
        if (! mk_index_dic(src_dic, ldic_name)) {
            goto error;
        }
        exit (0);
    }
#endif

    if (gdic_name == NULL) {
        gdic_name = getenv("SKK_JISYO");
    }
    Debug((stderr,"global jisyo : %s\n",gdic_name==NULL? "NONE": gdic_name));
    if (ldic_name == NULL) {
        if ((ldic_name = getenv("LOCAL_SKK_JISYO")) == NULL) {
            if ((p = getenv("HOME")) == NULL) {
                fprintf(stderr, "can not find $HOME\n");
                exit(1);
            }
            ldic_name = cbuf;
            strncpy(ldic_name, p, BUFSIZE-1);
            i = strlen(ldic_name);
            if (ldic_name[i-1] == '/' || ldic_name[i-1] == '\\')
                ldic_name[i-1] = 0;
            strncat(ldic_name, DEFAULT_LDIC, BUFSIZE - strlen(ldic_name) -1);
        }
    }
#ifdef UPDATE_DIC
    strncpy(ldbuf, ldic_name, BUFSIZE-1); /* keep local dic name */
#endif
    Debug((stderr, "local jisyo  : %s\n", ldic_name));

    /*
     * initialize
     */
    if (gdic_name != NULL) {
        if ((g_tbl.fp = fopen(gdic_name, DIC_RDMODE)) == NULL) {
            fprintf(stderr,
                "%s : can not open global jisyo -- IGNORE\n", gdic_name);
        }
    }
    else {
        g_tbl.fp = NULL;
    }

    if ((l_tbl.fp = fopen(ldic_name, DIC_RDMODE)) == NULL) {
        fprintf(stderr, "%s : can not open local jisyo\n", ldic_name);
#ifndef MERGE
        goto error;
#endif
    }
    init_tbl(&g_tbl);
    init_tbl(&l_tbl);

#ifdef UPDATE_DIC
    init_frame();
#endif
#ifndef MERGE
    Debug((stderr, "enter loop\n"));
    main_loop();
    exit(0);

error:
    prusage();
    exit(1);
#endif
}

#ifndef MERGE
static void
prusage()
{
    fprintf(stderr, "Usage : %s [-debug] [-global jisyo] [jisyo]\n", cmdname);
    fprintf(stderr, "Usage : %s -index jisyo [-to target]\n", cmdname);
    fprintf(stderr, "        write indexed jisyo to target or stdout\n");
    exit(1);
}

static void
wr_postbl(fp)
    FILE *fp;
{
    int i;
    long l;

    for ( i = l = 0 ; i < POS_SIZE ; i++) {
        fprintf(fp, " %09ld", l_tbl.pos[i]);
        l += 10;
        if (l >= 70) {
            fputc('\n', fp);
            l = 0;
        }
    }
    fputc('\n', fp);
}

static int
mk_index_dic(s, to)
    char *s;
    char *to;
{
    int i, l;
    FILE *fp;

    if ((l_tbl.fp = fopen(s, DIC_RDMODE)) == NULL) {
        fprintf(stderr, "%s : can not open jisyo to read\n", s);
        return 0;
    }
    Debug((stderr, "read original jisyo from %s\n", s));
    if (to == NULL) {
        fp = stdout;
        Debug((stderr, "write indexed jisyo to stdout\n"));
    }
    else if ((fp = fopen(to, DIC_WRMODE)) == NULL) {
        fprintf(stderr, "%s : can not open indexed jisyo to write\n", to);
        return 0;
    }
    else {
        Debug((stderr, "write indexed jisyo to %s\n", to));
    }

    /* read index */
    init_tbl(&l_tbl);

    /* write index */
    fputs(start_index_mark, fp);
    wr_postbl(fp);
    fputs(end_index_mark, fp);

    /* copy body */
    fseek(l_tbl.fp, l_tbl.base, 0);
    while (fgets(entrybuf, ENTRYBUF - 1, l_tbl.fp) != NULL) {
        fputs(entrybuf, fp);
    }
    fflush(fp);
    fclose(l_tbl.fp);
    if (to != NULL) fclose(fp);
    return 1;
}
#endif

static code_t
calcode(c1, c2)
    int c1, c2;
{
    return (code_t)(((c1 & 0xff) << 8) + (c2 & 0xff));
}

static int
codetoidx(code)
    code_t code;
{
    return (int)(code < KANA_MIN) ? 0 :
        ((code > KANA_MAX) ? KANA_SIZE + 1 : code - KANA_MIN + 1);
}

static void
skip_entry(fp)
    FILE *fp;
{
    int c;

    while ((c = fgetc(fp)) != '\n' && c != EOF)
        ;
}

/*
 * initilize index table
 */
static void
init_tbl(p)
    struct jdic_tbl *p;
{
    int idx, cur_idx, c1, c2;
    long cur_pos;
    FILE *fp;

    if ((fp = p->fp) == NULL)
        return;
    /* we should not read index at making indexed jisyo */
    if (src_dic == NULL && rd_index_dic(p)) {/* read indexed jisyo */
        Debug((stderr, "read index from jisyo\n"));
        return;
    }

    /* make index table */
    Debug((stderr, "make index table\n"));
    fseek(fp, 0L, 0);
    idx = 0;
    p->pos[idx] = 0L;
    while ((c1 = fgetc(fp)) == ' ') {
        skip_entry(fp);
    }
    if (c1 == EOF) {
        fseek(fp, 0L, 2);
        p->base = ftell(fp);
        while (idx < KANA_SIZE + 2) {
            p->pos[++idx] = 0L;
        }
        return;
    }
    p->base = cur_pos = ftell(fp) - 1L;
    fseek(fp, cur_pos, 0);
    while(idx < KANA_SIZE) {
        if ((c1 = fgetc(fp)) == EOF) break;
        if ((c2 = fgetc(fp)) == EOF) break;
        cur_idx = codetoidx(calcode(c1, c2));
        cur_pos = ftell(fp);
        while(idx < cur_idx) {
            p->pos[++idx] = cur_pos - 2L - p->base;
        }
        skip_entry(fp);
    }
    fseek(fp, 0L, 2);
    cur_pos = ftell(fp) - p->base;
    while (idx < KANA_SIZE + 2) {
        p->pos[++idx] = cur_pos ;
    }
    return;
}

/*
 * read index table from indexed jisyo
 */
static int
rd_index_dic(tblp)
    struct jdic_tbl *tblp;
{
    FILE *fp;
    int idx, c;
    long l;
    char *p;

    fp = tblp->fp;
    if (fgets(entrybuf, ENTRYBUF - 1, fp) == NULL)
        return 0;
    if (strcmp(entrybuf, start_index_mark) != 0)
        return 0;

    /* read index */
    l = 0;
    idx = 0;
    while (fgets(entrybuf, ENTRYBUF - 1, fp) != NULL) {
        if (entrybuf[0] != ' ')
            return 0; /* wrong index */
        if (strcmp(entrybuf, end_index_mark) == 0)
            break;
        p = entrybuf;
        while (idx < POS_SIZE) {
            while ((c = *p) && (c < '0' || '9' < c)) p++;
            if (c == 0)
                break;
            for (l = 0; '0' <= (c = *p++) && c <= '9'; l = l*10 + c-'0') ;
            tblp->pos[idx++] = l;
        }
    }
    while (idx < POS_SIZE)
        tblp->pos[idx++] = l;

    /* skip wrong entry */
    while ((c = fgetc(fp)) == ' ') {
        skip_entry(fp);
    }
    if (c == EOF)
        return 0;
    tblp->base = ftell(fp) - 1L;

    /* check index data */
    for (idx = 1 ; idx < POS_SIZE; idx++) {
        if (tblp->pos[idx-1] > tblp->pos[idx])
            return 0; /* wrong index */
    }
    /* check file size */
    fseek(fp, 0L, 2);
    if (ftell(fp) != tblp->base + tblp->pos[POS_SIZE-1])
        return 0; /* wrong file size */
    return 1;
}

/*
 * find key from dictionary
 */
static int
find_key(s, p, l)
    char *s;
    struct jdic_tbl *p;
    int l;
{
    int i, c;
    long start, end;

    if (p->fp == NULL) return 0;

    i = codetoidx(calcode(s[0], s[1]));
    start = p->pos[i] + p->base;
    end = p->pos[i + 1] + p->base;
    fseek(p->fp, start, 0);
    while(start < end) {
        for (i = 0; (c=(s[i]&0xff)) >= ' ' && c == (fgetc(p->fp)&0xff); i++) {
            if(c == ' ') {
                fgets(&entrybuf[l], ENTRYBUF - l - 1, p->fp);
                l = strlen(entrybuf);
                while (l > 0 && entrybuf[l] != '/')
                    --l;
                entrybuf[l] = 0;
                return l; /* success */
            }
        }
        skip_entry(p->fp);
        start = ftell(p->fp);
    }
    return 0; /* not found */
}

#ifdef UPDATE_DIC
/*
 * enter new words and make new local_skk_jisyo
 */

static int frame_used;
static char *frame_pool[FRAME_POOL];

static void
init_frame()
{
    int i;

    for( i = 0; i < FRAME_POOL; i++ )
        frame_pool[i] = NULL;
    f_post.next = NO_FRAME;
    f_post.following = &f_post;
    f_post.word[0] = 0; /* null string */
    frame_used = 0;
}

/*
 * get new frame for given string from pool
 */
static frame_t *
get_frame(s)
    char *s;
{
    int l, idx;
    frame_t *p;

    if (frame_used >= FRAME_POOL)
        return NO_FRAME; /* no more space */
    p = (frame_t *)&(frame_pool[frame_used]);
    l = &(p->word[strlen(s)+1]) - ((char *)frame_pool);

    idx = l / sizeof(char *);
    if (l % sizeof(char *)) idx++;
    if (idx >= FRAME_POOL)
        return NO_FRAME; /* no more space */
    frame_used = idx;
    p->next = p->following = NO_FRAME;
    strcpy(p->word, s);
    return p;
}

/*
 * compare words (stirngs with 0 or space terminator)
 */
static int
cmpword(s1, s2)
    char *s1, *s2;
{
    register int c1, c2;

    do {
        if ((c1=(*s1++)&0xff) == ' ') c1 = 0;
        if ((c2=(*s2++)&0xff) == ' ') c2 = 0;
    } while(c1 && c1 == c2);
    return c1 - c2;
}

/*
 * search key -- return pointer to key frame or NULL when not find
 */
static frame_t *
srch_key(s)
    char *s;
{
    frame_t *p;
    int i;

    for (p = f_post.next; p ; p = p->next) {
        if ((i=cmpword(s, p->word)) == 0) { /* same */
            return p;
        }
        else if (i < 0)
            break; /* no key */
    }
    return NO_FRAME;
}

/*
 * add key -- if no key then make key and return pointer to key frame
 */
static frame_t *
add_key(s)
    char *s;
{
    frame_t *p, *q, *tmp;
    int i;

    for (q = &f_post, p = f_post.next; p ; q = p, p = q->next) {
        if ((i=cmpword(s, p->word)) == 0) { /* same */
            return p;
        }
        else if (i < 0)
            break; /* no key */
    }
    if (tmp = get_frame(s)) {
        /* get frame -- insert to list */
        q->next = tmp;
        tmp->next = p;
    }
    return tmp;
}

#if 0 /* never used */
/*
 * search word -- return pointer to entry frame or NULL when not find
 */
static frame_t *
srch_word(keyp, s)
    frame_t *keyp;
    char *s;
{
    frame_t *p;

    for (p = keyp->following; p ; p = p->following) {
        if (cmpword(s, p->word) == 0)
            break;
    }
    return p;
}
#endif

/*
 * add word with key -- return 1 as success and 0 as fail
 */
static int
add_word(keyp, s, enter)
    frame_t *keyp;
    char *s;
    int enter;
{
    frame_t *p, *q;

    for (q = keyp, p = keyp->following; p ; q = p, p = q->following) {
        if (cmpword(s, p->word) == 0) {
            /* same entry */
            if (p == keyp->following) {
                /* top entry -- no work */
                goto skip;
            }
                /* delete from list */
            q->following = p->following;
            goto add_list;
        }
    }
    if ((p = get_frame(s)) == NO_FRAME)
        return 0; /* can not get frame -- fail */
add_list:
    /* insert as top entry  */
    Debug((stderr,"add_word:'%s' word:'%s' used:%05d\n",keyp->word,s,frame_used));
    p->following = keyp->following;
    keyp->following = p;
skip:
    if (enter && p->next == NO_FRAME) {
        p->next = f_post.following; /* add to entered word list */
        f_post.following = p;
    }
    return 1; /* success */
}

#ifndef SMALL_POOL
/*
 * read an entry of ldic and copy to word list
 */
static int
copy_entry(keyp, base)
    frame_t *keyp; /* pointer to key frame */
    char *base; /* entry */
{
    register int c;
    register char *tail;
    frame_t *p, *q;

    c = *base&0xff;
    while (c) {
        tail = ++base;
        while ((c = (*tail&0xff)) && c != '/')
            tail++;
        *tail = 0;
        for (q = keyp, p = keyp->following; p ; q = p, p = q->following) {
            if (cmpword(base, p->word) == 0)
                break; /* already has same word */
        }
        if (p == NO_FRAME) {
            if ((p = q->following = get_frame(base)) == NO_FRAME)
                return 0; /* can not get frame -- error */
        }
        if (p->next == NO_FRAME) {
            p->next = f_post.following; /* add to entered word list */
            f_post.following = p;
        }
        base = tail;
    }
    return 1;
}
#endif

/*
 * find words from word list
 */
static int
find_words(s, n)
    char *s;
    int n;
{
    frame_t *p;

    if (p = srch_key(s)) {
        for(p = p->following; n < ENTRYBUF && p ; p = p->following) {
            entrybuf[n++] = '/';
            strncpy(&entrybuf[n], p->word, ENTRYBUF-n-1);
            n = strlen(entrybuf);
        }
    }
    return n;
}

/*
 * enter new word to word list
 */
static int
enter_word(s, enter)
    char *s;
    int enter;
{
    int c;
    char *p;
    frame_t *keyp;

    c = strlen(s);
    while (c > 0 && s[c] != '/')
        --c;
    s[c] = 0;
    p = s;
    while ((c=(*p++&0xff)) && c != ' ')
        ;
    if (c != ' ')
        return 0; /* wrong format */
    if ((*p&0xff) != '/')
        return 0; /* wroing format */
    *p++ = 0;
    Debug((stderr, "key:'%s' word:'%s'\n", s, p));
    if (keyp = srch_key(s)) {
        return add_word(keyp, p, enter);
    }
    if (keyp = add_key(s)) {
#ifndef SMALL_POOL
        if (find_key(s, &l_tbl, 0)) {
            /* copy entry of dictionary to word list */
            if (!copy_entry(keyp, entrybuf))
                return 0; /* can not copy */
        }
#endif
        return add_word(keyp, p, enter);
    }
    return 0; /* error */
}

#ifndef MERGE
static void
pr_frames(fp)
    FILE *fp;
{
    frame_t *keyp, *p;

    fprintf(fp, " data list -- used: %d\n", frame_used);
    for (keyp = f_post.next; keyp ; keyp = keyp->next) {
        fprintf(fp, " %s :", keyp->word);
        for (p = keyp->following ; p ; p = p->following) {
            fprintf(fp, " %s", p->word);
            if (p->next) fputc('*', fp);
        }
        fprintf(fp, "\n");
    }
}
#endif

/*
 * make new dictionary which is merged internel list and local_dic
 */
static int
mk_newdic(s)
    char *s;
{
    frame_t *keyp, *p;
    FILE *fp;
    int i, c;
    char *base, *tail;

    if ((fp = fopen(s, DIC_WRMODE)) == NULL)
        return 0;
    fseek(l_tbl.fp, l_tbl.base, 0);
    keyp = f_post.next;
    i = 0; /* initial value to force reading */
    while(keyp) {
        if ( i >= 0) {
            if (fgets(entrybuf, ENTRYBUF - 1, l_tbl.fp) == NULL) {
                sav_words(fp, keyp, 1);  /* output current list */
                while (keyp = keyp->next)
                    sav_words(fp, keyp, 1);
                fclose(fp);
                return 1;
            }
        }
        i = cmpword(keyp->word, entrybuf);
        if (i <= 0) {
            /* output from list */
            sav_words(fp, keyp, (i<0)); /* output with '\n' when not same */
            if (i == 0) {
                /* list and ldic have same key --
                 *   check list has same word for each in ldic entry,
                 *   and output to file to recover copy_entry() fail if not.
                 */
                for (base = entrybuf; *base && *base != '/'; base++) ;
                for (c = strlen(base); c > 0 && base[c] != '/'; --c) ;
                base[c] = 0;
                for (c = *base; c ; base = tail) {
                    tail = ++base;
                    while ((c = *tail) && c != '/')
                        tail++;
                    *tail = 0;
                    for (p = keyp->following; p ; p = p->following) {
                        if (cmpword(base, p->word) == 0)
                            break; /* already has same word */
                    }
                    if (p == NO_FRAME) { /* not same word in list */
                        fputc('/', fp);
                        fputs(base, fp);
                    }
                }
                fputs("/\n", fp);
            }
            if ((keyp = keyp->next) == NO_FRAME) {
                if (i < 0)
                    fputs(entrybuf, fp); /* output current entrybuf */
                break;
            }
        }
        else {
            /* output from ldic */
            fputs(entrybuf, fp);
        }
    }
    while (fgets(entrybuf, ENTRYBUF - 1, l_tbl.fp) != NULL)
        fputs(entrybuf, fp);
    fflush(fp);
    fclose(fp);
    return 1;
}

/*
 * save list related keyp to fp
 */
static void
sav_words(fp, keyp, eol)
    FILE *fp;
    frame_t *keyp;
    int eol;
{
    frame_t *p;

    for (p = keyp->following ; p && p->next == NO_FRAME ; p = p->following) ;
    if (p) {
        fputs(keyp->word, fp);
        for (p = keyp->following ; p ; p = p->following)
            if (p->next != NO_FRAME)
                fprintf(fp, "/%s", p->word);
        if (eol) fputs("/\n", fp);
    }
}

/*
 * save old dictionary
 */
extern int errno;
static int
sav_dic(s)
    char *s;
{
#ifdef MSDOS
    /* make backup file name from given string
     * if string has suffix, change to SAV_SUFFIX,
     * and if not, only append SAV_SUFFIX.
     */
    int c;
    char *p, *q;
    strncpy(entrybuf, s, ENTRYBUF-1);
    p = q = &entrybuf[strlen(entrybuf)];
    while ((c=(*p))!=':' && c!='\\' && c!='/' && c!='.' && p>entrybuf) --p;
    if (c != '.') p = q;
    *p = 0;
    strncpy(p, SAV_SUFFIX, ENTRYBUF - strlen(entrybuf) - 1);
#else  /* minix, bsd, sysv, etc */
    /* make backup file name from given string -- only append '~'.
     */
    strncpy(entrybuf, s, ENTRYBUF-1);
    strncat(entrybuf, "~", ENTRYBUF - strlen(entrybuf) - 1);
#endif
    Debug((stderr, "sav_dic:'%s' as '%s'\n", s, entrybuf));
#ifndef NO_RENAME
    unlink(entrybuf);
    return (rename(s, entrybuf) == 0);
#else
    if (link(s, entrybuf) == 0)
        return (unlink(s) == 0);
    else if (errno == EEXIST) {
        return (unlink(entrybuf) == 0
                && link(s, entrybuf) == 0 && unlink(s) == 0);
    }
    else
        return 0;
#endif
}
#endif

/*
 * main function -- look up key and return entry, etc
 */

#ifdef MERGE
#define fputs(x, y)	adds(reply, x)
#define fputc(x, y)	addc(reply, x)
#define fflush(x)
static int repl_len;

static void
adds(p, s)
    char *p, *s;
{
    strncat(p, s, repl_len - strlen(p));
}

static void
addc(p, c)
    char *p;
    int c;
{
    char s[2];
    s[0] = c;
    s[1] = 0;
    strncat(p, s, repl_len - strlen(p));
}

int
lookup(cmd, cmdlen, reply, len)
    char *cmd, *reply;
    int cmdlen, len;
#else
static void
main_loop()
#endif
{
    int n, l;
    char *ack;

#ifdef MERGE
    strncpy(cbuf, cmd, BUFSIZE-1);
    repl_len = len;
    reply[0] = 0; /* set to null string */
    {
#else
    while(fgets(cbuf, BUFSIZE - 1, stdin) != NULL) {
#endif
        switch (cbuf[0]) {
        case 'S':
            curpos = 0;
            entrybuf[0] = 0;
            n = strlen(cbuf);
            if (cbuf[n-1] == '\n') cbuf[n-1] = ' ';
            Debug((stderr, "search key :%s\n", &cbuf[1]));
#ifdef UPDATE_DIC
            if (l = n = find_words(&cbuf[1], 0)) {
                Debug((stderr, "list <%s>\n", entrybuf));
            }
            else
#endif
            if (l = n = find_key(&cbuf[1],&l_tbl, 0)) {
                Debug((stderr, "ldic <%s>\n", entrybuf));
            }
            if (l += find_key(&cbuf[1],&g_tbl, l)) {
                Debug((stderr, "gdic <%s>\n", &entrybuf[n]));
            }
            else {
                Debug((stderr, "not found\n"));
            }
            Debug((stderr, "entrybuf <%s>\n", entrybuf));
            fputs(l ? "1\n" : "0\n", stdout);
            fflush(stdout);
            break;
        case 'N':
            if (entrybuf[curpos] == 0) fputs("0\n", stdout);
            else {
                fputc('1', stdout);
                while ((n = entrybuf[++curpos]) != '/' && n != 0)
                    fputc(n, stdout);
                fputc('\n', stdout);
            }
            fflush(stdout);
            break;
        case 'L':
            while (curpos > 0 && entrybuf[--curpos] != '/') ;
            if (curpos == 0) fputs("0\n", stdout);
            else {
                fputc('1', stdout);
                n = l = curpos;
                while (l > 0 && entrybuf[--l] != '/') ;
                l++;
                while (l < n)
                    fputc(entrybuf[l++], stdout);
                fputc('\n', stdout);
            }
            fflush(stdout);
            break;
        case 'V':
            fputs("1", stdout);
            fputs(version, stdout);
            fflush(stdout);
            break;
#ifdef UPDATE_DIC
#ifndef SMALL_POOL
        case 'R':
#endif
        case 'E':
            fputs(enter_word(&cbuf[1], (cbuf[0]=='E'))? "1\n":"0\n", stdout);
            entrybuf[curpos=0] = 0; /* clear entrybuf */
            fflush(stdout);
            break;
#endif
        case 'Q':
#ifdef MERGE
            if (g_tbl.fp != NULL) fclose(g_tbl.fp);
            if (l_tbl.fp != NULL) fclose(l_tbl.fp);
#else
            Debug((stderr, "exit\n"));
            return;
#endif
            break;
        case 'C':
#ifdef UPDATE_DIC
#ifdef SMALL_POOL
            sprintf(cbuf, "1skksrch; restricted (used:%d/%d)\n",
               frame_used, FRAME_POOL);
#else
            sprintf(cbuf, "1skksrch (used:%d/%d)\n", frame_used, FRAME_POOL);
#endif
#else
            sprintf(cbuf, "1skksrch; minimun (not support to update jisyo)\n");
#endif
            fputs(cbuf, stdout);
            fflush(stdout);
            break;
#ifdef UPDATE_DIC
        case 'U':
            if (f_post.following == &f_post) {
                Debug((stderr, "not need to update ldic\n"));
                fputs("2\n", stdout);
            }
            else {
                fputs((sav_dic(ldbuf) && mk_newdic(ldbuf))? "1\n": "0\n",
                      stdout);
                entrybuf[curpos=0] = 0; /* clear entrybuf */
            }
            fflush(stdout);
            break;
#endif
#ifndef MERGE
#ifdef UPDATE_DIC
        case 'f':
            if (debug) pr_frames(stdout);
            fputs(debug? "1\n":"0\n", stdout);
            fflush(stdout);
            break;
        case 's':
            if (debug) {
                n = strlen(cbuf);
                while (n >= 2 && cbuf[n] <= ' ') --n;
                cbuf[n+1] = 0;
                fprintf(stderr, "newdic to %s\n", &cbuf[2]);
                fputs((n > 2 && mk_newdic(&cbuf[2]))? "1\n": "0\n", stdout);
                entrybuf[curpos=0] = 0; /* clear entrybuf */
            }
            else
                fputs("0\n", stdout);
            fflush(stdout);
            break;
#endif
        case 'p':
            if (debug) {
                wr_postbl(stdout);
                fprintf(stdout, " base = %09ld\n", l_tbl.base);
                fputs("1\n", stdout);
                fflush(stdout);
                break;
            }
            /* drop to default */
#endif
        default:
            Debug((stderr, "bad sequence:%s", cbuf));
            fputs("0\n", stdout); /* error */
            fflush(stdout);
            break;
        }
    }
#ifdef MERGE
    return strlen(reply);
#endif
}
