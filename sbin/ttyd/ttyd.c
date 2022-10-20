#include <sys/wait.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <syslog.h>

#include <slog.h>

#include "extern.h"

#define GETTY_TIMEOUT 30
#define GETTY_TOOFAST 5
#define GETTY_QUICKIE_MAX 2

#define TTYD_CHILD_MAX 64

/*
 * "X" versions are an ugly hack to get around the fact that you cannot
 * do arithmetic in the #define.
 * The regular value is the length of the array; the X version is used
 * for scanf or looping through gargv, which must be null terminated.
 */

#define TTYD_DEV_LEN 16
#define TTYD_DEV_LENX 15

#define TTYD_GARGV_LEN 16

#define TTYD_GARGV_ARGLEN 32
#define TTYD_GARGV_ARGLENX 31

// preprocessor shenanigans: stringify
#define STRFY2(x) #x
#define STRFY(x) STRFY2(x)

struct ttyd_child {
	pid_t pid;
	char tty[TTYD_DEV_LEN];
	char gargv[TTYD_GARGV_LEN][TTYD_GARGV_ARGLEN];
	// track restarting too quickly
	time_t started_at;
	int quickies;
};

int fork_getty(struct ttyd_child *);
void ensure_timer();
void load_config(const char *, struct ttyd_child *);
void usage();

// signal block mask
sigset_t bmask;
// timer id for failed gettys
timer_t tid;

