# build lib first so things can use slog

SUBDIR = lib .WAIT
SUBDIR += bin \
	  sbin

.include <rf/subdir.mk>
