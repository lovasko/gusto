#  Copyright (c) 2019 Daniel Lovasko
#  All Rights Reserved
#
#  Distributed under the terms of the 2-clause BSD License. The full
#  license is in the file LICENSE, distributed as part of this software.

.POSIX:

# Compilation settings.
C99 = c99
STD = -std=c99
OPT = -Os
LNK = -static
FTM = -D_BSD_SOURCE -D_XOPEN_SOURCE -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE
CHK = -Wall -Wextra -Wconversion -Werror -fstrict-aliasing
INC = -Isrc/

CFLAGS = $(STD) $(OPT) $(FTM) $(CHK) $(INC) $(LNK)

.DEFAULT: bin/gusto

bin/gusto: src/gusto.c
	set -e;                                        \
	CKS=$$(find src/ -type f | xargs cat | cksum); \
	CRC=$$(echo $${CKS} | cut -d' ' -f1);          \
	LEN=$$(echo $${CKS} | cut -d' ' -f2);          \
	USR=$$(id -u -n);                              \
	HWT=$$(uname -m);                              \
	OSN=$$(uname -s);                              \
	$(C99) $(CFLAGS)                               \
	-DGUSTO_CRC="$${CRC}"                          \
	-DGUSTO_LEN="$${LEN}"                          \
	-DGUSTO_USR="$${USR}"                          \
	-DGUSTO_HWT="$${HWT}"                          \
	-DGUSTO_OSN="$${OSN}"                          \
	-DGUSTO_C99="$(C99)"                           \
	-DGUSTO_STD="$(STD)"                           \
	-DGUSTO_LNK="$(LNK)"                           \
	-DGUSTO_OPT="$(OPT)"                           \
	-DGUSTO_FTM="$(FTM)"                           \
	-DGUSTO_CHK="$(CHK)"                           \
	-o bin/gusto src/gusto.c

clean:
	rm -f bin/gusto
