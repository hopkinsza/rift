# Makefile

PROG = fsv
SRCS = fsv.c
OBJS = $(SRCS:.c=.o)

SLOG = ../slog

CFLAGS = -Wall -I$(SLOG)
LDLIBS = -lslog -lrt -lpthread
LDFLAGS = -static -L$(SLOG)


.PHONY: all clean

$(PROG): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

all: $(PROG)

$(OBJS): extern.h

clean:
	@rm -f $(PROG) *.o
	@echo cleaned

# a.out: fsv.c
# 	cc fsv.c `pkg-config --cflags --libs libbsd-overlay`
