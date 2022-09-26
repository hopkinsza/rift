#
# SPDX-License-Identifier: 0BSD
#

REBOOT_STYLE = linux

PROG = hpr
SRCS = hpr.c reboot.$(REBOOT_STYLE).c

CFLAGS = -Wall -static


.PHONY: clean

$(PROG): $(SRCS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(SRCS) -o $@

all: $(PROG)

clean:
	@rm -f $(PROG)
	@echo cleaned
