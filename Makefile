PROG=	mgopherd

SRCS+=	${PROG}.c
SRCS+=	options.c
SRCS+=	tools.c
SRCS+=	send.c
LDADD+=	-lmagic

WARNS?=	6
CSTD=	c99

NO_MAN=	YES

.include <bsd.prog.mk>
