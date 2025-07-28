/*
 * SPDX-License-Identifier: 0BSD
 */

#include <sys/wait.h>

#include <err.h>
#include <libgen.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "rfconf.h"

#define RC_SHUTDOWN_PATH ETCDIR "/rc.shutdown"
#define RC_SHUTDOWN_NAME "rc.shutdown"

#include "osind_reboot.h"

#if !defined(HPR_GRACE)
#define HPR_GRACE 5
#endif

int sleep_while_procs(int);
void usage();

int
main(int argc, char *argv[])
{
	if (geteuid() != 0)
		errx(1, "must run as root");

	/*
	 * Inspect argv[0] and set default action accordingly.
	 */

	int action = 'p'; // ascii h, p, or r
	char *progname;

	// Set progname.
	// Have to make a copy of argv[0] since basename() can modify its arg,
	// and this can never be freed...
	// BSDs and Solaris could just use getprogname();
	// tempting to require libbsd on linux for this.
	char *arg0 = malloc(strlen(argv[0]) + 1);
	if (arg0 == NULL)
		err(1, "malloc() failed");
	strcpy(arg0, argv[0]);
	progname = basename(arg0);

	if (strcmp(progname, "halt") == 0)
		action = 'h';
	else if (strcmp(progname, "poweroff") == 0)
		action = 'p';
	else if (strcmp(progname, "reboot") == 0)
		action = 'r';

	/*
	 * Process flags.
	 */

	int do_clean = 1;
	int do_log = 1;
	int do_rcshutdown = 1;
	int do_sync  = 1;
{
	int ch;
	while ((ch = getopt(argc, argv, "hlnpqrS")) != -1) {
		switch(ch) {
		case 'h':
			action = 'h';
			break;
		case 'l':
			do_log = 0;
			break;
		case 'n':
			do_sync = 0;
			break;
		case 'p':
			action = 'p';
			break;
		case 'q':
			do_clean = 0;
			break;
		case 'r':
			action = 'r';
			break;
		case 'S':
			do_rcshutdown = 0;
			break;
		default:
			usage();
			break;
		}
	}
}

	if (do_rcshutdown) {
		warnx("running " RC_SHUTDOWN_PATH);

		pid_t pid;
		switch(pid = fork()) {
		case -1:
			warn("fork() failed");
			break;
		case 0:
			// child
			execl(RC_SHUTDOWN_PATH, RC_SHUTDOWN_NAME, NULL);
			err(1, "child: exec " RC_SHUTDOWN_PATH " failed");
			break;
		default:
			// parent
			wait(NULL);
			break;
		}
	}

	if (do_log) {
		// getlogin(3) can report user more accurately if 'su' was used.
		char *user = getlogin();
		if (user == NULL) {
			struct passwd *p = getpwuid(getuid());
			if (p == NULL) {
				user = "???";
			} else {
				user = p->pw_name;
			}
		}

		openlog(NULL, 0, LOG_AUTH);
		switch (action) {
		case 'h':
			syslog(LOG_CRIT, "halted by %s", user);
			break;
		case 'p':
			syslog(LOG_CRIT, "powered off by %s", user);
			break;
		case 'r':
			syslog(LOG_CRIT, "rebooted by %s", user);
			break;
		}
		closelog();
	}

	/*
	 * This is where the fun begins.
	 */

	// ignore relevant signals
{
	struct sigaction sa = { .sa_handler = SIG_IGN };
	sigaction(SIGHUP,  &sa, NULL);
	sigaction(SIGINT,  &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGPIPE, &sa, NULL);
	sigaction(SIGTSTP, &sa, NULL);
}

	// tell init to not respawn processes
	kill(1, SIGTSTP);

	if (do_clean) {
		warnx("waiting up to %d seconds for processes to exit...",
		    HPR_GRACE);
		// send TERM 0.1 seconds before HUP
		kill(-1, SIGTERM);
		nanosleep(&(struct timespec){0,100000000}, NULL);
		kill(-1, SIGHUP);
		kill(-1, SIGCONT);
		sleep_while_procs(HPR_GRACE);
	}

	if (do_sync) {
		warnx("sync()");
		sync();
	}

	switch (action) {
	case 'h':
		warnx("halt");
		osind_reboot(OSIND_RB_HALT);
		break;
	case 'p':
		warnx("poweroff");
		osind_reboot(OSIND_RB_POWEROFF);
		break;
	case 'r':
		warnx("reboot");
		osind_reboot(OSIND_RB_REBOOT);
		break;
	default:
		// should never happen
		kill(1, SIGHUP);
		abort();
		break;
	}
}

int
sleep_while_procs(int timeout)
{
	for (int i=0; i<timeout; i++) {
		if(kill(-1, 0) == -1) {
			// no processes remain
			return 0;
		}
		// processes remain
		nanosleep(&(struct timespec){1,0}, NULL);
	}

	// processes remain
	return -1;
}

void
usage()
{
	fprintf(stderr, "usage: <halt|poweroff|reboot> [-hlnpqrS]\n");
	fprintf(stderr, "    -h  halt\n");
	fprintf(stderr, "    -l  do not log to syslog(3)\n");
	fprintf(stderr, "    -n  do not sync(2)\n");
	fprintf(stderr, "    -p  poweroff\n");
	fprintf(stderr, "    -q  do not give processes a chance to shut down "
	    "cleanly\n");
	fprintf(stderr, "    -r  reboot\n");
	fprintf(stderr, "    -S  do not run " RC_SHUTDOWN_PATH "\n");
	exit(1);
}
