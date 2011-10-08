# $OpenBSD$ 

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

#CLEANFILES+=    magic post-magic
all:            file

#MAG1=           $(.CURDIR)/magdir/Header\
#                $(.CURDIR)/magdir/Localstuff\
#                $(.CURDIR)/magdir/OpenBSD
#MAGFILES=       $(.CURDIR)/magdir/[0-9a-z]*

#post-magic:     $(MAGFILES)
#        for i in ${.ALLSRC:N*.orig}; \
#        do \
#                echo $$i; \
#        done|sort|xargs -n 1024 cat > $(.TARGET)
#
#magic:          $(MAG1) post-magic
#        cat ${MAG1} post-magic > $(.TARGET)


#afterinstall:
#        ${INSTALL} ${INSTALL_COPY} -o $(MAGICOWN) -g $(MAGICGRP) -m $(MAGICMODE) magic \
#                $(DESTDIR)$(MAGIC)

.include <bsd.prog.mk>

