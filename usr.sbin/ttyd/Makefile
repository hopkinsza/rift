# Makefile

PROG = ttyd

SLOG = ../slog

CFLAGS = -Wall -I$(SLOG)
CPPFLAGS = -DSUPPORT_UTMP
LDLIBS = -lslog -lrt -lpthread -lutil
LDFLAGS = -static -L$(SLOG)


.PHONY: all clean

$(PROG): $(PROG).c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $(PROG).c $(LDLIBS)

all: $(PROG)

clean:
	@rm -f $(PROG) *.o
	@echo cleaned
