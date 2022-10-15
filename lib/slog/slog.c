#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "slog.h"

static int upto = LOG_DEBUG;

static int do_stderr = 1;
static int do_syslog = 0;

/*
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
slog_open(const char *ident, int logopt, int facil)
{
	do_syslog = 1;
	do_stderr = 0;

	/*
	 * extract LOG_NLOG and LOG_PERROR from logopt
	 */

	int nl = LOG_NLOG & logopt;
	if (nl != 0) {
		do_syslog = 0;
		logopt ^= LOG_NLOG;
	}

	int pe = LOG_PERROR & logopt;
	if (pe != 0) {
		do_stderr = 1;
		logopt ^= LOG_PERROR;
	}

	openlog(ident, logopt, facil);
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

	// not necessary, but why not
	setlogmask(LOG_UPTO(prio));

	return prev;
}
