# Makefile

PROG = ttyd

CFLAGS = -Wall
CPPFLAGS = -DSUPPORT_UTMP
LDLIBS = -lrt -lutil


.PHONY: all clean

$(PROG): $(PROG).c slog/slog.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $(PROG).c slog/slog.c $(LDLIBS)

all: $(PROG)

clean:
	@rm -f $(PROG) *.o
	@echo cleaned
