#include <sys/file.h> // for flock(2) on linux
#include <sys/stat.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h> // for LOG_* level constants
#include <time.h>
#include <unistd.h>

#include <slog.h>

#include "extern.h"

int fork_chld(int, struct fsv_child *, int[], char *[], long);
long str_to_l(const char *);
void termprocs(struct fsv_child[]);
__dead void usage();
void write_info(int fd, struct fsv_parent *fsv, struct fsv_child chld[]);

// define externs
int fd_devnull = -1;
sigset_t bmask;

int
main(int argc, char *argv[])
{
	slog_open(NULL, LOG_PID|LOG_PERROR|LOG_NLOG, LOG_DAEMON);
	slog_upto(LOG_INFO);

	/*
	 * Declare and initialize process structs.
	 */

	struct fsv_parent fsv;
	struct fsv_child chld[2];

	memset(&fsv, 0, sizeof(fsv));
	// fsv's since value is the only one recorded with CLOCK_REALTIME
	clock_gettime(CLOCK_REALTIME, &fsv.since);

	for (int i=0; i<2; i++) {
		memset(&chld[i], 0, sizeof(chld[i]));
		chld[i].pid = -1;
		chld[i].recent_secs = 3600;
		chld[i].max_recent_execs = 3;
	}

	/*
	 * Process flags.
	 */

	char *name = NULL;

	int do_daemon = 0;
	int do_status = 0;

	// -1 means we are not logging at all
	long out_mask = -1;

	uid_t status_uid = -1;

	const char *getopt_str = "+BbdhL:l:M:m:n:o:p:R:r:S:s:t:u:VYy";

	struct option longopts[] = {
		{ "background",		no_argument,		NULL,	'b' },
		{ "daemon",		no_argument,		NULL,	'b' },
		{ "debug",		no_argument,		NULL,	'd' },
		{ "help",		no_argument,		NULL,	'h' },
		{ "loglevel",		required_argument,	NULL,	'L' },
		{ "log",		required_argument,	NULL,	'l' },
		{ "max-execs-log",	required_argument,	NULL,	'M' },
		{ "max-execs",		required_argument,	NULL,	'm' },
		{ "name",		required_argument,	NULL,	'n' },
		{ "output-mask",	required_argument,	NULL,	'o' },
		{ "pids",		required_argument,	NULL,	'p' },
		{ "recent-secs-log",	required_argument,	NULL,	'R' },
		{ "recent-secs",	required_argument,	NULL,	'r' },
		{ "status-exit",	required_argument,	NULL,	'S' },
		{ "status",		required_argument,	NULL,	's' },
		{ "timeout",		required_argument,	NULL,	't' },
		{ "uid",		required_argument,	NULL,	'u' },
		{ "version",		no_argument,		NULL,	'V' },
		{ "syslog-only",	no_argument,		NULL,	'Y' },
		{ "syslog",		no_argument,		NULL,	'y' },
		{ NULL,			0,			NULL,	0 }
	};

	int ch;
	char *logstring = NULL;
	char *largv[32];
	while ((ch = getopt_long(argc, argv, getopt_str, longopts, NULL)) != -1) {
		switch(ch) {
		case 'B':
			do_daemon = 2;
			break;
		case 'b':
			do_daemon = 1;
			slog_open(NULL, LOG_PID, LOG_DAEMON);
			break;
		case 'd':
			slog_upto(LOG_DEBUG);
			slog(LOG_DEBUG, "debugging on");
			break;
		case 'h':
			usage();
			exit(0);
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
			else {
				slog(LOG_ERR, "unrecognized loglevel for -L");
				usage();
			}
			break;
		case 'l':
			if (out_mask == -1)
				out_mask = 3;

			// parse into an argv; gnarly
			if (logstring != NULL) {
				slog(LOG_ERR, "-l specified more than once");
				usage();
			}
			for (int i=0; i<32; i++)
				largv[i] = NULL;

			logstring = malloc(strlen(optarg) + 1);
			strcpy(logstring, optarg);

			int i = 0;
			char *c = logstring;

			largv[0] = c;

			while (1) {
				// consume any leading spaces
				while (*c == ' ') {
					*c = '\0';
					c++;
				}

				// end of string
				if (*c == '\0')
					break;

				if (i == 32) {
					slog(LOG_ERR, "too many words in -l arg");
					exit(1);
				}

				if (*c != '"') {
					// normal argument
					largv[i] = c;
					while (*c != ' ' && *c != '\0')
						c++;
				} else {
					// double-quoted argument
					*c = '\0';
					c++;
					largv[i] = c;

					while (*c != '"' && *c != '\0')
						c++;

					if (*c == '\0') {
						slog(LOG_ERR, "unmatched double-quote in -l arg");
						usage();
					} else {
						*c = '\0';
						c++;
					}
				}

				i++;
			}

			for (int i=0; i<32; i++) {
				if (largv[i] == NULL)
					break;
				slog(LOG_DEBUG, "largv: %s", largv[i]);
			}
			break;
		case 'M':
			chld[1].max_recent_execs = str_to_l(optarg);
			break;
		case 'm':
			chld[0].max_recent_execs = str_to_l(optarg);
			break;
		case 'n':
			name = optarg;
			break;
		case 'o':
			// this is not settable to -1 with str_to_l
			out_mask = str_to_l(optarg);
			if (out_mask < 0 || out_mask > 3) {
				slog(LOG_ERR, "-m arg must be in range 0-3");
				usage();
			}
			break;
		case 'p':
			name = optarg;
			do_status = 'p';
			break;
		case 'R':
			chld[1].recent_secs = str_to_l(optarg);
			break;
		case 'r':
			chld[0].recent_secs = str_to_l(optarg);
			break;
		case 'S':
			name = optarg;
			do_status = 'S';
			break;
		case 's':
			name = optarg;
			do_status = 's';
			break;
		case 't':
			fsv.timeout = str_to_l(optarg);
			break;
		case 'u':
			status_uid = (uid_t)str_to_l(optarg);
			break;
		case 'V':
			printf("fsv %s\n", FSV_VERSION);
			exit(0);
			break;
		case 'Y':
			slog_open(NULL, LOG_PID, LOG_DAEMON);
			break;
		case 'y':
			slog_open(NULL, LOG_PID|LOG_PERROR, LOG_DAEMON);
			break;
		case '?':
		default:
			usage();
			exit(1);
		}
	}
	argc -= optind;
	argv += optind;

	slog(LOG_DEBUG, "finished processing options");

	/*
	 * Set externs.
	 */

	fd_devnull = open("/dev/null", O_RDWR);
	if (fd_devnull == -1) {
		slog(LOG_ERR, "open(/dev/null) failed: %m");
		exit(1);
	}

	sigemptyset(&bmask);
	sigaddset(&bmask, SIGCHLD);
	sigaddset(&bmask, SIGUSR1);
	sigaddset(&bmask, SIGUSR2);
	sigaddset(&bmask, SIGINT);
	sigaddset(&bmask, SIGHUP);
	sigaddset(&bmask, SIGTERM);

	/*
	 * Declare/init fsvdir, to chdir() later.
	 */

	char fsvdir[32];
	{
		// build string: fsv-$euid
		int l;

		l = snprintf(fsvdir, sizeof(fsvdir), "fsv-%ld", (long)geteuid());
		if (l == -1) {
			slog(LOG_ERR, "snprintf: %m");
			exit(1);
		} else if (l >= sizeof(fsvdir)) {
			slog(LOG_ERR, "snprintf: fsvdir too long");
			exit(1);
		}
	}

	/*
	 * If we are just printing status, do so.
	 * Note that this will exit(3).
	 */

	if (do_status != 0) {
		if (status_uid == -1)
			status_uid = geteuid();

		status(do_status, status_uid, name);
	}

	/*
	 * Verify that we have a command to run.
	 * Find name if not already set by -n.
	 */

	if (argc == 0) {
		slog(LOG_ERR, "no cmd to execute");
		usage();
	}

	if (name == NULL) {
		// basename(3) may modify its argument, so use a copy of argv[0]
		char *tmp = malloc(strlen(argv[0]) + 1);
		strcpy(tmp, argv[0]);

		// have room for an extra character
		// because basename("") can return "."
		name = malloc(strlen(tmp) + 2);
		strcpy(name, basename(tmp));
	}

	if (*name == '.' || *name == '/') {
		slog(LOG_ERR, "name does not make sense");
		usage();
	}

	/*
	 * chdir() to the directory.
	 */

	// cd to FSV_STATE_PREFIX
	if (chdir(FSV_STATE_PREFIX) == -1) {
		slog(LOG_ERR, "chdir(%s) failed: %m", FSV_STATE_PREFIX);
		exit(1);
	}

	// cd to fsv-$euid
	{
		// check if it exists
		int exists = 1;
		struct stat st;
		if (stat(fsvdir, &st) == -1) {
			if (errno == ENOENT) {
				exists = 0;
			} else {
				slog(LOG_ERR, "stat: %m");
				exit(1);
			}
		}

		if (exists) {
			// verify the owner;
			// be paranoid because this is a tmp-like directory
			if (st.st_uid != geteuid()) {
				slog(LOG_ERR, "unexpected owner of %s", fsvdir);
				exit(1);
			}
		} else {
			// create, failing if exists
			if (mkdir(fsvdir, 00755) == -1) {
				slog(LOG_ERR, "mkdir(%s) failed: %m", fsvdir);
				exit(1);
			}
		}

		if (chdir(fsvdir) == -1) {
			slog(LOG_ERR, "chdir(%s) failed: %m", fsvdir);
			exit(1);
		}
	}

	// cd to $name
	if (mkdir(name, 00755) == -1) {
		if (errno == EEXIST) {
			// ok
		} else {
			slog(LOG_ERR, "mkdir(%s) failed: %m", name);
			exit(1);
		}
	}
	if (chdir(name) == -1) {
		slog(LOG_ERR, "chdir(%s) failed: %m", name);
		exit(1);
	}

	/*
	 * Create logging pipe.
	 * Open some other file descriptors.
	 */

	int logpipe[2];
	if (pipe(logpipe) == -1) {
		slog(LOG_ERR, "pipe() failed: %m");
		exit(1);
	}

	// open and flock(2) the lockfile
	int fd_lock;
	fd_lock = open("lock", O_CREAT|O_RDWR, 00600);
	if (fd_lock == -1) {
		slog(LOG_ERR, "open(lock) failed: %m");
		exit(1);
	}
	if (flock(fd_lock, LOCK_EX|LOCK_NB) == -1) {
		slog(LOG_ERR, "flock(lock) failed (already running?): %m");
		exit(1);
	}

	// open info.struct
	int fd_info;
	fd_info = open("info.struct", O_CREAT|O_RDWR, 00644);
	if (fd_info == -1) {
		slog(LOG_ERR, "open(info.struct) failed: %m");
		exit(1);
	}

	/*
	 * Daemonize if needed.
	 * Now we can set CLOEXEC on fd_info.
	 *
	 * fd_devnull and the logpipe descriptors do NOT have cloexec set;
	 * the child needs them to set its STD{IN,OUT,ERR} and will have to
	 * close them itself.
	 */

	if (do_daemon == 1) {
		if (daemon(0, 0) == -1) {
			slog(LOG_ERR, "daemon() failed: %m");
			exit(1);
		}
	} else if (do_daemon == 2) {
		if (daemon(0, 1) == -1) {
			slog(LOG_ERR, "daemon() failed: %m");
			exit(1);
		}
	}

	// set my pid now, because the fork changes it
	fsv.pid = getpid();

	fcntl(fd_info, F_SETFD, FD_CLOEXEC);

	/*
	 * Block signals and handle in the main loop.
	 */

	sigprocmask(SIG_BLOCK, &bmask, NULL);

	/*
	 * Initialize the timers.
	 * cmd_tmout_itspec is the main one. It is used for the cmd "timeout".
	 * cmd_itspec and log_itspec are only used if the fork() fails,
	 * to wait 0.2 seconds before trying again.
	 */

	// queue a USR1
	timer_t cmd_tid;
	struct itimerspec cmd_itspec = { {0,0}, {0,200000000}};
	struct itimerspec cmd_tmout_itspec = { {0,0}, {fsv.timeout,0}};
	{
		struct sigevent sev;
		sev.sigev_notify = SIGEV_SIGNAL;
		sev.sigev_signo = SIGUSR1;

		if (timer_create(CLOCK_MONOTONIC, &sev, &cmd_tid) == -1) {
			slog(LOG_ERR, "timer_create() failed: %m");
			exit(1);
		}
	}

	// USR2
	timer_t log_tid;
	struct itimerspec log_itspec = { {0,0}, {0,200000000}};
	{
		struct sigevent sev;
		sev.sigev_notify = SIGEV_SIGNAL;
		sev.sigev_signo = SIGUSR2;

		if (timer_create(CLOCK_MONOTONIC, &sev, &log_tid) == -1) {
			slog(LOG_ERR, "timer_create() failed: %m");
			exit(1);
		}
	}

	/*
	 * Start cmd and log for the first time.
	 */

	raise(SIGUSR1);
	raise(SIGUSR2);

	/*
	 * Main loop.
	 */

	slog(LOG_DEBUG, "begin main loop");

	int sig;
	while (sigwait(&bmask, &sig) == 0) switch (sig) {
	case SIGCHLD:
		slog(LOG_DEBUG, "> SIGCHLD");

		int status;
		pid_t epid;

		// wait() on all terminated children
		while ((epid = waitpid(-1, &status, WNOHANG)) > 0) {
			if (epid != chld[0].pid && epid != chld[1].pid) {
				slog(LOG_DEBUG, "??? unknown child!");
			}

			for (int i=0; i<2; i++) if (epid == chld[i].pid) {
				// cmd or log has exited
				chld[i].pid = 0;

				char buf[32];
				if (WIFEXITED(status)) {
					snprintf(buf, sizeof(buf),
					    "exited %d", WEXITSTATUS(status));
				} else if (WIFSIGNALED(status)) {
					snprintf(buf, sizeof(buf),
					    "terminated by signal %d", WTERMSIG(status));
				}

				if (i == 0) {
					slog(LOG_NOTICE, "cmd process %s", buf);
					raise(SIGUSR1);
				} else if (i == 1) {
					slog(LOG_NOTICE, "log process %s", buf);
					raise(SIGUSR2);
				}
			}
		}
		break;
	case SIGUSR1:
	case SIGUSR2:
	{ // create a scope so we can declare variables
		// USR1 is to start up the cmd process;
		// USR2 is for log
		int n;
		const char *cname;
		if (sig == SIGUSR1) {
			slog(LOG_DEBUG, "> SIGUSR1");
			n = 0;
			cname = "cmd";
		} else if (sig == SIGUSR2) {
			slog(LOG_DEBUG, "> SIGUSR2");
			n = 1;
			cname = "log";
			if (out_mask == -1) {
				slog(LOG_DEBUG, "but out_mask is -1, ignore");
				break;
			}
		}

		if (chld[n].pid > 0) {
			slog(LOG_DEBUG, "but the process is already running");
			break;
		}

		struct timespec now;
		clock_gettime(CLOCK_MONOTONIC, &now);

		// set recent_execs
		if (chld[n].recent_secs == 0 ||
		    ((now.tv_sec - chld[n].since.tv_sec) <= chld[n].recent_secs)) {
			chld[n].recent_execs++;
		} else {
			chld[n].recent_execs = 1;
		}

		// check if limit has been exceeded
		if (chld[n].recent_execs > chld[n].max_recent_execs) {
			// exit or timeout
			if (fsv.timeout == 0 || n == 1) {
				slog(LOG_WARNING, "max_recent_execs exceeded for %s, exiting",
				     cname);
				termprocs(chld);
				fsv.pid = 0;
				fsv.gaveup = 1;
				write_info(fd_info, &fsv, chld);
				exit(0);
			} else {
				slog(LOG_WARNING,
				    "max_recent_execs exceeded for %s, timeout for %ld secs",
				    cname, fsv.timeout);
				timer_settime(cmd_tid, 0, &cmd_tmout_itspec, NULL);
				write_info(fd_info, &fsv, chld);
				break;
			}
		}

		// exec
		int r;
		if (n == 0) {
			r = fork_chld(n, &chld[n], logpipe, argv, out_mask);
			if (r == -1)
				timer_settime(cmd_tid, 0, &cmd_itspec, NULL);
		} else if (n == 1) {
			r = fork_chld(n, &chld[n], logpipe, largv, out_mask);
			if (r == -1)
				timer_settime(log_tid, 0, &log_itspec, NULL);
		}
		write_info(fd_info, &fsv, chld);
		break;
	}
	case SIGINT:
	case SIGHUP:
	case SIGTERM:
		slog(LOG_DEBUG, "> INT, HUP, or TERM");
		termprocs(chld);
		fsv.pid = 0;
		write_info(fd_info, &fsv, chld);
		exit(0);
		break;
	}
}

