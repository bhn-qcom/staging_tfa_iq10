/*
 * Copyright (c) 2026, Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Public interface for the QTI TF-A clock driver: clock-group bring-up and
 * teardown for the boot flow.
 */

#ifndef CLOCK_H
#define CLOCK_H

/* Logical groups of related clocks. */
enum clock_group_type {
	CLOCK_GROUP_ABT,
	CLOCK_GROUP_BUS,
	CLOCK_GROUP_INIT,
	CLOCK_GROUP_QDSS,
	CLOCK_GROUP_EUD,
	CLOCK_GROUP_INIT_SSC,
	CLOCK_GROUP_INIT_GPU,
	CLOCK_GROUP_INIT_CAMERA,
	CLOCK_GROUP_INIT_DISPLAY,
	CLOCK_GROUP_INIT_AUDIO,
	CLOCK_GROUP_VSENSE_PRIMARY,
	CLOCK_GROUP_VSENSE_SECONDARY,
	CLOCK_GROUP_INIT_VIDEO,
	CLOCK_GROUP_INIT_DISPLAY_1,
	CLOCK_GROUP_INIT_PCIE,
	CLOCK_GROUP_INIT_NSP,
	CLOCK_GROUP_INIT_MODEM,

	CLOCK_GROUP_TOTAL
};

/*
 * Bring up the clocks BL31 needs while initializing, run fn (may be NULL)
 * with them held, then release the ones only needed during init. On return
 * the init-only clocks are off again.
 */
#ifdef QTI_CLOCK_ENABLED
void qti_clock_init(void (*fn)(void));
#else
static inline void qti_clock_init(void (*fn)(void))
{
	if (fn != NULL) {
		fn();
	}
}
#endif

/* Enable all clocks in a group, plus any required power domains. */
int clock_group_enable(enum clock_group_type group);

/* Disable all clocks in a group, plus any required power domains. */
int clock_group_disable(enum clock_group_type group);

#endif /* CLOCK_H */
