# $OpenBSD$ 

NOMAN=Yes
MAGIC=          /etc/magic
MAGICOWN=       root
MAGICGRP=       bin
MAGICMODE=      444

PROG=           file
SRCS=           file.c
CFLAGS+=        -DMAGIC='"$(MAGIC)"' -g
CFLAGS+=        -Wall -Wstrict-prototypes -Wmissing-prototypes
CFLAGS+=        -Wmissing-declarations
CFLAGS+=        -Wshadow -Wpointer-arith -Wcast-qual
CFLAGS+=        -Wsign-compare
#MAN=            file.1 magic.5

LDADD+=         -lutil
DPADD+=         ${LIBUTIL}


.include <bsd.prog.mk>

