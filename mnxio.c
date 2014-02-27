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
 * mnxio.c -- OS or machine depended tty control functions
 *            for minix and her sisters
 *
 * version 1.00,  04.12.1992
 */
 
#include "config.h"
#ifdef TERMIO
# include <termio.h>
  static struct termio sav, cur;
  char *scrctl_conf = "termio";
#else
# include <sgtty.h>
  static struct sgttyb sav, cur;
  char *scrctl_conf = "sgtty";
#endif

void init_tty(),  /* save current tty mode and set raw mode */
     end_tty(),   /* restore saved tty mode */
     out_tty();   /* out charactors to tty */
int  getch();     /* return a charactor from tty */

void
init_tty()
{
#ifdef TERMIO
	if (ioctl(0, TCGETA, &sav) < 0) abort();
	cur = sav;
	cur.c_cc[VMIN] = 1;
	cur.c_cc[VTIME] = 0;
	cur.c_iflag |= IGNBRK;
	cur.c_iflag &= ~( ICRNL | INLCR | ISTRIP | IXON | IXOFF );
	cur.c_oflag &= ~OPOST;
	cur.c_cflag |= CS8;
	cur.c_cflag &= ~PARENB;
	cur.c_lflag &= ~( ICANON | ISIG | ECHO);
	if (ioctl(0, TCSETAF, &cur) < 0) abort();
#else
	ioctl(0, TIOCGETP, &sav);
	cur = sav;
	cur.sg_flags |= CBREAK;
	/* cur.sg_flags &= ~(CRMOD|ECHO|XTABS); */
	cur.sg_flags &= ~(CRMOD|ECHO);
	cur.sg_flags |= XTABS | RAW;
	ioctl(2, TIOCSETP, &cur);
#endif
}

void
end_tty()
{
#ifdef TERMIO
	if (ioctl(0, TCSETAF, &sav) < 0) abort();
#else
	ioctl(2, TIOCSETP, &sav);
#endif
}

void
out_tty(s, n)
	char *s;
	int n;
{
	write(1, s, n);
}

int
getch()
{
	char c;

	if (read(0, &c, 1) < 1) return (-1);
	return (c & 0xff);
}
