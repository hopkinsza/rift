// slog revision 1

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "slog.h"

static int facil = SLOG_FACILITY;

static int upto = LOG_DEBUG;

static int do_stderr = 1;
static int do_syslog = 0;

/*
 * syslog(3) lookalike that also allows printing to stderr only.
 * This is the default.
 *
 * Currently, only netbsd syslog(3) can do this with its LOG_NLOG flag,
 * otherwise we would use it directly.
 * Hopefully, LOG_NLOG will eventually become universal and this wrapper will
 * no longer be necessary.
 *
 * When printing to stderr, a %m specifier is only recognized at the end.
 * Argument 'level' should be a level only, not OR'd with facility.
 */
void
slog(int level, const char *fmt, ...)
{
	va_list ap;

	if (do_stderr && level <= upto) {
		va_start(ap, fmt);

		/*
		 * Determine if %m is there.
		 */

		int do_errno = 0;
		size_t fmtlen = strlen(fmt) + 1;
		char *fmtcpy = malloc(fmtlen);

		if (fmtcpy == NULL) {
			// malloc failed
			do_errno = 0;
		} else {
			strcpy(fmtcpy, fmt);
			if (fmtlen >= 3) {
				// pointer to the last non-null character
				char *p = fmtcpy + (fmtlen - 2);
				if (*(p-1) == '%' && *p == 'm') {
					// if fmt ended with "%m",
					// NULL out both of those chars
					*(p-1) = '\0';
					*p = '\0';
					do_errno = 1;
				}
			}
		}

		/*
		 * Then print accordingly.
		 */

		if (do_errno) {
			vfprintf(stderr, fmtcpy, ap);
			fprintf(stderr, "%s", strerror(errno));
			fprintf(stderr, "\n");
		} else {
			vfprintf(stderr, fmt, ap);
			fprintf(stderr, "\n");
		}

		va_end(ap);
	}

	if (do_syslog) {
		va_start(ap, fmt);
		vsyslog(level, fmt, ap);
		va_end(ap);
	}
}

/*
 * Wrappers around openlog() and closelog().
 */
void
slog_open()
{
	openlog(NULL, LOG_NDELAY|LOG_PID, facil);
}
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
