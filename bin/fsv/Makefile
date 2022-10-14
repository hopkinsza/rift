PROG = fsv
SRCS = fsv.c status.c
INCS = extern.h

SLOG = ../../lib/slog

COPTS = -I$(SLOG)

# glibc wants _GNU_SOURCE for asprintf(3)
CPPFLAGS.status = -D_GNU_SOURCE

LDADD += -lslog -lrt -lpthread
LDFLAGS = -L$(SLOG)

.include <bsd.prog.mk>
