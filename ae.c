/*
 *	ae.c		Anthony's Editor  IOCCC '91
 *
 *	Public Domain 1991 by Anthony Howe.  All rights released.
 */

#ifdef ARI
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
 */

static char *version   = "ari version 1.00, 04.12.1992";
static char *copyright = "ari version 1.00, Copyright (C) 1991 Yuji Nimura";
static char *short_notice[] = {
    (char *)0, /* here comes copyright */
    "Ari comes with ABSOLUTELY NO WARRANTY.",
    "This is free software, and you are welcome to redistribute it",
    "under certain conditions.",
    "Type `:notice' for details.",
    (char *)0
};
static char *full_notice[] = {
    "ari -- Japanese fat ae with mock SKK",
    (char *)0, /* here comes copyright */
    "",
    "This program is free software; you can redistribute it and/or modify",
    "it under the terms of the GNU General Public License as published by",
    "the Free Software Foundation; either version 1, or (at your option)",
    "any later version.",
    "",
    "This program is distributed in the hope that it will be useful,",
    "but WITHOUT ANY WARRANTY; without even the implied warranty of",
    "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the",
    "GNU General Public License for more details.",
    "",
    "You should have received a copy of the GNU General Public License",
    "along with this program; if not, write to the Free Software",
    "Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.",
    "",
    "",
    "<Hit any key to return>",
    (char *)0
};

# include "config.h"
# ifndef NO_STDLIB_H
#  include <stdlib.h>
# else
   extern char *getenv();
# endif
# include <stdio.h>
# include "lazy.h"
#else
#include <ctype.h>
# include <curses.h>
#endif

int done;
int row, col;
int index, page, epage;
#ifdef ARI
ic_t buf[BUF];
ic_t *ebuf;
ic_t *gap = buf;
ic_t *egap;
char *fbuf;
int sys_line = 0;
int counter = 0;
int modified = 0;
#define REGBUF 1023
ic_t regbuf[REGBUF+1];
extern char *codedef;
#else
char buf[BUF];
char *ebuf;
char *gap = buf;
char *egap;
#endif
char *filename;

/*
 *	The following assertions must be maintained.
 *
 *	o  buf <= gap <= egap <= ebuf
 *		If gap == egap then the buffer is full.
 *
 *	o  point = ptr(index) and point < gap or egap <= point
 *
 *	o  page <= index < epage
 *
 *	o  0 <= index <= pos(ebuf) <= BUF
 *
 *
 *	Memory representation of the file:
 *
 *		low	buf  -->+----------+
 *				|  front   |
 *				| of file  |
 *			gap  -->+----------+<-- character not in file 
 *				|   hole   |
 *			egap -->+----------+<-- character in file
 *				|   back   |
 *				| of file  |
 *		high	ebuf -->+----------+<-- character not in file 
 *
 *
 *	point & gap
 *
 *	The Point is the current cursor position while the Gap is the 
 *	position where the last edit operation took place. The Gap is 
 *	ment to be the cursor but to avoid shuffling characters while 
 *	the cursor moves it is easier to just move a pointer and when 
 *	something serious has to be done then you move the Gap to the 
 *	Point. 
 */

int adjust();
int nextline();
int pos();
int prevline();
#ifdef ARI
extern ic_t *ptr();
extern void initkkf(), closekkf();
extern char **kk_conf();
static int isspace(), icclass(), cmd(), count();
static void setup();
static ic_t *paramvalue();
#else
char *ptr();
#endif

void bottom();
void delete();
void display();
void down();
#ifdef ARI
int  file();
#else
void file();
#endif
void insert();
void left();
void lnbegin();
void lnend();
void movegap();
void pgdown();
void pgup();
void redraw();
void right();
void quit();
void top();
void up();
void wleft();
void wright();
#ifdef ARI
void colon();
void deletecmds();
void deleteeol();
void deleteleft();
void lndelete();
void lninsert();
void lnappend();
void insertbol();
void appendeol();
void putprev();
void putnext();
void yankcmds();
void lnyank();
void yank();
void yankleft();
void tellstatus();
void lntop();
void wrong();
void pr_ShortNotice();
void pr_Notice();
int  toroku();
void pr_setup();
void setparam();
#endif

