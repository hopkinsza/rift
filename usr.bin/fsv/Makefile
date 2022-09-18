# Makefile

PROG = fsv
SRCS = fsv.c slog.c
OBJS = $(SRCS:.c=.o)

CFLAGS = -Wall
LDLIBS = -lrt -lpthread
LDFLAGS = -static


.PHONY: all clean

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

all: $(PROG)

$(OBJS): extern.h slog.h

clean:
	@rm -f $(PROG) *.o
	@echo cleaned

# a.out: fsv.c
# 	cc fsv.c `pkg-config --cflags --libs libbsd-overlay`
