PROG = ttyd

NOMAN =

SLOG = ../../lib/slog

CPPFLAGS = -DSUPPORT_UTMP
COPTS = -I$(SLOG)

LDADD += -lslog -lrt -lpthread -lutil
LDFLAGS += -L$(SLOG)

.include <bsd.prog.mk>
