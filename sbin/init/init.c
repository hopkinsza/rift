/*
 * SPDX-License-Identifier: 0BSD
 */

#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "rfconf.h"

#define RC_PATH ETCDIR "/rc"
#define RC_NAME "rc"
#define RC_SINGLE_PATH ETCDIR "/rc.single"
#define RC_SINGLE_NAME "rc.single"
#define RC_INT_PATH ETCDIR "/rc.int"
#define RC_INT_NAME "rc.int"

enum modes { SINGLE, MULTI };

pid_t clean_fork();
pid_t spawn_rc(enum modes, int);
int spawn_rc_int();
int wait_while_procs(int);
void smite();

#define notice(...)  l(LOG_NOTICE,  __VA_ARGS__)
#define warning(...) l(LOG_WARNING, __VA_ARGS__)
#define error(...)   l(LOG_ERR,     __VA_ARGS__)
void l(int, const char *, ...);

void nop(int i) {}

int
main(int argc, char *argv[])
{
	/*
	 * Be amicable.
	 */

	if (getpid() != 1)
		errx(1, "already running");

	if (geteuid() != 0)
		errx(1, "must run as root");

	/*
	 * Initialize.
	 */

	// paranoia
	close(0);
	close(1);
	close(2);
{
	int fd;
	fd = open(_PATH_DEVNULL, O_RDWR|O_NOCTTY);
	dup2(fd, 0);
	dup2(fd, 1);
	dup2(fd, 2);
	if (fd > 2)
		close(fd);
}

	// create initial session
	setsid();
	// establish initial user (a no-op on some systems)
	setlogin("root");

	chdir("/");
	setenv("PATH", _PATH_STDPATH, 1);
	umask(0022);

	openlog("init", 0, LOG_AUTH);

	/*
	 * Process flags.
	 */

	enum modes mode = MULTI;
{
	int ch;
	while((ch = getopt(argc, argv, "s")) != -1) {
		switch (ch) {
		case 's':
			mode = SINGLE;
			break;
		default:
			warning("unrecognized flag: -%c", ch);
			break;
		}
	}

	if (optind != argc)
		notice("ignoring excess arguments");
}

	/*
	 * Block signals.
	 */

	// Set handlers to a no-op function;
	// this ensures they are not ignored.
	// An ignored signal (SIG_IGN) may not be posted at all.
	// These will reset to SIG_DFL on exec.
{
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = nop;
	sigaction(SIGALRM, &sa, NULL);
	sigaction(SIGCHLD, &sa, NULL);
	sigaction(SIGINT,  &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGTSTP, &sa, NULL);
	sigaction(SIGHUP,  &sa, NULL);
}

	// block
	sigset_t bmask;
	sigemptyset(&bmask);
	sigaddset(&bmask, SIGALRM);
	sigaddset(&bmask, SIGCHLD);
	sigaddset(&bmask, SIGINT);
	sigaddset(&bmask, SIGTERM);
	sigaddset(&bmask, SIGTSTP);
	sigaddset(&bmask, SIGHUP);
	sigprocmask(SIG_BLOCK, &bmask, NULL);

	/*
	 * Main loop.
	 */

	// Set an alarm to catch corner-cases.
	// For example, a process could exit before waiting on its children,
	// then those zombie children are re-parented to init.
{
	struct itimerval it = { {60, 0}, {60, 0}};
	setitimer(ITIMER_REAL, &it, NULL);
}

	pid_t rc = spawn_rc(mode, 0);

	int death = 0;
	int sig;
	while (sigwait(&bmask, &sig) == 0) switch (sig) {
	case SIGALRM:
	case SIGCHLD:
	{
		int status;
		pid_t pid;

		// wait on all possible children
		while ((pid = waitpid(-1, &status, WNOHANG|WUNTRACED)) > 0) {
			if (pid != rc)
				continue;

			if (WIFSTOPPED(status)) {
				notice("rc stopped, sending SIGCONT");
				kill(pid, SIGCONT);
				continue;
			}

			// rc has exited.
			// Avoid possible collisions with other PIDs
			// and waitpid(2)'s return values.
			rc = -2;

			if (death)
				continue;

			// Multiuser rc exited 0, ignore.
			if (mode == MULTI &&
			    WIFEXITED(status) && WEXITSTATUS(status) == 0) {
					continue;
			}

			// Swap mode.
			if (mode == SINGLE)
				mode = MULTI;
			else
				mode = SINGLE;
			rc = spawn_rc(mode, 1);
		}
		break;
	}
	case SIGINT:
		notice("got SIGINT");
		if (spawn_rc_int() == -1) {
			mode = SINGLE;
			rc = spawn_rc(mode, 1);
		}
		break;
	case SIGTERM:
		notice("got SIGTERM");
		if (death) {
			break;
		} else {
			mode = SINGLE;
			rc = spawn_rc(mode, 1);
		}
		break;
	case SIGTSTP:
		notice("got SIGTSTP");
		death = 1;
		break;
	case SIGHUP:
		notice("got SIGHUP");
		death = 0;
		break;
	}

	// unreachable
	abort();
}

/*
 * Wrapper around fork(2) with signals unblocked in child.
 * Also clean up a possible stray syslog(3) fd.
 */
