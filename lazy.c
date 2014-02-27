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
 * lazy.c -- curses like functions and others for ari editor
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
#include <stdio.h>

#ifdef NO_STDLIB_H
extern char *getenv();
#endif

#define _LAZY_C_
#include "lazy.h"

/*
 * OS or machine depended tty control functions
 */
extern void init_tty(), end_tty(), out_tty();

static int cur_x, cur_y;

#define MAX_LINES	40
#define MAX_COLS	120
#define DEFAULT_LINES	25
#define DEFAULT_COLS	80

typedef struct {
	int m, f;
	ic_t col[MAX_COLS];
} scr_line_t;
static scr_line_t scr_line[MAX_LINES];

static char _wbuf[20];		/* work buffer */
#define MOVE(y,x)	(sprintf(_wbuf, "\033[%d;%dH", (y)+1, (x)+1), _wbuf)
#define CLEAR		"\033[H\033[0J"
#define CLRTOEOL	"\033[K"

#ifdef SMALL_POOL
# define OUTSZ 1024
#else
# define OUTSZ 2048
#endif

static char outbuf[OUTSZ];
static int nout = 0;

static void clrtoeol(), outcol(), outstr(), outc(), outflush();

static char *default_codedef = "ENE@J";
static char cur_codedef[] = { 'E', 'N', 'E', '@', 'J', 0};
char *codedef = cur_codedef;
static int k_f = 0, hk_f = 0;
static int stoic(), jetoic(), ictos(), ictoe(), ictoj();

#define PADDING		((ic_t)0x2020)
#define COLBUFSZ	(MAX_COLS*4)
static char colbuf[COLBUFSZ];
static int toscr = 0;

void
initwin()
{
	char *s;

	init_tty();
	LINES = DEFAULT_LINES;
	COLS = DEFAULT_COLS;
	if ((s = getenv("LINES")) != NULL) {
		LINES = atoi(s);
		LINES = (LINES > MAX_LINES)? MAX_LINES:LINES;
	}
	if ((s = getenv("COLUMNS")) != NULL) {
		COLS = atoi(s);
		COLS = (COLS > MAX_COLS)? MAX_COLS:COLS;
	}
	ctrlflag &= ~F_TABEXPAND;
	tabsz = 8; /* default tabsize */
	clear();
	refresh();
}

void
endwin()
{
	end_tty();
	outstr(MOVE(LINES, 0));
	outflush();
}

void
bell()
{
	outc('\07');
	outflush();
}

void
move(y, x)
	int y, x;
{
	cur_x = (x < 0) ? 0 : (x >= COLS) ? COLS-1 : x;
	cur_y = (y < 0) ? 0 : (y >= LINES) ? LINES-1 : y;
}

void
clear()
{
	int n, m;
	scr_line_t *lp;
	ic_t *p;

	m = LINES;
	lp = scr_line;
	while (m--) {
		p = lp->col;
		n = COLS;
		while (n--) *p++ = 0;
		lp->f = 0;
		(lp++)->m = 0;
	}
	move(0,0);
	outstr(CLEAR);
}

static void
clrtoeol()
{
	register int n;
	scr_line_t *lp;
	register ic_t *p;

	lp = &scr_line[cur_y];
	p = &lp->col[cur_x];
	n = COLS - cur_x;
	if (lp->f) {
		if (cur_x == 0) lp->f = 0;
		while (n--) {
			if (*p) lp->m = 1;
			*p++ = 0;
		}
	}
}

void
clrtobot()
{
	register int n;
	int y;
	scr_line_t *lp;
	register ic_t *p;

	clrtoeol();
	for (y = cur_y + 1; y < LINES; y++) {
		lp = &scr_line[y];
		if (lp->f) {
			n = COLS;
			p = lp->col;
			while (n--) {
				if (*p) lp->m = 1;
				*p++ = 0;
			}
			lp->f = 0;
		}
	}
}

void
mvaddstr(y, x, s)
	int y, x;
	register char *s;
{
	move(y, x);
	while(*s) addch((ic_t)*s++);
	clrtoeol();
}

void
mvaddicstr(y, x, s)
	int y, x;
	register ic_t *s;
{
	move(y, x);
	while(*s) addch(*s++);
	clrtoeol();
}

void
addch(ic)
	ic_t ic;
{
	scr_line_t *lp;
	register ic_t *p;
	int dx;

	lp = &scr_line[cur_y];
	p = &lp->col[cur_x];
	lp->f = 1;
	if (ic == '\n') {
		clrtoeol();
		if (cur_y < LINES - 1) move(cur_y+1, 0);
	}
	else {
		if (*p != ic) lp->m = 1;
		*p = ((ctrlflag&F_TABEXPAND)&&(ic=='\t'))?' ':ic;
		if ((dx = icwidth(cur_x, ic)) + cur_x > COLS)
			dx = COLS-cur_x;
		cur_x += dx;
		while (--dx > 0)
		    *(++p) = ((ctrlflag&F_TABEXPAND)&&(ic=='\t'))?' ':PADDING;
		if (cur_x >= COLS && cur_y < LINES - 1) move(cur_y+1, 0);
	}
}