int
main(int argc, char *argv[])
{
	/*
	 * Init global variables.
	 */

	sigemptyset(&bmask);
	sigaddset(&bmask, SIGALRM);
	sigaddset(&bmask, SIGCHLD);
	sigaddset(&bmask, SIGHUP);
	sigaddset(&bmask, SIGINT);
	sigaddset(&bmask, SIGTERM);

	if (timer_create(CLOCK_MONOTONIC, NULL, &tid) == -1)
		err(1, "timer_create() failed");

	/*
	 * General initialization.
	 *
	 * cur contains the current set of known children. 
	 * If a child pid is 0, that means either the fork() has failed or
	 * it has exceeded QUICKIE_MAX.
	 * The alarm should have been set to start it up again later.
	 */

	struct ttyd_child cur[TTYD_CHILD_MAX];
	memset(cur, 0, sizeof(cur));

	// set up logging
	slog_open(NULL, LOG_PID|LOG_PERROR|LOG_NLOG, LOG_AUTH);
	slog_upto(LOG_INFO);

	/*
	 * Process flags.
	 */

	char *conf = CONFDIR "/ttyd.conf";
	int do_daemon = 0;

	const char *getopt_str = "Bbc:dL:XxYy";
	int ch;
	while ((ch = getopt(argc, argv, getopt_str)) != -1) switch (ch) {
	case 'B':
		do_daemon = 2;
		break;
	case 'b':
		do_daemon = 1;
		slog_open(NULL, LOG_PID, LOG_AUTH);
		break;
	case 'c':
		conf = optarg;
		break;
	case 'd':
		slog_upto(LOG_DEBUG);
		break;
	case 'L':
		if (strcmp(optarg, "emerg") == 0)
			slog_upto(LOG_EMERG);
		else if (strcmp(optarg, "alert") == 0)
			slog_upto(LOG_ALERT);
		else if (strcmp(optarg, "crit") == 0)
			slog_upto(LOG_CRIT);
		else if (strcmp(optarg, "err") == 0)
			slog_upto(LOG_ERR);
		else if (strcmp(optarg, "warning") == 0)
			slog_upto(LOG_WARNING);
		else if (strcmp(optarg, "notice") == 0)
			slog_upto(LOG_NOTICE);
		else if (strcmp(optarg, "info") == 0)
			slog_upto(LOG_INFO);
		else if (strcmp(optarg, "debug") == 0)
			slog_upto(LOG_DEBUG);
		else
			errx(64, "unrecognized loglevel for -v");
		break;
	case 'Y':
		slog_open(NULL, LOG_PID, LOG_AUTH);
		break;
	case 'y':
		slog_open(NULL, LOG_PID|LOG_PERROR, LOG_AUTH);
		break;
	default:
		usage();
		break;
	}
	argc -= optind;
	argc += optind;

	if (access(conf, R_OK) == -1) {
		slog(LOG_ERR, "fatal: could not access config '%s': %m", conf);
		exit(1);
	}

	// daemonize
	if (do_daemon == 1) {
		if (daemon(0, 0) == -1) {
			slog(LOG_ERR, "fatal: daemon() failed: %m", conf);
			exit(1);
		}
	} else if (do_daemon == 2) {
		if (daemon(0, 1) == -1) {
			slog(LOG_ERR, "fatal: daemon() failed: %m", conf);
			exit(1);
		}
	}

	/*
	 * Main loop.
	 */

	// block signals now
	sigprocmask(SIG_BLOCK, &bmask, NULL);

	slog(LOG_DEBUG, "begin main loop");

	raise(SIGHUP);

	int sig;
	while (sigwait(&bmask, &sig) == 0) switch (sig) {
	case SIGALRM:
		slog(LOG_DEBUG, "> SIGALRM");
		// fork_getty for anyone in the list whose pid is 0
		for (int i=0; i<TTYD_CHILD_MAX; i++) {
			if (cur[i].tty[0] == '\0')
				break;
			if (cur[i].pid > 0)
				continue;

			cur[i].quickies = 0;
			fork_getty(&cur[i]);
		}
		break;
	case SIGCHLD:
	{
		slog(LOG_DEBUG, "> SIGCHLD");

		int status;
		pid_t epid;

		while ((epid = waitpid(-1, &status, WNOHANG)) > 0) {
			// some getty process has exited
			for (int i=0; i<TTYD_CHILD_MAX; i++) {
				if (cur[i].tty[0] == '\0')
					break;
				if (epid != cur[i].pid)
					continue;

				cur[i].pid = 0;
				do_logout(cur[i].tty, status);

				// check quickies
				struct timespec ts;
				clock_gettime(CLOCK_MONOTONIC, &ts);
				time_t now = ts.tv_sec;
				if ((now - cur[i].started_at) <= GETTY_TOOFAST) {
					cur[i].quickies++;
					if (cur[i].quickies > GETTY_QUICKIE_MAX) {
						// timeout
						ensure_timer();
						break;
					}
				} else {
					cur[i].quickies = 0;
				}

				fork_getty(&cur[i]);
			}
		}

		break;
	}
	case SIGHUP:
	{
		slog(LOG_DEBUG, "> SIGHUP");
		slog(LOG_NOTICE, "got SIGHUP, reloading config");

		struct ttyd_child new[TTYD_CHILD_MAX];
		memset(new, 0, sizeof(new));

		load_config(conf, new);
		slog(LOG_DEBUG, "dump array 'new'");
		for (int i=0; i<TTYD_CHILD_MAX; i++) {
			if (new[i].tty[0] == '\0')
				break;

			slog(LOG_DEBUG, "=> new[%d]", i);
			slog(LOG_DEBUG, "tty: %s", new[i].tty);

			for (int j=0; j<TTYD_GARGV_LEN; j++) {
				if (new[i].gargv[j][0] == '\0')
					break;
				slog(LOG_DEBUG, "'%s'", new[i].gargv[j]);
			}
		}

		// todo: check for duplicate tty entries

		/*
		 * If a tty has been added, fork off a new getty process for it.
		 * If a tty has been changed, do nothing; the new getty process
		 * will be used on logout.
		 * If a tty has been removed, TERM the getty process.
		 */

		// added or changed
		for (int n=0; n<TTYD_CHILD_MAX && new[n].tty[0] != '\0'; n++) {
			int found = 0;

			for (int c=0; c<TTYD_CHILD_MAX && cur[c].tty[0] != '\0'; c++) {
				if ((strcmp(cur[c].tty, new[n].tty)) == 0) {
					slog(LOG_DEBUG, "keeping tty: %s", cur[c].tty);
					found = 1;
					break;
				}
			}

			if (!found) {
				// this is a new tty, fork the process
				slog(LOG_DEBUG, "new tty: %s", new[n].tty);
				fork_getty(&new[n]);
			}
		}

		// removed
		for (int c=0; c<TTYD_CHILD_MAX && cur[c].tty[0] != '\0'; c++) {
			int found = 0;

			for (int n=0; n<TTYD_CHILD_MAX && new[n].tty[0] != '\0'; n++) {
				if ((strcmp(cur[c].tty, new[n].tty)) == 0) {
					found = 1;
					break;
				}
			}

			if (!found) {
				// tty has been removed, TERM
				slog(LOG_DEBUG, "removed tty: %s", cur[c].tty);
			}
		}

		memcpy(cur, new, sizeof(cur));
		break;
	}
	case SIGINT:
	case SIGTERM:
		slog(LOG_DEBUG, "> SIGINT or SIGTERM");
		slog(LOG_NOTICE, "got SIGINT or SIGTERM, exiting");

		// TERM any children
		for (int i=0; i<TTYD_CHILD_MAX; i++) {
			if (cur[i].pid > 0) {
				kill(SIGTERM, cur[i].pid);
			}
		}

		exit(0);
		break;
	}
}

