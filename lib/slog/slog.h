#ifndef _SLOG_H_
#define _SLOG_H_

/*
 * DO NOT USE LOG_ODELAY with slog! It is aliased to LOG_NLOG if your libc
 * doesn't already have that defined.
 *
 * Since LOG_ODELAY is already the default, it is typically never used.
 * Utilizing this already-defined and standardized flag value means we never
 * have to worry about a collision, which would be possible if a random high
 * value was chosen.
 */
#ifndef LOG_NLOG
#define LOG_NLOG LOG_ODELAY
#endif

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

void slog(int, const char *, ...);

void slog_open(const char *, int, int);
void slog_close();

int slog_upto(int);

#endif // !_SLOG_H_
