#
# This build uses the Makefile.inc facility to set *DIR variables properly.
# They are used by the `install' target.
#
# For a user to install to a specific prefix, they can specify a non-empty
# PREFIX variable on the make command line -- default /usr/local.
#
# PREFIX=/ is handled specially; variable INST_USR can be set to `yes'
# (typically by a subdirectory's Makefile.inc) to install to /usr.
#
# Note that make variables are (typically) expanded at time of usage,
# so subdirectories are free to use $(PREFIX).
#

PREFIX ?= /usr/local

.if "$(PREFIX)" == "/" && "$(INST_USR)" == "yes"
PREFIX = /usr
.endif

BINDIR  ?= $(PREFIX)/bin
LIBDIR  ?= $(PREFIX)/lib
INCSDIR ?= $(PREFIX)/include
MANDIR  ?= $(PREFIX)/share/man

#
# Support for __dead function attribute.
# This currently uses the GCC attribute syntax, but could use C11 _Noreturn,
# or C23 attribute [[noreturn]] in the future.
# Rift code will probably be updated to use [[noreturn]] once that is commonly
# available.
#

CPPFLAGS += "-D__dead=__attribute__((noreturn))"
