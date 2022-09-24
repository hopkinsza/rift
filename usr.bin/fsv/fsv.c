#include <sys/file.h> // for flock(2) on linux
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
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

#include "extern.h"
#include "slog/slog.h"

int fork_chld(int, struct fsv_child *, int[], char *[], long);
long str_to_l(const char *);
void termprocs(struct fsv_child[]);
void usage();
void write_info(int fd, struct fsv_parent *fsv, struct fsv_child chld[]);

// declare externs
int fd_devnull;
sigset_t bmask;

int
main(int argc, char *argv[])
{
	/*
	 * Initialize externs.
	 */

	fd_devnull = open("/dev/null", O_RDWR);
	if (fd_devnull == -1)
		err(1, "open(/dev/null) failed");

	sigemptyset(&bmask);
	sigaddset(&bmask, SIGCHLD);
	sigaddset(&bmask, SIGUSR1);
	sigaddset(&bmask, SIGUSR2);
	sigaddset(&bmask, SIGINT);
	sigaddset(&bmask, SIGHUP);
	sigaddset(&bmask, SIGTERM);

	slog_open();
	slog_upto(LOG_INFO);

	/*
	 * Initialize structs.
	 */

	struct fsv_parent fsv;
	memset(&fsv, 0, sizeof(fsv));
	// fsv's since value is the only one recorded with CLOCK_REALTIME
	clock_gettime(CLOCK_REALTIME, &fsv.since);

	struct fsv_child chld[2];

	for (int i=0; i<2; i++) {
		memset(&chld[i], 0, sizeof(chld[i]));
		chld[i].pid = -1;
		chld[i].recent_secs = 3600;
		chld[i].max_recent_execs = 3;
	}

	/*
	 * Initialize fsvdir name, to chdir() later.
	 */

	char fsvdir[32];
	{
		// build string: fsv-$euid
		int l;

		l = snprintf(fsvdir, sizeof(fsvdir), "fsv-%ld", (long)geteuid());
		if (l == -1)
			err(1, "snprintf");
		else if (l >= sizeof(fsvdir))
			errx(1, "snprintf: fsvdir too long");
	}

	/*
	 * Process flags.
	 */

	int do_daemon = 0;
	int do_status = 0;

	char *cmdname = NULL;

	// -1 means we are not logging at all
	long out_mask = -1;

	const char *getopt_str = "+Bbdhl:M:m:n:o:p:R:r:s:t:Vv:XxYy";

	struct option longopts[] = {
		{ "background",	no_argument,		NULL,	'b' },
		{ "daemon",	no_argument,		NULL,	'b' },
		{ "debug",	no_argument,		NULL,	'd' },
		{ "help",	no_argument,		NULL,	'h' },
		{ "log",	required_argument,	NULL,	'l' },
		{ "max-execs-log",required_argument,	NULL,	'M' },
		{ "max-execs",	required_argument,	NULL,	'm' },
		{ "name",	required_argument,	NULL,	'n' },
		{ "output-mask",required_argument,	NULL,	'o' },
		{ "pids",	required_argument,	NULL,	'p' },
		{ "recent-secs-log",required_argument,	NULL,	'R' },
		{ "recent-secs",required_argument,	NULL,	'r' },
		{ "status",	required_argument,	NULL,	's' },
		{ "timeout",	required_argument,	NULL,	't' },
		{ "version",	no_argument,		NULL,	'V' },
		{ "loglevel",	required_argument,	NULL,	'v' },
		{ "no-stderr",	no_argument,		NULL,	'X' },
		{ "stderr",	no_argument,		NULL,	'x' },
		{ "no-syslog",	no_argument,		NULL,	'Y' },
		{ "syslog",	no_argument,		NULL,	'y' },
		{ NULL,		0,			NULL,	0 }
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
			slog_do_stderr(0);
			slog_do_syslog(1);
			break;
		case 'd':
			slog_upto(LOG_DEBUG);
			slog(LOG_DEBUG, "debugging on");
			break;
		case 'h':
			usage();
			exit(0);
			break;
		case 'l':
			if (out_mask == -1)
				out_mask = 3;

			// parse into an argv; gnarly
			if (logstring != NULL)
				errx(64, "-l specified more than once");
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

				if (i == 32)
					errx(1, "too many words in -l arg");

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
						errx(64, "unmatched double-quote in -l arg");
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
			cmdname = optarg;
			break;
		case 'o':
			// this is not settable to -1 with str_to_l
			out_mask = str_to_l(optarg);
			if (out_mask < 0 || out_mask > 3)
				errx(64, "-m arg must be in range 0-3");
			break;
		case 'p':
			cmdname = optarg;
			do_status = 'p';
			break;
		case 'R':
			chld[1].recent_secs = str_to_l(optarg);
			break;
		case 'r':
			chld[0].recent_secs = str_to_l(optarg);
			break;
		case 's':
			cmdname = optarg;
			do_status = 's';
			break;
		case 't':
			fsv.timeout = str_to_l(optarg);
			break;
		case 'V':
			printf("fsv %s\n", FSV_VERSION);
			exit(0);
			break;
		case 'v':
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
		case 'X':
			slog_do_stderr(0);
			break;
		case 'x':
			slog_do_stderr(1);
			break;
		case 'Y':
			slog_do_syslog(0);
			break;
		case 'y':
			slog_do_syslog(1);
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
	 * If we are just printing status, do so.
	 */

	if (do_status != 0) {
		int fd_info;
		struct allinfo ai;

		if (chdir(FSV_CMDDIR_PREFIX) == -1)
			err(1, "chdir(%s) failed", FSV_CMDDIR_PREFIX);
		if (chdir(fsvdir) == -1)
			err(1, "chdir(%s) failed", fsvdir);
		if (chdir(cmdname) == -1)
			err(1, "chdir(%s) failed", cmdname);

		fd_info = open("info.struct", O_RDONLY);
		if (fd_info == -1)
			err(1, "open(info.struct) failed");

		ssize_t r;
		r = read(fd_info, &ai, sizeof(ai));
		if (r == -1)
			err(1, "read from info.struct failed");
		else if (r < sizeof(ai))
			errx(1, "unexpected data in info.struct");

		if (do_status == 'p') {
			printf("%ld\n", (long)ai.fsv.pid);
			printf("%ld\n", (long)ai.chld[0].pid);
			printf("%ld\n", (long)ai.chld[1].pid);
		} else if (do_status == 's') {
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

		if (ai.fsv.pid > 0)
			exit(0);
		else
			exit(1);
	}

	/*
	 * Verify that we have a command to run.
	 * Find cmdname if not already set by -n.
	 */

	if (argc == 0) {
		warnx("no cmd to execute");
		usage();
		exit(64);
	}

	if (cmdname == NULL) {
		// basename(3) may modify its argument, so use a copy of argv[0]
		char *tmp = malloc(strlen(argv[0]) + 1);
		strcpy(tmp, argv[0]);

		// have room for an extra character
		// because basename("") can return "."
		cmdname = malloc(strlen(tmp) + 2);
		strcpy(cmdname, basename(tmp));
	}

	if (*cmdname == '.' || *cmdname == '/') {
		errx(64, "cmdname does not make sense");
	}

	/*
	 * chdir() to the directory.
	 */

	// cd to FSV_CMDDIR_PREFIX
	if (chdir(FSV_CMDDIR_PREFIX) == -1)
		err(1, "chdir(%s) failed", FSV_CMDDIR_PREFIX);

	// cd to fsv-$euid
	{
		// check if it exists
		int exists = 1;
		struct stat st;
		if (stat(fsvdir, &st) == -1) {
			if (errno == ENOENT) {
				exists = 0;
			} else {
				err(1, "stat");
			}
		}

		if (exists) {
			// verify the owner;
			// be paranoid because this is a tmp-like directory
			if (st.st_uid != geteuid())
				errx(1, "unexpected owner of %s", fsvdir);
		} else {
			// create, failing if exists
			if (mkdir(fsvdir, 00755) == -1)
				err(1, "mkdir(%s) failed", fsvdir);
		}

		if (chdir(fsvdir) == -1)
			err(1, "chdir(%s) failed", fsvdir);
	}

	// cd to $cmdname
	if (mkdir(cmdname, 00755) == -1) {
		if (errno == EEXIST) {
			// ok
		} else {
			err(1, "mkdir(%s) failed", cmdname);
		}
	}
	if (chdir(cmdname) == -1)
		err(1, "chdir(%s) failed", cmdname);

	/*
	 * Create logging pipe.
	 * Open some other file descriptors.
	 */

	int logpipe[2];
	if (pipe(logpipe) == -1)
		err(1, "pipe() failed");

	// open and flock info.struct
	int fd_info;
	fd_info = open("info.struct", O_CREAT|O_RDWR, 00644);
	if (fd_info == -1)
		err(1, "open(info.struct) failed");

	if (flock(fd_info, LOCK_EX|LOCK_NB) == -1)
		err(1, "flock(info.struct) failed (already running?)");

	/*
	 * Daemonize if needed.
	 * Now we can set CLOEXEC on fd_info.
	 *
	 * fd_devnull and the logpipe descriptors do NOT have cloexec set;
	 * the child needs them to set its STD{IN,OUT,ERR} and will have to
	 * close them itself.
	 */

	if (do_daemon == 1) {
		if (daemon(0, 0) == -1)
			err(1, "daemon() failed");
	} else if (do_daemon == 2) {
		if (daemon(0, 1) == -1)
			err(1, "daemon() failed");
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

		if (timer_create(CLOCK_MONOTONIC, &sev, &cmd_tid) == -1)
			err(1, "timer_create() failed");
	}

	// USR2
	timer_t log_tid;
	struct itimerspec log_itspec = { {0,0}, {0,200000000}};
	{
		struct sigevent sev;
		sev.sigev_notify = SIGEV_SIGNAL;
		sev.sigev_signo = SIGUSR2;

		if (timer_create(CLOCK_MONOTONIC, &sev, &log_tid) == -1)
			err(1, "timer_create() failed");
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
		warnx("string to number conversion failed for `%s': "
		      "not a number, or improper format", str);
		exit(64);
	}
	if (*ep != '\0') {
		warnx("string to number conversion failed for `%s': "
		      "trailing junk `%s'", str, ep);
		exit(64);
	}
	if (errno != 0) {
		warnx("string to number conversion failed for `%s': "
		      "number out of range", str);
		exit(64);
	}
	if (val < 0) {
		warnx("string to number conversion failed for `%s': "
		      "should be positive", str);
		exit(64);
	}

	return val;
}

//pid_t
//fork_log(struct fsv_child *log, int logpipe[], char *argv[])
//{
//	log->total_execs++;
//	gettimeofday(&log->tv, NULL);
//	//TODO
//}

// int
// fork_rc()
// {
// 	int pid;
// 	if ((pid = fork()) == 0) {
// 		sigprocmask(SIG_UNBLOCK, &bmask, NULL);
// 		execl("/bin/rc", "rc", NULL);
// 	}
// 	return pid;
// }

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
		warn("write into info.struct failed");
		// no way to report the error outside of debugging mode
	}
}