#ifdef ARI
char key[] = {
	'h', 'j', 'k', 'l',
	'b', '\06','\02', 'w',
	'0', '$', 'T', 'B',
	'i', 'x', '\014',
	':', 'D', 'd', 'O', 'o',
	'I', 'A', 'P', 'p',
	'Y', '\07', '^', 'X', 'y',
	'\0'
};
void (*func[])() = {
	left, down, up, right,
	wleft, pgdown, pgup, wright,
	lnbegin, lnend, top, bottom,
	insert, delete, redraw,
	colon, deleteeol, deletecmds, lninsert, lnappend,
	insertbol, appendeol, putprev, putnext,
	lnyank, tellstatus, lntop, deleteleft, yankcmds,
	wrong
};
#else
char key[] = "hjklHJKL[]tbixWRQ";
void (*func[])() = {
	left, down, up, right,
	wleft, pgdown, pgup, wright,
	lnbegin, lnend, top, bottom,
	insert, delete, file, redraw, quit, movegap
};
#endif

#ifdef ARI
ic_t *
#else
char *
#endif
ptr(offset)
int offset;
{
	if (offset < 0)
		return (buf);
	return (buf+offset + (buf+offset < gap ? 0 : egap-gap));
}

int
pos(pointer)
#ifdef ARI
ic_t *pointer;
#else
char *pointer;
#endif
{
	return (pointer-buf - (pointer < egap ? 0 : egap-gap));
}

void
top()
{
	index = 0;
}

void
bottom()
{
	epage = index = pos(ebuf);
}

void
quit()
{
	done = 1;
}

void
redraw()
{
	clear();
	display();
}

void
movegap()
{
#ifdef ARI
	ic_t *p = ptr(index);
#else
	char *p = ptr(index);
#endif
	while (p < gap)
		*--egap = *--gap;
	while (egap < p)
		*gap++ = *egap++;
	index = pos(egap);
}
#ifdef ARI
void
wrong()
{
	bell(); movegap();
}
#endif

int
prevline(offset)
int offset;
{
#ifdef ARI
	ic_t *p;
#else
	char *p;
#endif
	while (buf < (p = ptr(--offset)) && *p != '\n')
		;
	return (buf < p ? ++offset : 0);
}

int
nextline(offset)
int offset;
{
#ifdef ARI
	ic_t *p;
#else
	char *p;
#endif
	while ((p = ptr(offset++)) < ebuf && *p != '\n')
		;
	return (p < ebuf ? offset : pos(ebuf));
}

int
adjust(offset, column)
int offset, column;
{
#ifdef ARI
	ic_t *p;
#else
	char *p;
#endif
	int i = 0;
	while ((p = ptr(offset)) < ebuf && *p != '\n' && i < column) {
#ifdef ARI
		i += icwidth(i, *p);
#else
		i += *p == '\t' ? 8-(i&7) : 1;
#endif
		++offset;
	}
	return (offset);
}

void
left()
{
#ifdef ARI
	do {
#endif
	if (0 < index)
		--index;
#ifdef ARI
	} while (--counter > 0);
#endif
}

void
right()
{
#ifdef ARI
	do {
#endif
	if (index < pos(ebuf))
		++index;
#ifdef ARI
	} while (--counter > 0);
#endif
}

void
up()
{
#ifdef ARI
	do {
#endif
	index = adjust(prevline(prevline(index)-1), col);
#ifdef ARI
	} while (--counter > 0);
#endif
}

void
down()
{
#ifdef ARI
	do {
#endif
	index = adjust(nextline(index), col);
#ifdef ARI
	} while (--counter > 0);
#endif
}

