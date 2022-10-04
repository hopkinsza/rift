TARGET_OS ?= $(.MAKE.OS)

.if $(TARGET_OS) != Linux && $(TARGET_OS) != NetBSD
.error unsupported platform
.endif

PROG = hpr
SRCS = hpr.c reboot.$(TARGET_OS).c
INCS = osind_reboot.h

MAN = hpr.8

.include <bsd.prog.mk>
