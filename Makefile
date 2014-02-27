#
# Makefile for ari, Japanese fat ae with mock SKK
#	Copyright (C) 1991 Yuji Nimura
#	04.12.1992
#
# Usage
#
# make all     -- make ari and skksrch               : negate MERGE
# make ari     -- make only ari                      : negate MERGE
# make skksrch -- make only skksrch                  : negate MERGE
# make m_ari   -- make merged ari (stand alone type) : assert MERGE
# make clean   -- clean up internal object file
#
# version
VER = 1_00
#
# compile options
#
#----------------------------------------------------------------------------#
# machine dependency
#
# assert when no <stdlib.h>
#NO_STDLIB = -DNO_STDLIB_H
#
# assert when use <strings.h> instead of <string.h> on BSD
#NO_STRING = -DNO_STRING_H
#
# assert when use termio instead of sgtty with ioctl on sysV
#TERMIO = -DTERMIO
#
# assert when get merged ari. Never assert to get separated ari and skksrch.
#MERGE = -DMERGE
#
# assert when rename() is unavailable.
#  NOTICE: rename() on MINIX 1.5 for IBM/PC, etc seems wrong to use.
NO_RENAME = -DNO_RENAME
#
#----------------------------------------------------------------------------#
# select
#
# MINIX for IBM-PC, NEC PC-98 with ACK C.
C2OBJ = -S
OBJ2E = -o $@
COPT = -F
CC = cc
LDFLAGS = -i
O = s
TTYIO = mnxio
DEPENDENCY = -DSMALL_REG
#------------------
#
# Other systems; MacMINIX, MINIX-ST, BSD, sysV, etc.
#C2OBJ = -c
#OBJ2E = -o $@
#COPT = -O
#CC = cc
#O = o
#TTYIO = mnxio
#DEPENDENCY = 
#----------------------------------------------------------------------------#


CFLAGS = $(COPT) -I. -DARI $(TERMIO) $(NO_STDLIB) $(NO_STRING) \
 $(MERGE) $(NO_RENAME) $(DEPENDENCY)

CMN_OBJ = ae.$O lazy.$O kanakan.$O
TTYIO_OBJ = $(TTYIO).$O
SKK_OBJ = skksrch.$O
ARI_OBJ = $(CMN_OBJ) $(TTYIO_OBJ)
MERGED_OBJ = $(ARI_OBJ) $(SKK_OBJ)

all: ari$E skksrch$E

ari$E: $(ARI_OBJ)
	$(CC) $(LDFLAGS) $(OBJ2E) $(ARI_OBJ)

m_ari$E: $(MERGED_OBJ)
	$(CC) $(LDFLAGS) $(OBJ2E) $(MERGED_OBJ)

$(CMN_OBJ): lazy.h config.h
$(SKK_OBJ): config.h
$(TTYIO_OBJ): config.h

.c.$O:
	$(CC) $(C2OBJ) $(CFLAGS) $<

skksrch$E: skksrch.c config.h
	$(CC) $(COPT) -I. -DARI $(NO_STDLIB) $(NO_STRING) \
	$(NO_RENAME) $(DEPENDENCY) $(OBJ2E) skksrch.c

clean:
	rm -f $(ARI_OBJ) $(SKK_OBJ)
#	
#- End of Makefile ----------------------------------------------------------#
