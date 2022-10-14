#include <sys/file.h> // for flock(2) on linux

// #include <err.h>
// #include <errno.h>
#include <fcntl.h>
// #include <getopt.h>
// #include <libgen.h>
// #include <signal.h>
// #include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
// #include <string.h>
#include <syslog.h> // for LOG_* level constants
#include <time.h> // for printing times
#include <unistd.h>

#include <slog.h>

#include "extern.h"

/*
 * Argument `c' is a character which corresponds to a one-letter flag.
 * Argument `name' is the service name.
 * This function does not return, and instead calls exit(3).
 */
void
status(char c, char *name)
{
	/*
	 * cd to the directory.
	 */
	{
		int r;
		char *dir;

		r = asprintf(&dir, "%s/fsv-%ld/%s",
		    FSV_STATE_PREFIX, (long)geteuid(), name);
		if (chdir(dir) == -1) {
			slog(LOG_ERR, "chdir(%s) failed: %m", dir);
			exit(1);
		}
		free(dir);
	}

	/*
	 * Now open info.struct and report status.
	 */

	int e;
	int fd_info;

	fd_info = open("info.struct", O_RDONLY);
	if (fd_info == -1) {
		slog(LOG_ERR, "open(info.struct) failed: %m");
		exit(1);
	}

	// see if we can place a flock(2)
	if (flock(fd_info, LOCK_EX|LOCK_NB) == -1)
		e = 0;
	else
		e = 1;

	// if -S, just exit now
	if (c == 'S')
		exit(e);

	// otherwise, read the data
	struct allinfo ai;

	{
		ssize_t r;
		r = read(fd_info, &ai, sizeof(ai));
		if (r == -1) {
			slog(LOG_ERR, "read from info.struct failed: %m");
			exit(1);
		} else if (r < sizeof(ai)) {
			slog(LOG_ERR, "unexpected data in info.struct");
			exit(1);
		}
	}

	// print as needed
	if (c == 'p') {
		printf("%ld\n", (long)ai.fsv.pid);
		printf("%ld\n", (long)ai.chld[0].pid);
		printf("%ld\n", (long)ai.chld[1].pid);
	} else if (c == 's') {
		printf("fsv\n");

		printf("pid: ");
		if (ai.fsv.pid == 0)
			printf("not running\n");
		else
			printf("%ld\n", (long)ai.fsv.pid);

		char tstr[64];
		struct tm *tm;

		tm = localtime(&ai.fsv.since.tv_sec);
		strftime(tstr, sizeof(tstr), "%F %T %z (%r %Z)", tm);
		printf("since: %s\n", tstr);

		tm = gmtime(&ai.fsv.since.tv_sec);
		strftime(tstr, sizeof(tstr), "%F %T %z (%r %Z)", tm);
		printf("since: %s\n", tstr);

		printf("since (timespec): { %ld, %09ld }\n",
		       (long)ai.fsv.since.tv_sec, ai.fsv.since.tv_nsec);
		printf("gaveup: %d\n", ai.fsv.gaveup);

		for (int i=0; i<2; i++) {
			struct fsv_child *p = &ai.chld[i];

			printf("\n");
			if (i == 0)
				printf("cmd\n");
			else if (i == 1)
				printf("log\n");

			printf("pid: ");
			if (p->pid == -1)
				printf("never started\n");
			else if (p->pid == 0)
				printf("not running\n");
			else
				printf("%ld\n", (long)p->pid);

			printf("total_execs: %ld\n", p->total_execs);
			printf("recent_execs: %ld\n", p->recent_execs);
			printf("max_recent_execs: %ld\n", p->max_recent_execs);
			printf("recent_secs: %ld\n", p->recent_secs);
		}
	}

	exit(e);
}
