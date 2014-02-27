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
 * config.h -- configration file for ari editor
 *   NOTICE: machine dependet configration is defined in makefile
 *
 * version 1.00,  04.12.1992
 */

/*
 *----------------------------------------------------------------------------*
 * value
 */
#define MODE	0666	/* mode at creating file to write                     */

#define BUF	16383	/*  maximum text size;                                *
			 *  BUF < (maximum postive value of int)/2            */

/*
 *----------------------------------------------------------------------------*
 * flag
 */
#define	H_KANA		/* assert -- support roman-kana and hankaku-kana      *
			 * negate -- only roman-kana                          */

#define	UPDATE_DIC	/* assert when enable to enter new entries            *
			 * and update local jisyo                             */
/*
 * NOTICE:
 * When you make merged ari on the systems such as MINIX for IBM/PC, on which
 * data + stack < 64KB, you should define SMALL_POOL and reduce FRAME_POOL
 * size to save internal work area.
 * However you need not change the following if SMALL_REG is defined in
 * makefile; when SMALL_REG, UPDATE_DIC and MERGE are defined, it is
 * automatically done to define SMALL_POOL and reduce FRAME_POOL size.
 */
#undef	SMALL_POOL	/* assert to save work area when not enough memory;   *
			 * it is not available to resort word order in each   *
			 * entries of local jisyo when SMALL_POOL is asserted.*/

#define	FRAME_POOL 16000 /* size of internal work area                        */

/*
 *----------------------------------------------------------------------------*
 * the following is reguration to define SMALL_POOL and to adjust
 * FRAME_POOL size -- don't change
 *
 */
#ifdef UPDATE_DIC
# ifdef SMALL_REG
#  ifdef MERGE
#   ifdef FRAME_POOL
#    if FRAME_POOL > 256
#     undef FRAME_POOL
#     define FRAME_POOL 256
#    endif
#   endif
#  endif
# endif
# ifndef FRAME_POOL
#  define FRAME_POOL 64
# endif
# if FRAME_POOL < 1024
#  ifndef SMALL_POOL
#   define SMALL_POOL
#  endif
# endif
#endif
/*
 *- end of config.h ----------------------------------------------------------*
 */
