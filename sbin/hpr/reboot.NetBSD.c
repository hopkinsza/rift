/*
 * SPDX-License-Identifier: 0BSD
 */

#include <sys/reboot.h>

#include <errno.h>
#include <unistd.h>

#include "osind_reboot.h"

int
osind_reboot(int howto)
{
	if (howto == OSIND_RB_HALT) {
		reboot(RB_HALT, NULL);
	} else if (howto == OSIND_RB_POWEROFF) {
		reboot(RB_HALT|RB_POWERDOWN, NULL);
	} else if (howto == OSIND_RB_REBOOT) {
		reboot(RB_AUTOBOOT, NULL);
	} else {
		errno = EINVAL;
	}
	return -1;
}
