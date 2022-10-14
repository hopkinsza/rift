#ifndef _EXTERN_H_
#define _EXTERN_H_

#define FSV_VERSION "0.0.0"

#ifndef FSV_STATE_PREFIX
#define FSV_STATE_PREFIX "/tmp"
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
 * status.c
 */
void status(char, char *);

#endif // !_EXTERN_H_