void
lnbegin()
{
	index = prevline(index);
}

void
lnend()
{
	index = nextline(index);
	left();
}

void
wleft()
{
#ifdef ARI
	ic_t *p;
	int b, c;
	if (0 < index) --index;
	do {
		b = icclass(*(p = ptr(index)));
		while(b <= (c = icclass(*(p = ptr(index)))) && buf < p) {
			b = c;
			--index;
		}
	} while (--counter > 0);
	if (b > c && p < ebuf) ++index;
#else
	char *p;
	while (!isspace(*(p = ptr(index))) && buf < p)
		--index;
	while (isspace(*(p = ptr(index))) && buf < p)
		--index;
#endif
}

void
pgdown()
{
	page = index = prevline(epage-1);
	while (0 < row--)
		down();
	epage = pos(ebuf);
}

void
pgup()
{
#ifdef ARI
	int i = LINES-1;
#else
	int i = LINES;
#endif
	while (0 < --i) {
		page = prevline(page-1);
		up();
	}
}

void
wright()
{
#ifdef ARI
	ic_t *p;
	int b, c;
	do {
		b = icclass(*(p = ptr(index)));
		while(b >= (c = icclass(*(p = ptr(index)))) && p < ebuf) {
			b = c;
			++index;
		}
	} while (--counter > 0);
#else
	char *p;
	while (!isspace(*(p = ptr(index))) && p < ebuf)
		++index;
	while (isspace(*(p = ptr(index))) && p < ebuf)
		++index;
#endif
}

void
insert()
{
#ifdef ARI
	movegap();
	kinsert();
	modified = 1;
#else
	int ch;
	movegap();
	while ((ch = getch()) != '\f') {
		if (ch == '\b') {
			if (buf < gap)
				--gap;
		} else if (gap < egap) {
			*gap++ = ch == '\r' ? '\n' : ch;
		}
		index = pos(egap);
		display();
	}
#endif
}

void
delete()
{
#ifdef ARI
	int i = 0;
	movegap();
	do {
		if (egap < ebuf && i < REGBUF)
			regbuf[i++] = *egap++;
	} while (--counter > 0);
	regbuf[i] = 0;
	index = pos(egap);
	modified = 1;
#else
	movegap();
	if (egap < ebuf)
		index = pos(++egap);
#endif
}

#ifdef ARI
int
file(fname)
	char *fname;
#else
void
file()
#endif
{
	int i;
	int j = index;
#ifdef ARI
	int n;
#endif
	index = 0;
	movegap();
#ifdef ARI
	n = (int)(ebuf - egap);
	n = wrtofile((i=creat(fname, MODE)), egap, n) &&
		(egap[n-1]=='\n' || write(i, "\n", 1)==1);
	close(i);
	index = j;
	return n;
#else
	write(i = creat(filename, MODE), egap, (int)(ebuf-egap));
	close(i);
	index = j;
#endif
}

void
display()
{
#ifdef ARI
	ic_t *p;
#else
	char *p;
#endif
	int i, j;
	if (index < page)
		page = prevline(index);
	if (epage <= index) {
		page = nextline(index);
#ifdef ARI
		i = page==pos(ebuf)?
		 ((pos(ebuf)>0 && *ptr(pos(ebuf)-1)=='\n')? LINES-3:LINES-2)
		  : LINES-1;
#else
		i = page == pos(ebuf) ? LINES-2 : LINES;
#endif
		while (0 < i--)
			page = prevline(page-1);
	}
	move(0, 0);
	i = j = 0;
	epage = page;
	while (1) {
#ifdef ARI
		p = ptr(epage);
		if (j == COLS-1 && *p < 0) {
			addch(' ');
			move(++i, j = 0);
		}
#endif
		if (index == epage) {
#ifdef ARI
			if (!sys_line) {
#endif
			row = i;
			col = j;
#ifdef ARI
			}
#endif
		}
#ifdef ARI
		if (LINES-1 <= i || ebuf <= p)
			break;
		addch(*p);
		j += icwidth(j, *p);
#else
		p = ptr(epage);
		if (LINES <= i || ebuf <= p)
			break;
		if (*p != '\r') {
			addch(*p);
			j += *p == '\t' ? 8-(j&7) : 1;
		}
#endif
		if (*p == '\n' || COLS <= j) {
			++i;
			j = 0;
		}
		++epage;
	}
#ifdef ARI
	clrtobot();
	if (++i < LINES-1)
		mvaddstr(i, 0, "<< EOF >>");
	mvaddicstr(LINES-1, 0, sysbuf);
#else
	clrtobot();
	if (++i < LINES)
		mvaddstr(i, 0, "<< EOF >>");
#endif
	move(row, col);
	refresh();
}