pid_t
clean_fork()
{
	pid_t pid = fork();

	switch (pid) {
	case -1:
		return -1;
	case 0:
	{
		// child

		// unblock signals; handlers will be restored on exec
		sigset_t bmask;
		sigemptyset(&bmask);
		sigprocmask(SIG_SETMASK, &bmask, NULL);

		closelog();

		return 0;
		break;
	}
	default:
		// parent
		return pid;
		break;
	}
}

/*
 * Spawn /etc/rc for multi-user mode, or /etc/rc.single for single-user mode.
 */
pid_t
spawn_rc(enum modes mode, int do_smite)
{
	if (mode == SINGLE)
		notice("entering single-user mode");
	else
		notice("entering multi-user mode");

	if (do_smite)
		smite();

	pid_t pid = clean_fork();
	// If the fork(2) fails, keep trying ad infinitum.
	while (pid == -1) {
		error("fork() failed: %s", strerror(errno));
		notice("trying again in 5 seconds...");
		nanosleep(&(struct timespec){5,0}, NULL);
		pid = clean_fork();
	}

	switch (pid) {
	case 0:
	{
		// child

		// Set up a new session with controlling terminal.
		// There are 2 historical implementations for how a
		// session leader acquires a controlling terminal:
		// 1. the sysv way:
		//    - open(2) a tty file without O_NOCTTY in the flags
		// 2. the bsd way:
		//    - use ioctl(2) with TIOCSCTTY on a tty file descriptor
		//    - or better yet, use wrapper function login_tty(3)
		//
		// This works for either, essentially replicating login_tty(3).

		setsid();

		int fd;
		// NetBSD distinguishes between /dev/console for log messages
		// and /dev/constty for the console tty.
#if defined(_PATH_CONSTTY)
		fd = open(_PATH_CONSTTY, O_RDWR);
#else
		fd = open(_PATH_CONSOLE, O_RDWR);
#endif

#if defined(TIOCSCTTY)
		ioctl(fd, TIOCSCTTY, NULL);
#endif

		dup2(fd, 0);
		dup2(fd, 1);
		dup2(fd, 2);
		if (fd > 2)
			close(fd);

		if (mode == MULTI) {
			execl(RC_PATH, RC_NAME, NULL);
			error("child: exec " RC_PATH " failed: %s",
			      strerror(errno));
			exit(1);
		} else {
			execl(RC_SINGLE_PATH, RC_SINGLE_NAME, NULL);
			error("child: exec " RC_SINGLE_PATH " failed: %s",
			      strerror(errno));

			notice("child: trying /bin/sh");
			// don't leak syslog fd
			closelog();
			execl("/bin/sh", "-sh", NULL);

			// tell init to not bother respawning
			kill(1, SIGTSTP);
			warnx("child: could not exec " RC_SINGLE_PATH
			      " or /bin/sh");
			nanosleep(&(struct timespec){5,0}, NULL);
			exit(1);
		}
		break;
	}
	default:
		// parent
		return pid;
		break;
	}
}

/*
 * Spawn /etc/rc.int.
 */
int
spawn_rc_int()
{
	pid_t pid = clean_fork();
	// If the fork(2) fails, retry once.
	if (pid == -1) {
		error("fork() for SIGINT failed: %s", strerror(errno));
		notice("trying again in 1 second...");
		nanosleep(&(struct timespec){1,0}, NULL);
		pid = clean_fork();
	}

	switch (pid) {
	case -1:
		error("fork() for SIGINT failed again: %s", strerror(errno));
		return -1;
		break;
	case 0:
		// child
		execl(RC_INT_PATH, RC_INT_NAME, NULL);
		error("child: exec " RC_INT_PATH " failed: %s",
		      strerror(errno));
		notice("child: telling init to go single-user");
		kill(1, SIGTERM);
		exit(1);
		break;
	default:
		// parent
		return pid;
		break;
	}
}

/*
 * wait(2) on all child processes, up to a timeout.
 */
int
wait_while_procs(int timeout)
{
	for (int i=0; i<timeout; i++) {
		int pid;

		while ((pid = waitpid(-1, NULL, WNOHANG)) > 0)
			;

		if (pid == -1 && errno == ECHILD) {
			// no children remain
			return 0;
		}

		// children remain
		nanosleep(&(struct timespec){1,0}, NULL);
	}

	// timeout reached
	return -1;
}

/*
 * Terminate all existing processes.
 */
void
smite()
{
	notice("waiting up to 5 seconds for processes to exit...");

	// send TERM 0.1 seconds before HUP
	kill(-1, SIGTERM);
	nanosleep(&(struct timespec){0,100000000}, NULL);
	kill(-1, SIGHUP);
	if (wait_while_procs(5) == 0)
		return;

	notice("waiting up to 3 seconds for processes to force-exit...");
	kill(-1, SIGKILL);
	if (wait_while_procs(3) == 0)
		return;

	warning("some processes would not die; ps axl advised");
}

/*
 * Logging to the console and syslog.
 * Opens a new fd to the console to print to every time;
 * existing file descriptors to a tty get yiffed if
 * its controlling process exits.
 */
void
l(int priority, const char *fmt, ...)
{
	va_list ap;

	// console
	va_start(ap, fmt);
	int fd = open(_PATH_CONSOLE, O_RDWR|O_CLOEXEC|O_NOCTTY);
	dprintf(fd, "init: ");
	vdprintf(fd, fmt, ap);
	dprintf(fd, "\n");
	close(fd);
	va_end(ap);

	// syslog
	va_start(ap, fmt);
	vsyslog(priority, fmt, ap);
	va_end(ap);
}
