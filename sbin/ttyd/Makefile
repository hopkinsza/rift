TARGET_OS ?= $(.MAKE.OS)

PROG = ttyd
SRCS = ttyd.c do_logout.$(TARGET_OS).c
INCS = extern.h
NOMAN =

SLOG = ../../lib/slog

CPPFLAGS = -DUSE_UTMP -I$(SLOG)

LDADD += -lslog -lrt -lpthread -lutil
LDFLAGS += -L$(SLOG)

.include <bsd.prog.mk>