int
main(argc, argv)
int argc;
char **argv;
{
	int ch, i;
#ifdef ARI
	int n;
	fbuf = (char *)&buf[BUF/2];
#endif
	egap = ebuf = buf + BUF;
	if (argc < 2)
		return (2);
#ifdef ARI
	ctrlflag = 0;
	initiocode();
	initwin();
	pr_ShortNotice();
	setup();
	initkkf();
	clear();
#else
	initscr();
	raw();
	noecho();
	idlok(stdscr, 1);
#endif
	if (0 < (i = open(filename = *++argv, 0))) {
#ifdef ARI
		n = read(i, fbuf, BUF);
		n = (*ctoic)(fbuf, n, buf, BUF);
		gap += n;
#else
		gap += read(i, buf, BUF);
#endif
		if (gap < buf)
			gap = buf;
		close(i);
	}
	top();
	while (!done) {
		display();
		i = 0;
		ch = getch();
#ifdef ARI
		if ('1' <= ch && ch <= '9') ch = count(ch);
#endif
		while (key[i] != '\0' && ch != key[i])
			++i;
		(*func[i])();
#ifdef ARI
		counter = 0;
#endif
	}
	endwin();
#ifdef ARI
	printf("\n");
	closekkf();
#endif
	return (0);
}
#ifdef ARI

/*
 *  the following is addtional functions.
 */

static int
isspace(c) /* c might be less than zero, never use macro defined in ctype.h */
	int c;
{
	return (c == ' ' || c == '\t' || c == '\n');
}

static int
icclass(c)
	int c;
{
	if (isspace(c)) return 0;
	/* ASCII */
	if ((ic_t)0x0000<= c && c<=(ic_t)0x007f) return 6;
	/* kuten, touten */
	if ((ic_t)0xa1a1<= c && c<=(ic_t)0xa1a8) return 1;
	/* hiragana */
	if ((ic_t)0xa4a1<= c && c<=(ic_t)0xa4f3) return 3;
	/* katakana */
	if (((ic_t)0xa5a1<=c && c<=(ic_t)0xa5f4) || c==(ic_t)0xa1bc) return 4;
	/* JIS kigo, Alphabet */
	if ((ic_t)0xa1a9<= c && c<=(ic_t)0xa9fe) return 5;
	/* hankaku kana */
	if ((ic_t)0x00a1<=c && c<=(ic_t)0x00df) return 2;
	/* JIS kanji */
	return 7;
}

