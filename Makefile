# build lib first so things can use slog

SUBDIR = lib .WAIT
SUBDIR += bin \
	  sbin

.if $(.MAKE.OS) != "NetBSD"
.include "rift.subdir.mk"
.else
.include <bsd.subdir.mk>
.endif