void
refresh()
{
	int y, pos;
	scr_line_t *lp;
	ic_t ic;

	toscr = 1;
	for ( y = 0, lp = scr_line; y < LINES; y++, lp++) {
		if (lp->m) {
			outstr(MOVE(y, 0));
			outstr(CLRTOEOL);
			outcol(lp->col);
			lp->m = 0;
		}
	}
	outstr(MOVE(cur_y, cur_x));
	outflush();
	toscr = 0;
}

static void
outcol(ic_s)
	ic_t *ic_s;
{
	register int n;
	register ic_t *p;
	register char *s;

	p = ic_s;
	n = 0;
	while (*p++ && n++ < COLS) ;
	k_f = hk_f = 0;
	n = (*ictoscr)(ic_s, n, colbuf, COLBUFSZ, 1);
	s = colbuf;
	while(n--) outc(*s++);
}

int
icwidth(x, ic)
	int x;
	ic_t ic;
{
	return ic == '\t'?tabsz-(x%tabsz):ic<0?2:(ic>=0x20||ic=='\n')?1:2;
}

static void
outstr(s)
	char *s;
{
	while (*s) outc(*s++);
}

static void
outc(c)
	int c;
{
	if (nout == OUTSZ) outflush();
	outbuf[nout++] = c;
}

static void
outflush()
{
	if (nout > 0) out_tty(outbuf, nout);
	nout = 0;
}

void
initiocode()
{
	setiocode("-----"); /* copy from default */
}

void
setiocode(p)
	char *p;
{
	char *op = codedef;
	int i;

	for (i = 0 ;i < 5 && p[i] ; i++)
		op[i] = p[i]=='!'? op[i]:(p[i]=='-'?default_codedef[i]:p[i]);

	ctoic   = op[0]=='S'? stoic: jetoic;                   /* read file */
	ictoc   = op[0]=='S'? ictos: op[0]=='J'? ictoj: ictoe; /* write file */
	ictoscr = op[2]=='S'? ictos: op[2]=='J'? ictoj: ictoe; /* to screen */

	if (op[3] != '@' && op[3] != 'B')
		op[3] = '@';
	if (op[4] != 'J' && op[4] != 'B')
		op[4] = 'J';
}

void
stoics(s, ics, l)
	char *s;
	ic_t *ics;
	register int l;
{
	int n;
	n = strlen(s);
	l = n < l? n: l;
	ics[l] = 0;
	while (l--)
		ics[l] = s[l]&0xff;
}

int
wrtofile(fd, buf, len)
	int fd, len;
	ic_t *buf;
{
	int l, n;

	k_f = hk_f = 0;
	if (fd < 0) return 0;
	while (len) {
		l = len < OUTSZ/4 ? len : OUTSZ/4;
		len -= l;
		n = (*ictoc)(buf, l, outbuf, OUTSZ, len == 0);
		buf += l;
		if ( n == OUTSZ) {
		    stoics("*** WARNING: date lost in writing ***\n",
		    	sysbuf, SYSBUF-1);
		}
		if (write(fd, outbuf, n) <= 0)
			break;
	}
	return (len == 0);
}

static int
stoic(s, n, ic_s, max)
	char *s;
	ic_t *ic_s;
	int n, max;
{
	int c;
	int i = 0;
	int ch = 0;
	while (n-- && i < max) {
		c = (*s++)&0xff;
		if (ch) {
			ch = (ch<<1) - ((ch <= 0x9f)?0x00e1:0x0161);
			if (c < 0x9f)
				c -= (c > 0x7f)?0x20:0x1f;
			else {
				c -= 0x7e;
				ch++;
			}
			ic_s[i++] = (ic_t)((ch<<8)+c)|0x8080;
			ch = 0;
		}
		else if ((0x80 <= c && c <= 0x9f)||(0xe0 <= c && c <= 0xff)) {
			ch = c&0xff;
		}
		else
			ic_s[i++] = (ic_t)c;
	}
	return i;
}

