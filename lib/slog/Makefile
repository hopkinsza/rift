LIBNAME = libslog.a
SRCS = slog.c
OBJS = $(SRCS:.c=.o)

CFLAGS = -Wall
ARFLAGS = rvs


.PHONY: all clean

$(LIBNAME): $(OBJS)
	$(AR) $(ARFLAGS) $@ $(OBJS)

all: $(LIBNAME)

$(OBJS): slog.h

clean:
	@rm -f $(LIBNAME) *.o
	@echo cleaned
