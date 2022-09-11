# Makefile

PROG = fsv
SRCS = fsv.c slog.c
OBJS = $(SRCS:.c=.o)

CFLAGS = -Wall -static
LDLIBS = -lrt -lpthread


.PHONY: all clean

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $@ $(LDLIBS)

all: $(PROG)

$(OBJS): extern.h

clean:
	@rm -f $(PROG) *.o
	@echo cleaned

# a.out: fsv.c
# 	cc fsv.c `pkg-config --cflags --libs libbsd-overlay`
