PROG = fsv
SRCS = fsv.c
INCS = extern.h

SLOG = ../../lib/slog

COPTS = -I$(SLOG)

LDADD += -lslog -lrt -lpthread
LDFLAGS = -L$(SLOG)

.include <bsd.prog.mk>
