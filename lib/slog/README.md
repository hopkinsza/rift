slog
====

`syslog(3)` lookalike that also allows printing to stderr only.

why
---

`syslog(3)` is a nice interface for logging.
It has well-defined log levels,
and you can very easily select which levels you want output from.

Pretty much everything has the `LOG_PERROR` flag to be able to print
to `stderr` as well as to `/dev/log`,
but currently only NetBSD `syslog(3)` has the `LOG_NLOG` flag to allow
printing to `stderr` only.
Hopefully this will become universal and this wrapper will no longer
be necessary.