/*
 * Fork a child and exec the given argv.
 * Arg 'log': 0 for cmd process, 1 for log process
 * (for the appropriate file descriptor redirections).
 * Arg 'cmd': pointer to the appropriate struct fsv_child to update its values.
 */
int
fork_chld(int log, struct fsv_child *fc, int logpipe[], char *argv[],
          long out_mask)
{
	pid_t pid;
	int fd0, fd1, fd2;

	if (log == 1 && out_mask == -1)
		return 0;

	fc->total_execs++;
	clock_gettime(CLOCK_MONOTONIC, &fc->since);

	// set up fds
	fd0 = fd1 = fd2 = fd_devnull;
	if (log == 0) {
		switch (out_mask) {
		case 0:
			// no output is sent to log process;
			// questionably useful, but allowed
			break;
		case 1:
			fd1 = logpipe[1];
			break;
		case 2:
			fd2 = logpipe[1];
			break;
		case 3:
			fd1 = logpipe[1];
			fd2 = logpipe[1];
			break;
		}
	} else if (log == 1) {
		fd0 = logpipe[0];
	}

	// now fork
	pid = fork();
	if (pid == 0) {
		// set up new fds
		dup2(fd0, 0);
		dup2(fd1, 1);
		dup2(fd2, 2);
		close(fd_devnull);
		close(logpipe[0]);
		close(logpipe[1]);

		slog_close();

		// unblock signals
		sigprocmask(SIG_UNBLOCK, &bmask, NULL);

		execvp(argv[0], argv);

		// This runs only if the exec failed.
		// <sysexits.h> EX_USAGE was chosen because it is a permanent
		// failure that will never be fixed by simply re-execing anyway.
		exit(64);
	}

	if (pid == -1) {
		fc->pid = 0;
		return -1;
	} else {
		fc->pid = pid;
		return 0;
	}
}

