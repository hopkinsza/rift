TARGET_OS ?= $(.MAKE.OS)

.if $(TARGET_OS) == Linux
HPR_REBOOT_STYLE = linux
.elif $(TARGET_OS) == NetBSD
HPR_REBOOT_STYLE = netbsd
.else
.error unsupported platform
.endif

PROG = hpr
SRCS = hpr.c reboot.$(HPR_REBOOT_STYLE).c
INCS = osind_reboot.h

NOMAN =

.include <bsd.prog.mk>
