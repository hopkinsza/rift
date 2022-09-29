/*
 * USE_UTMPX is a no-op for this file, because utmp(5) says:
 * "Linux defines the utmpx structure to be the same as the utmp structure"
 */

#ifdef USE_UTMP
#include <utmp.h>
#endif

void
record_logout(char *line, int status)
{
#ifdef USE_UTMP
	logout(line);
	logwtmp(line, "", "");
#endif
}
