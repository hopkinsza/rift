/*
 * SPDX-License-Identifier: 0BSD
 */

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

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
void init_child();
void reset_procs();

int fd_console;
pid_t cpid;	// pid of rc or single user shell
sigset_t bmask;	// signal block mask -- full

const struct timespec one_sec = { 1, 0 };

int
main(int argc, char *argv[])
{
	if (getpid() != 1)
		return 1;

	/*
	 * Initial setup.
	 */

	chdir("/");

	// Block all signals.
	sigfillset(&bmask);
	sigprocmask(SIG_BLOCK, &bmask, NULL);

	// Create initial session, set default PATH and umask.
	setsid();
	setenv("PATH", _PATH_STDPATH, 1);
	umask(0022);

	// Open fd for /dev/console and make sure STD{IN,OUT,ERR} use it.
	fd_console = open("/dev/console", O_RDWR|O_NOCTTY);
	if (fd_console == -1) {
		warn("open /dev/console failed");
		hang();
	}
	dup2(fd_console, 0);
	dup2(fd_console, 1);
	dup2(fd_console, 2);

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
			warnx("unrecognized flag: -%c", ch);
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
			warnx("entering singleuser mode");
		} else if (state == SINGLEUSER) {
			state = MULTIUSER;
			warnx("entering multiuser mode");
		}

		reset_procs();
		cpid = spawn_for(state);
		break;
	case SIGUSR1:
		do_respawn = 0;
		break;
	case SIGUSR2:
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
		warn("fork failed");
		hang();
	}

	if (pid == 0) {
		init_child();

		if (s == MULTIUSER) {
			execl(INIT_RC_PATH, INIT_RC_NAME, NULL);
			warn("(child) exec " INIT_RC_PATH " failed");
			exit(1);
		} else if (s == SINGLEUSER) {
			execl(INIT_SINGLE_PATH, INIT_SINGLE_NAME, NULL);
			// fallback to a shell
			execl("/bin/sh", "-sh", NULL);

			// If you don't have either of those, you are seriously screwed.
			// Tell init not to bother re-execing anymore.
			kill(1, SIGUSR1);
			warn("(child) could not exec " INIT_SINGLE_PATH " or /bin/sh");
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
 * Initialization to be run in a child process.
 */
void
init_child()
{
	sigprocmask(SIG_UNBLOCK, &bmask, NULL);
	if (setsid() == -1)
		warn("(child) setsid(2) failed");
	if (ioctl(fd_console, TIOCSCTTY, 1) == -1)
		warn("(child) ioctl to make /dev/console the ctty failed");
}

/*
 * Terminate all existing processes.
 */
void
reset_procs()
{
	warnx("waiting up to 10 seconds for processes to exit...");

	kill(-1, SIGTERM);
	kill(-1, SIGHUP);
	kill(-1, SIGCONT);
	if (wait_for_upto(8) == 0)
		return;

	kill(-1, SIGKILL);
	if (wait_for_upto(2) == 0)
		return;

	warnx("some processes would not die; ps axl advised");
}