void
colon()
{
	static char msg_q[] = "modified -- use q! to abort or wq to save";
	static char msg_w[] = "can't write to '%s'";
	int sav_row, sav_col;
	int i, n, pos, fd;
	ic_t *p;
	char *s, *f;

	sys_line = 1;
	p = sysbuf;
	sav_row = row;
	sav_col = col;
	row = LINES - 1;
	pos = col = 1;
	p[0] = (ic_t)':';
	p[1] = 0;
	display();
	while ((n = getch()) != '\r') {
		if (n == '\b') {
			p[--pos] = 0;
			if (pos < 1) break;
		}
		else if (n == '\033') { /* ESC -- escape colon */
			pos = 0;
			break;
		}
		else {
			if (col < COLS && pos < SYSBUF-1) {
				p[pos++] = n;
				p[pos] = 0;
			}
			else
				bell();
		}
		for ( col = i = 0 ; i < pos ; i++)
			col += icwidth(col, p[i]);
		display();
	}
	if (pos > 0) {
		p++;
		if (cmd(p, "w") || (p[0]=='w' && p[1]==' ')) {
			if (p[1] == ' ') {
				/* have filename */
				f = (char *)p;
				p += 2;
				for (i = 0; f[i] = p[i]; i++) ;
			}
			else
				f = filename;
			if (file(f)) {
				modified = 0;
				sysbuf[0] = 0;
			}
			else {
				sprintf((char *)sysbuf, msg_w, f);
				stoics((char *)sysbuf, sysbuf, SYSBUF-1);
			}
		}
		else if (cmd(p, "wq") || cmd(p, "x")) {
			if (file(filename)) {
				quit();
				sysbuf[0] = 0;
			}
			else {
				sprintf((char *)sysbuf, msg_w, filename);
				stoics((char *)sysbuf, sysbuf, SYSBUF-1);
			}
		}
		else if (cmd(p, "q")) {
			if (! modified) {
				quit();
				sysbuf[0] = 0;
			}
			else
				stoics(msg_q, sysbuf, SYSBUF-1);
		}
		else if (cmd(p, "q!")) {
			quit();
			sysbuf[0] = 0;
		}
		else if (cmd(p, "$")) {
			bottom();
			sysbuf[0] = 0;
		}
		else if ('1' <= *p && *p <= '9') {
			f = (char *)p;
			for (i = 0; f[i] = p[i]; i++) ;
			i = atoi(f);
			top();
			while (--i)
				index = nextline(index);
			index = adjust(index, sav_col);
			sysbuf[0] = 0;
		}
		else if (*p == 'r' && *(p+1) == ' ') {
			f = (char *)p;
			p += 2;
			for (i = 0; f[i] = p[i]; i++) ;
			index = nextline(i = index);
			movegap();
			n = egap - gap;
			s = (char *)&gap[n/2];
			if ((fd = open(f, 0)) > 0) {
				n = read(fd, s, n);
				n = (*ctoic)(s, n, gap, egap - gap);
				gap += n;
				modified = 1;
				close(fd);
			}
			index = i;
			movegap();
			sysbuf[0] = 0;
		}
		else if (cmd(p, "version")) {
			stoics(version, sysbuf, SYSBUF-1);
		}
		else if (cmd(p, "toroku") || cmd(p, "touroku")) {
			if (toroku())
				sysbuf[0] = 0;
		}
		else if (cmd(p,"setup") || cmd(p,"set") || cmd(p,"set all")) {
			pr_setup();
			sysbuf[0] = 0;
		}
		else if (cmd(p, "notice")) {
			pr_Notice();
			sysbuf[0] = 0;
		}
		else if (p[0]=='s' && p[1]=='e' && p[2]=='t' && p[3]==' ') {
			/* set command */
			setparam(p + 4);
			redraw();
			sysbuf[0] = 0;
		}
		else
			sysbuf[0] = 0;
	}
	else
		sysbuf[0] = 0;
	sys_line = 0;
	row = sav_row;
	col = sav_col;
}

static int
cmd(is_s, s)
	ic_t *is_s;
	char *s;
{
	while (*is_s == *s++) {
		if (*is_s++ == 0)
			return 1;
	}
	return 0;
}

void
deletecmds()
{
	switch (getch()) {
	case 'd':
		lndelete();
		break;
	case 'l':
		delete();
		break;
	case 'h':
		deleteleft();
		break;
	default:
		bell();
		break;
	}
}

void
lndelete()
{
	int i;
	index = prevline(index);
	movegap();
	i = 0;
	while(egap < ebuf && i < REGBUF) {
		regbuf[i++] = *egap;
		if (*egap++ == '\n' && --counter <= 0) break;
	}
	regbuf[i] = 0;
	index = pos(egap);
	modified = 1;
}

