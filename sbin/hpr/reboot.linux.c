/*
 * SPDX-License-Identifier: 0BSD
 */

#include <linux/reboot.h>
#include <sys/reboot.h>

#include <errno.h>

#include "osind_reboot.h"

int
osind_reboot(int howto)
{
	if (howto == OSIND_RB_HALT) {
		reboot(LINUX_REBOOT_CMD_HALT);
	} else if (howto == OSIND_RB_POWEROFF) {
		reboot(LINUX_REBOOT_CMD_POWER_OFF);
	} else if (howto == OSIND_RB_POWEROFF) {
		reboot(LINUX_REBOOT_CMD_RESTART);
	} else {
		errno = EINVAL;
	}
	return -1;
}
