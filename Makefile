PROG=	mgopherd

SRCS+=	${PROG}.c
SRCS+=	options.c

WARNS?=	6

NO_MAN=	YES

.include <bsd.prog.mk>