int
fork_getty(struct ttyd_child *tc)
{
	slog(LOG_DEBUG, "forking for %s", tc->tty);
	pid_t pid;

	pid = fork();
	if (pid == 0) {
		slog_close();
		sigprocmask(SIG_UNBLOCK, &bmask, NULL);

		// todo: open tty as stdin and others as /dev/null

		// generate a real, null-terminated argv to exec
		char *argv[TTYD_GARGV_LEN+1];
		argv[TTYD_GARGV_LEN] = NULL;

		for (int i=0; i<TTYD_GARGV_LEN; i++) {
			argv[i] = tc->gargv[i];
		}

		setsid();
		execvp(argv[0], argv);

		// if exec failure, exit 64
		slog(LOG_ERR, "exec failed for %s: %m", tc->tty);
		exit(64);
	}

	// always set the time, even if the fork failed
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	tc->started_at = ts.tv_sec;

	if (pid == -1) {
		tc->pid = 0;
		slog(LOG_ERR, "fork() failed for %s: %m", tc->tty);
		ensure_timer();
		return -1;
	} else {
		tc->pid = pid;
		return pid;
	}
}

/*
 * If the timer is not already armed, arm it.
 */
void
ensure_timer()
{
	struct itimerspec cur;
	timer_gettime(tid, &cur);

	if (cur.it_value.tv_sec == 0 && cur.it_value.tv_nsec == 0) {
		struct itimerspec new = { {0,0}, {GETTY_TIMEOUT,0}};
		timer_settime(tid, 0, &new, NULL);
	}
}

/*
 * Load the config at path 'conf' into the array 'new'.
 */
void
load_config(const char *conf, struct ttyd_child *new)
{
	FILE *fconf = fopen(conf, "r");
	if (fconf == NULL) {
		slog(LOG_WARNING, "fopen() config failed: %m");
		return;
	}

	char *line = NULL;
	size_t linelen;

	// current line number
	int lineno = 0;
	// current index into 'new' array
	int gettyno = 0;

	slog(LOG_DEBUG, "begin processing file");

	// note: line is terminated by '\n'
	while (getline(&line, &linelen, fconf) != -1) {
		lineno++;

		slog(LOG_DEBUG, "=> line %d", lineno);

		// skip comments, empty lines, and lines with leading whitespace
		if ((*line == '#') || isspace(*line)) {
			slog(LOG_DEBUG, "skipping comment/blank");
			goto cleanup;
		}

		int r;
		int n;

		r = sscanf(line, "%" STRFY(TTYD_DEV_LENX) "s%n", new[gettyno].tty, &n);
		if (r != 1) {
			// invalid; no strings on that line
			slog(LOG_WARNING, "config line %d invalid; no tty", lineno);
			goto cleanup;
		}

		/*
		 * Rest of line is getty;
		 * convert into argv to exec later.
		 */

		char *p = line + n;

		for (int i=0; i<TTYD_GARGV_LEN; i++) {
			// consume leading whitespace, then get the arg
			r = sscanf(p, "%*[ \t]%" STRFY(TTYD_GARGV_ARGLENX) "s%n",
			    new[gettyno].gargv[i], &n);
			if (r != 1) {
				// no string detected
				if (i == 0) {
					slog(LOG_WARNING,
					    "config line %d invalid; no getty process", lineno);
					memset(&new[gettyno], 0, sizeof(struct ttyd_child));
					goto cleanup;
				} else {
					break;
				}
			}
			p += n;
		}

		gettyno++;
		if (gettyno >= TTYD_CHILD_MAX) {
			slog(LOG_WARNING, "hit TTYD_CHILD_MAX (%d), stopping", TTYD_CHILD_MAX);
		}

cleanup:
		free(line);
		line = NULL;
		linelen = 0;
	}
	// must free even when getline() has failed
	free(line);
	line = NULL;
	linelen = 0;

	if (feof(fconf)) {
		slog(LOG_DEBUG, "reached EOF");
	} else if (ferror(fconf)) {
		slog(LOG_DEBUG, "error reading fconf");
		// error
	}

	fclose(fconf);
}

void
usage()
{
	errx(64, "usage");
}
