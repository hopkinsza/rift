/*
 * SPDX-License-Identifier: 0BSD
 */

#ifndef _OSIND_REBOOT_H_
#define _OSIND_REBOOT_H_

#define OSIND_RB_HALT     0
#define OSIND_RB_POWEROFF 1
#define OSIND_RB_REBOOT   2

/*
 * One function, an OS independent wrapper around reboot(2).
 * If successful, do not return. Otherwise, return -1 with errno set
 * appropriately.
 */

int osind_reboot(int);

#endif