void
deleteleft()
{
	ic_t *p;
	int i = 0;
	movegap();
	p = gap;
	do {
		if (buf < p)
			--p;
	} while (--counter > 0);
	for(i = 0; &p[i] < gap && i < REGBUF; i++)
		regbuf[i] = p[i];
	regbuf[i] = 0;
	gap = p;
	index = pos(egap);
	modified = 1;
}

void
deleteeol()
{
	int i = 0;
	movegap();
	while(egap < ebuf && i < REGBUF && *egap != '\n')
		regbuf[i++] = *egap++;
	regbuf[i] = 0;
	index = pos(egap);
	modified = 1;
}

void
lninsert()
{
	index = prevline(index);
	movegap();
	if (gap < egap)
		*gap++ = '\n';
	movegap();
	kinsert();
	modified = 1;
}

void
lnappend()
{
	index = nextline(index);
	movegap();
	if (gap < egap)
		if (index == pos(ebuf) && *(gap-1) != '\n')
			*gap++ = '\n';
		else
			*(--egap) = '\n';
	index = pos(egap);
	movegap();
	kinsert();
	modified = 1;
}

void
insertbol()
{
	index = prevline(index);
	movegap();
	kinsert();
	modified = 1;
}

void
appendeol()
{
	index = nextline(index);
	if (index > 1 && *ptr(index - 1) == '\n') --index;
	movegap();
	kinsert();
	modified = 1;
}

static int
count(c)
	int c;
{
	do {
		counter = counter * 10 + c - '0';
	} while((c = getch()) >= '0' && c <= '9');
	return c;
}

void
putprev()
{
	ic_t *p = regbuf;

	movegap();
	while ( *p && gap < egap)
		*gap++ = *p++;
	movegap();
	modified = 1;
}

void
putnext()
{
	ic_t *p = regbuf;

	right();
	movegap();
	while ( *p && gap < egap)
		*gap++ = *p++;
	movegap();
	modified = 1;
}

void
yankcmds()
{
	switch (getch()) {
	case 'y':
		lnyank();
		break;
	case 'l':
		yank();
		break;
	case 'h':
		yankleft();
		break;
	default:
		bell();
		break;
	}
}

void
lnyank()
{
	ic_t *p;
	int i = 0;
	index = prevline(index);
	movegap();
	p = egap;
	while(p < ebuf && i < REGBUF) {
		regbuf[i++] = *p;
		if (*p++ == '\n' && --counter <= 0) break;
	}
	regbuf[i] = 0;
}

void
yank()
{
	ic_t *p;
	int i = 0;
	movegap();
	p = egap;
	do {
		if (p < ebuf && i < REGBUF)
			regbuf[i++] = *p++;
	} while (--counter > 0);
	regbuf[i] = 0;
}

void
yankleft()
{
	ic_t *p;
	int i = 0;
	movegap();
	p = gap;
	do {
		if (buf < p)
			--p;
	} while (--counter > 0);
	for(i = 0; &p[i] < gap && i < REGBUF; i++)
		regbuf[i] = p[i];
	regbuf[i] = 0;
	index = pos(p);
}

void
tellstatus()
{
	int i, n;

	for (n = 1, i = 0; i < index ; i++) {
		if (*ptr(i) == '\n')
			n++;
	}
	sprintf((char *)sysbuf, "<[%c%c%c] \"%s\" %s line %d >",
		codedef[0], codedef[1], codedef[2],
		filename, modified? "[MODIFIED]": "", n);
	stoics((char *)sysbuf, sysbuf, SYSBUF-1);
}

void
lntop()
{
	lnbegin();
	if (isspace(*ptr(index)))
		wright();
}

void
pr_ShortNotice()
{
	int i;
	short_notice[0] = copyright;
	for (i = 0; short_notice[i] ; i++) {
		mvaddstr(i, 0, short_notice[i]);
	}
	mvaddstr(++i, 0, "Wait a moment, please...");
	refresh();
}

