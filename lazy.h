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
 * lazy.h -- common include file for ari editor
 *
 * version 1.00,  04.12.1992
 */

typedef short ic_t;	/* internal code type */
#define index index_pos

#ifdef _LAZY_C_
# define EXTERN
#else
# define EXTERN extern
#endif

EXTERN int LINES, COLS;
EXTERN int (*ctoic)(), (*ictoc)(), (*ictoscr)();

#define SYSBUF 160
EXTERN ic_t sysbuf[SYSBUF];

#define F_BRKFENCE		0x1
#define F_TABEXPAND		0x2
#define F_ASCII_INDICATE	0x4
EXTERN int ctrlflag, tabsz;

void initscr(), initiocode(), setiocode(),
     raw(), endwin(), bell(), move(), clear(),
     clrtobot(), mvaddstr(), mvaddicstr(), addch(), refresh();
int getch(), icwidth(), wrtofile();
void stoics();
