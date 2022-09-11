#ifndef _EXTERN_H_
#define _EXTERN_H_

#define FSV_VERSION "0.0.0"

#ifndef FSV_CMDDIR_PREFIX
#define FSV_CMDDIR_PREFIX "/var/tmp"
#endif

#include <signal.h>

struct fsv_parent {
	// PID is 0 if not running
	pid_t pid;
	// running or stopped since when?
	struct timespec since;

	// Only relevant if not running.
	// True if fsv "gave up" on restarting one of
	// the programs because its timeout was set to 0
	int gaveup;

	// configuration
	long timeout;
};

struct fsv_child {
	// PID is 0 if not running
	pid_t pid;
	// running or stopped since when?
	struct timespec since;

	// tracking
	long total_execs;
	long recent_execs;

	// If total_execs > 0, most recent exit status as returned by wait(2);
	// use W* macros to examine.
	int status;

	// configuration
	long max_recent_execs;
	long recent_secs;
};

struct allinfo {
	struct fsv_parent fsv;
	struct fsv_child chld[2];
};

// file descriptor to /dev/null
extern int fd_devnull;

// signal block mask
extern sigset_t bmask;

/*
 * slog.c
 */

void slog(int, const char *, ...);
void slog_init();
void slog_close();
int slog_upto(int);
int slog_do_stderr(int);
int slog_do_syslog(int);

#endif // !_EXTERN_H_
