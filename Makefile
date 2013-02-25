PROG=	mgopherd

SRCS+=	${PROG}.c
SRCS+=	options.c
SRCS+=	tools.c
LDADD+=	-lmagic

WARNS?=	6

NO_MAN=	YES

.include <bsd.prog.mk>