static int
ictos(ic_s, n, s, max, m)
	ic_t *ic_s;
	char *s;
	int n, max, m;
{
	ic_t ic;
	int ch, cl;
	int i = 0;
	while(n--) {
		ic = *ic_s++;
		if (ic < 0) {
			if (i >= max - 1) break;
			ch = (ic>>8)&0x7f;
			cl = ic&0x7f;
			s[i++] = ((ch-1)>>1) + ((ch <= 0x5e)?0x71:0xb1);
			s[i++] = cl + ((ch&1)?((cl < 0x60)?0x1f:0x20):0x7e);
		}
		else if (toscr && ic == PADDING) {
			/* NULL */
		}
		else if (toscr && ic < 0x20 && ic != '\t') {
			if (i >= max - 1) break;
			s[i++] = '^';
			s[i++] = (ic&0xff) | 0x40;
		}
		else {
			if (i >= max) break;
			s[i++] = ic&0xff;
		}
	}
	return i;
}

static int
jetoic(s, n, ic_s, max)
	char *s;
	ic_t *ic_s;
	int n, max;
{
	int c;
	int i = 0;
	int ch = 0;
	while (n-- && i < max) {
		c = (*s++)&0xff;
		if (c == '\016' || c == '\017') { /* SO or SI */
			hk_f = c == '\016';
			ch = 0;
			continue;
		}
		else if (c == '\033') { /* ESC */
			if (n && (*s == '$' || *s == '(')) {
				k_f = *s == '$';
				n -= 2;
				s += 2;
				continue;
			}
		}
		if (ch) {
			ic_s[i++] = (ic_t)(((ch&0xff)<<8)+(c&0xff))|0x8080;
			ch = 0;
		}
		else if (hk_f) {
			ic_s[i++] = (ic_t)c|0x80;
		}
		else if ((0x80 <= c && c <= 0xff) || k_f) {
			ch = c;
		}
		else
			ic_s[i++] = (ic_t)c;
	}
	return i;
}

static int
ictoe(ic_s, n, s, max, m)
	ic_t *ic_s;
	char *s;
	int n, max, m;
{
	ic_t ic;
	int i = 0;
	while(n--) {
		ic = *ic_s++;
		if (ic < 0) {
			if (hk_f) {
				if (i >= max) break;
				s[i++] = '\017'; /* SI */
				hk_f = 0;
			}
			if (i >= max - 1) break;
			s[i++] = (ic>>8)&0xff;
			s[i++] = ic&0xff;
		}
		else if (toscr && ic == PADDING) {
			/* NULL */
		}
		else if (toscr && ic < 0x20 && ic != '\t') {
			if (i >= max - 1) break;
			s[i++] = '^';
			s[i++] = (ic&0xff) | 0x40;
		}
		else {
			if (!hk_f && 0xa0 <= ic && ic <= 0xdf) {
				if (i >= max) break;
				s[i++] = '\016'; /* SO */
				hk_f = 1;
			}
			else if (hk_f && 0 <= ic && ic <= 0x7f) {
				if (i >= max) break;
				s[i++] = '\017'; /* SI */
				hk_f = 0;
			}
			if (i >= max) break;
			s[i++] = ic&0x7f;
		}
	}
	if (m && hk_f && i < max) {
		s[i++] = '\017'; /* SI */
	}
	return i;
}

static int
ictoj(ic_s, n, s, max, m)
	ic_t *ic_s;
	char *s;
	int n, max, m;
{
	ic_t ic;
	int i = 0;
	while(n--) {
		ic = *ic_s++;
		if (ic < 0) {
			if (hk_f) {
				if (i >= max) break;
				s[i++] = '\017'; /* SI */
				hk_f = 0;
			}
			if (!k_f) {
				if (i >= max - 3) break;
				s[i++] = '\033';
				s[i++] = '$';
				s[i++] = codedef[3];
				k_f = 1;
			}
			if (i >= max - 1) break;
			s[i++] = (ic>>8)&0x7f;
			s[i++] = ic&0x7f;
		}
		else if (toscr && ic == PADDING) {
			/* NULL */
		}
		else if (toscr && ic < 0x20 && ic != '\t') {
			if (i >= max - 1) break;
			s[i++] = '^';
			s[i++] = (ic&0xff) | 0x40;
		}
		else {
			if (k_f) {
				if (i >= max - 3) break;
				s[i++] = '\033';
				s[i++] = '(';
				s[i++] = codedef[4];
				k_f = 0;
			}
			if (!hk_f && 0xa0 <= ic && ic <= 0xdf) {
				if (i >= max) break;
				s[i++] = '\016'; /* SO */
				hk_f = 1;
			}
			else if (hk_f && 0 <= ic && ic <= 0x7f) {
				if (i >= max) break;
				s[i++] = '\017'; /* SI */
				hk_f = 0;
			}
			if (i >= max) break;
			s[i++] = ic&0x7f;
		}
	}
	if (m && k_f && i < max - 3) {
		s[i++] = '\033';
		s[i++] = '(';
		s[i++] = codedef[4];
	}
	if (m && hk_f && i < max) {
		s[i++] = '\017'; /* SI */
	}
	return i;
}
