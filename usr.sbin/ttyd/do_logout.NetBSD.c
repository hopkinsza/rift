#ifdef USE_UTMP
#include <utmp.h>
#endif
#ifdef USE_UTMPX
#include <utmpx.h>
#endif

#if defined(USE_UTMP) || defined(USE_UTMPX)
#include <util.h>
#endif

void
do_logout(char *line, int status)
{
#ifdef USE_UTMP
	logout(line);
	logwtmp(line, "", "");
#endif

#ifdef USE_UTMPX
	logoutx(line, status, DEAD_PROCESS);
	logwtmpx(line, "", "", status, DEAD_PROCESS);
#endif
}