void
pr_Notice()
{
	int i;
	full_notice[1] = copyright;
	clear();
	for (i = 0; full_notice[i] ; i++) {
		mvaddstr(i, 0, full_notice[i]);
	}
	refresh();
	(void)getch(); /* wait to get any key. */
	redraw();
}

void
pr_setup()
{
	int i;
	extern char
	    *scrctl_conf; /* screen control -- defined in mnxio.c or others */
	char **pp;

	clear();
	mvaddstr(0, 0, version);
	i = 2;
	pp = kk_conf();
	while (*pp)
		mvaddstr(i++, 0, *pp++);
	sprintf((char *)sysbuf, "screen control: %s", scrctl_conf);
	mvaddstr(i++, 0, (char *)sysbuf);
	i++;
	sprintf((char *)sysbuf, "codedef=%s", codedef);
	mvaddstr(i++, 0, (char *)sysbuf);
	sprintf((char *)sysbuf, "tab=%d", tabsz);
	mvaddstr(i++, 0, (char *)sysbuf);
	mvaddstr(i++, 0, (ctrlflag&F_TABEXPAND)?
	    "tabexpand":"notabexpand");

	mvaddstr(i++, 0, (ctrlflag&F_BRKFENCE)?
	    "breakfence":"nobreakfence");
	mvaddstr(i++, 0, (ctrlflag&F_ASCII_INDICATE)?
	    "asciiindicate":"noasciiindicate");

	mvaddstr(i+2, 0, "<Hit any key to return>");
	sysbuf[0] = 0;
	refresh();
	(void)getch(); /* wait to get any key. */
	redraw();
}

static void
setup()
{
	char *p;
	ic_t *s;
	int i;

	/* setup parameters from environment value */
	if ((p = getenv("ARI_SETUP")) != NULL) {
		while (*p) {
			s = sysbuf;
			for (i = 0;
			 i<SYSBUF-1 && *p && *p != '|' && *p != ','; i++, p++)
				s[i] = *p;
			s[i] = 0;
			while (--i>=0 && (s[i]==' ' || s[i]=='\t')) s[i] = 0;
			while (*s==' ' || *s=='\t') s++;
			if (*s)
				setparam(s);
			if (*p == '|' || *p == ',') p++;
		}
		sysbuf[0] = 0;
	}

	/* setup codedef */
	if ((p = getenv("ARI_CODEDEF")) != NULL) setiocode(p);
}

static ic_t *
paramvalue(p, s)
	ic_t *p;
	char *s;
{
	int c;

	while ((c = *p++) && c == *s++) {
		if (c == '=')
			return p;
	}
	return (ic_t *)0;
}

void
setparam(p)
	ic_t *p;
{
	ic_t *q;
	char *s;
	int i;

	if (cmd(p, "nobreakfence"))         ctrlflag &= ~F_BRKFENCE;
	else if (cmd(p, "breakfence"))      ctrlflag |= F_BRKFENCE;
	else if (cmd(p, "noasciiindicate")) ctrlflag &= ~F_ASCII_INDICATE;
	else if (cmd(p, "asciiindicate"))   ctrlflag |= F_ASCII_INDICATE;
	else if (cmd(p, "notabexpand"))     ctrlflag &= ~F_TABEXPAND;
	else if (cmd(p, "tabexpand"))       ctrlflag |= F_TABEXPAND;
	else if ((q = paramvalue(p, "tab=")) != (ic_t *)0) {
		for (s = (char *)q, i = 0; s[i] = q[i]; i++) ;
		if ((i = atoi(s)) > 0)
			tabsz = i;
	}
	else if ((q = paramvalue(p, "codedef=")) != (ic_t *)0) {
		for (s = (char *)q, i = 0; s[i] = q[i]; i++) ;
		setiocode(s);
	}
}
#endif
