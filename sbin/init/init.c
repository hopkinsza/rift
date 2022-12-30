/*
 * SPDX-License-Identifier: 0BSD
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#if defined(BSD)
#include <util.h>
#endif

#if defined(__linux__)
// glibc has login_tty(3) in utmp.h
#include <utmp.h>
#endif

#include "rfconf.h"

#define INIT_RC_PATH ETCDIR "/rc"
#define INIT_RC_NAME "rc"

#define INIT_SINGLE_PATH ETCDIR "/rc.single"
#define INIT_SINGLE_NAME "rc.single"

enum states {
	MULTIUSER,
	SINGLEUSER
} state;

int spawn_for(enum states);
int wait_for_upto(int);
void hang();
void reset_procs();
void w(const char *, ...);
void wx(const char *, ...);

pid_t cpid;	// pid of rc or single user shell
sigset_t bmask;	// signal block mask -- full

const struct timespec one_sec = { 1, 0 };

void
nop(int i)
{
}

int
main(int argc, char *argv[])
{
	if (getpid() != 1)
		return 1;

	/*
	 * Initial setup.
	 */

	// Block all signals.
	sigfillset(&bmask);
	sigprocmask(SIG_BLOCK, &bmask, NULL);

	// For all signals we deal with, set its handler to a no-op function.
	// On BSD, it seems a signal will not be posted to us if it's ignored,
	// even if it is blocked.
	{
		struct sigaction sa;
		memset(&sa, 0, sizeof(sa));

		sa.sa_flags = SA_NOCLDSTOP;
		sa.sa_handler = nop;
		sigaction(SIGALRM, &sa, NULL);
		sigaction(SIGCHLD, &sa, NULL);
		sigaction(SIGTERM, &sa, NULL);
		sigaction(SIGTSTP, &sa, NULL);
		sigaction(SIGHUP, &sa, NULL);
	}

	// Create initial session.
	setsid();

	// Other random setup.
	chdir("/");
	setenv("PATH", _PATH_STDPATH, 1);
	umask(0022);

	/*
	 * Process flags, then spawn the first process.
	 */

	state = MULTIUSER;

	int ch;
	while ((ch = getopt(argc, argv, "s")) != -1) {
		switch (ch) {
		case 's':
			state = SINGLEUSER;
			break;
		default:
			wx("unrecognized flag: -%c", ch);
			break;
		}
	}

	cpid = spawn_for(state);

	/*
	 * Main loop.
	 */

	// Set an alarm to catch corner-cases.
	// For example, a process could exit before waiting on its children,
	// then those zombie children are re-parented to init.
	alarm(90);

	int do_respawn = 1;

	int sig;
	while (sigwait(&bmask, &sig) == 0) switch (sig) {
	case SIGALRM:
	case SIGCHLD:
	{
		int status;
		pid_t pid;

		alarm(90);

		// wait on all possible children
		while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
			if (pid == cpid && do_respawn) {
				raise(SIGTERM);
			}
		}
		break;
	}
	case SIGTERM:
		if (state == MULTIUSER) {
			state = SINGLEUSER;
			wx("entering singleuser mode");
		} else if (state == SINGLEUSER) {
			state = MULTIUSER;
			wx("entering multiuser mode");
		}

		reset_procs();
		cpid = spawn_for(state);
		break;
	case SIGTSTP:
		do_respawn = 0;
		break;
	case SIGHUP:
		do_respawn = 1;
		break;
	}
}

/*
 * Spawn /etc/rc for MULTIUSER mode, or /etc/rc.single for SINGLEUSER mode.
 */
int
spawn_for(enum states s)
{
	int pid = fork();

	if (pid == -1) {
		w("fork failed");
		hang();
	}

	if (pid == 0) {
		// Unblock signals.
		sigprocmask(SIG_UNBLOCK, &bmask, NULL);

		// Call login_tty(3) on constty or console.
		// It will call setsid(2), set up STD{IN,OUT,ERR},
		// set the controlling tty, and then close fd if needed.
		int fd;
#if defined(_PATH_CONSTTY)
		fd = open(_PATH_CONSTTY, O_RDWR|O_NOCTTY);
#else
		fd = open(_PATH_CONSOLE, O_RDWR|O_NOCTTY);
#endif
		login_tty(fd);

		if (s == MULTIUSER) {
			execl(INIT_RC_PATH, INIT_RC_NAME, NULL);
			w("(child) exec " INIT_RC_PATH " failed");
			exit(1);
		} else if (s == SINGLEUSER) {
			execl(INIT_SINGLE_PATH, INIT_SINGLE_NAME, NULL);
			w("(child) exec " INIT_SINGLE_PATH " failed");
			wx("(child) falling back to /bin/sh");
			execl("/bin/sh", "-sh", NULL);

			// If you don't have either of those, you are seriously screwed.
			// Tell init not to bother re-execing anymore.
			kill(1, SIGTSTP);
			w("(child) could not exec " INIT_SINGLE_PATH " or /bin/sh");
			fprintf(stderr, "\n");
			fprintf(stderr, "your system is screwed;\n");
			fprintf(stderr, "you will need to perform a daring rescue or reinstall");
			exit(1);
		}
	}

	return pid;
}

/*
 * Wait for all existing processes to exit, with a timeout.
 * Return 0 if everything has exited, otherwise 1.
 */
int
wait_for_upto(int secs)
{
	int pid;

	for (int i=0; i<secs; i++) {
		while ((pid = waitpid(-1, NULL, WNOHANG)) > 0)
			;

		if (pid == -1 && errno == ECHILD) {
			// All children successfully waited for.
			return 0;
		}

		nanosleep(&one_sec, NULL);
	}

	// Unwaited children remain.
	return 1;
}

/*
 * Sleep for a small bit, then exit, so a user can read an error message.
 */
void
hang()
{
	for (int i=0; i<6; i++) {
		nanosleep(&one_sec, NULL);
	}

	exit(1);
}

/*
 * Terminate all existing processes.
 */
void
reset_procs()
{
	wx("waiting up to 10 seconds for processes to exit...");

	kill(-1, SIGTERM);
	kill(-1, SIGHUP);
	kill(-1, SIGCONT);
	if (wait_for_upto(8) == 0)
		return;

	kill(-1, SIGKILL);
	if (wait_for_upto(2) == 0)
		return;

	wx("some processes would not die; ps axl advised");
}

/*
 * warn(3) and warnx(3) clones.
 * They open a new fd to the console to print to every time;
 * on BSD, it seems all existing file descriptors to a tty get yiffed if
 * its controlling process exits.
 */
void
w(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	int fd = open(_PATH_CONSOLE, O_RDWR|O_NOCTTY);
	dprintf(fd, "init: ");
	vdprintf(fd, fmt, ap);
	dprintf(fd, ": %s\n", strerror(errno));
	close(fd);

	va_end(ap);
}
void
wx(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);

	int fd = open(_PATH_CONSOLE, O_RDWR|O_NOCTTY);
	dprintf(fd, "init: ");
	vdprintf(fd, fmt, ap);
	dprintf(fd, "\n");
	close(fd);

	va_end(ap);
}
