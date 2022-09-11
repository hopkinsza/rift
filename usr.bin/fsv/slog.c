#include <err.h>
#include <stdarg.h>
#include <stdio.h>
#include <syslog.h>

#include "extern.h"

// default syslog facility
static int facil = LOG_DAEMON;

static int upto = LOG_DEBUG;

static int do_stderr = 1;
static int do_syslog = 0;

/*
 * syslog(3) lookalike that also allows printing to stderr only.
 * This is the default.
 * Currently, only netbsd syslog(3) can do this with its LOG_NLOG flag,
 * otherwise we would use it directly.
 *
 * Argument 'level' should be a level only, not OR'd with facility.
 *
 * Assume that the LOG_* level constants are defined as they originally were
 * in 4.2BSD, to be able to compare them.
 * This is true for modern BSDs, glibc, and musl.
 *
 * Additionally, vsyslog(3) is not in POSIX.
 */
void
slog(int level, const char *fmt, ...)
{
	va_list ap;

	if (do_stderr && level <= upto) {
		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		fprintf(stderr, "\n");
		va_end(ap);
	}

	if (do_syslog) {
		va_start(ap, fmt);
		vsyslog(level, fmt, ap);
		va_end(ap);
	}
}

/*
 * Call me at the beginning of your program to verify the assumption,
 * and go ahead and open the syslog fd.
 */
void
slog_init()
{
	int e = 0;

	if (LOG_EMERG != 0)
		e = 1;
	else if (LOG_ALERT != 1)
		e = 1;
	else if (LOG_CRIT != 2)
		e = 1;
	else if (LOG_ERR != 3)
		e = 1;
	else if (LOG_WARNING != 4)
		e = 1;
	else if (LOG_NOTICE != 5)
		e = 1;
	else if (LOG_INFO != 6)
		e = 1;
	else if (LOG_DEBUG != 7)
		e = 1;

	if (e != 0) {
		warnx("slog() basic assumption was wrong");
		warnx("your libc does not define syslog.h LOG_* as expected");
		errx(1, "exiting");
	}

	openlog(NULL, LOG_NDELAY|LOG_PID, facil);
}

/*
 * Wrapper around closelog() to close the syslog fd.
 */
void
slog_close()
{
	closelog();
}

int
slog_upto(int prio)
{
	int prev = upto;
	upto = prio;

	setlogmask(LOG_UPTO(prio));

	return prev;
}

int
slog_do_stderr(int x)
{
	int prev = do_stderr;
	do_stderr = x;
	return prev;
}

int
slog_do_syslog(int x)
{
	int prev = do_syslog;
	do_syslog = x;
	return prev;
}
