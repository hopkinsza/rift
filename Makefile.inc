#
# This build uses the Makefile.inc facility to set BINDIR and LIBDIR properly;
# these variables are used by the `install' target.
#
# For a user to install to a specific prefix, they can specify a non-empty
# PREFIX variable on the make command line.
# By default, it is /usr/local.
#
# Alternatively, installation to "the system" can be requested by specifying
# an empty prefix (PREFIX=).
# Subdirectories' `Makefile.inc's should tentatively set BINDIR/LIBDIR to
# install to the appropriate "system" directory (or, they can leave them unset
# to use the default of the mk-files implementation).
# It is not so simple as setting `PREFIX=/' because some things want to install
# to /bin, some to /usr/bin.
#

PREFIX ?= /usr/local

.if "$(PREFIX)" != ""

.if "$(BINDIR:M*sbin*)" != ""
BINDIR = $(PREFIX)/sbin
.else
BINDIR = $(PREFIX)/bin
.endif

LIBDIR = $(PREFIX)/lib

INCSDIR = $(PREFIX)/include
MANDIR = $(PREFIX)/share/man
.endif