// strtol(3) with errors;
// only allow positive numbers
long
str_to_l(const char *str)
{
	char *ep;
	long val;

	errno = 0;
	val = strtol(str, &ep, 0);

	if (ep == str) {
		slog(LOG_ERR, "string to number conversion failed for `%s': "
		      "not a number, or improper format", str);
		usage();
	}
	if (*ep != '\0') {
		slog(LOG_ERR, "string to number conversion failed for `%s': "
		      "trailing junk `%s'", str, ep);
		usage();
	}
	if (errno != 0) {
		slog(LOG_ERR, "string to number conversion failed for `%s': "
		      "number out of range", str);
		usage();
	}
	if (val < 0) {
		slog(LOG_ERR, "string to number conversion failed for `%s': "
		      "should be positive", str);
		usage();
	}

	return val;
}

void
termprocs(struct fsv_child chld[])
{
	if (chld[0].pid > 0) {
		kill(chld[0].pid, SIGTERM);
		kill(chld[0].pid, SIGCONT);
	}
	if (chld[1].pid > 0) {
		kill(chld[1].pid, SIGTERM);
		kill(chld[1].pid, SIGCONT);
	}

	chld[0].pid = 0;
	chld[1].pid = 0;
}

void
usage()
{
	fprintf(stderr, "usage: fsv [options] <cmd>\n");
	fprintf(stderr, "Type 'man 1 fsv' for the manual.\n");
	exit(64);
}

void
write_info(int fd, struct fsv_parent *fsv, struct fsv_child chld[])
{
	struct allinfo ai;
	ai.fsv = *fsv;
	ai.chld[0] = chld[0];
	ai.chld[1] = chld[1];

	size_t size = sizeof(ai);
	ssize_t written;

	// the fd is long-lived, so lseek(2) to the beginning;
	// should never fail in this circumstance
	lseek(fd, 0, SEEK_SET);

	written = write(fd, &ai, size);
	if (written == -1 || written != size) {
		slog(LOG_WARNING, "write into info.struct failed: %m");
	}
}
