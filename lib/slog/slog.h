#ifndef _SLOG_H_
#define _SLOG_H_

#include <syslog.h>

/*
 * Assume that LOG_* level constants are defined as they originally were
 * in 4.2BSD, to be able to compare them.
 * This is true for modern BSDs, glibc, and musl.
 */
#if (\
  LOG_EMERG   != 0 ||\
  LOG_ALERT   != 1 ||\
  LOG_CRIT    != 2 ||\
  LOG_ERR     != 3 ||\
  LOG_WARNING != 4 ||\
  LOG_NOTICE  != 5 ||\
  LOG_INFO    != 6 ||\
  LOG_DEBUG   != 7)
#error slog.c basic assumption was wrong,\
your libc does not define syslog.h LOG_* levels as expected
#endif

#ifndef SLOG_FACILITY
#define SLOG_FACILITY LOG_DAEMON
#endif

void slog(int, const char *, ...);

void slog_open();
void slog_close();

int slog_upto(int);
int slog_do_stderr(int);
int slog_do_syslog(int);

#endif // _SLOG_H_
