# build lib first so things can use slog

SUBDIR = lib .WAIT
SUBDIR += sbin \
	usr.bin \
	usr.sbin

.if $(.MAKE.OS) != "NetBSD"
.include "rift.subdir.mk"
.else
.include <bsd.subdir.mk>
.endif
