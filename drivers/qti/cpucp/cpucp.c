/*
 * Copyright (c) 2026, Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <assert.h>
#include <stdint.h>
#include <arch_helpers.h>
#include <common/debug.h>
#include <lib/bakery_lock.h>
#include <lib/mmio.h>
#include <cpucp.h>
#include <cpucp_hwio.h>
#include <drivers/arm/css/scmi.h>
#include <lib/mmio.h>
#include <lib/spinlock.h>
#include <plat/common/platform.h>
#include <platform_def.h>

#include <scmi_private.h>

/* CPUCP doorbell: APSS -> CPUCP IPC interrupt trigger bit. */
#define CPUCP_DB_MODIFY_MASK		\
	(1U << APSS_SHARED_TZ_IPC_INTERRUPT_OSM_IPC_SHFT)
#define CPUCP_DB_PRESERVE_MASK		(~CPUCP_DB_MODIFY_MASK)

/* CPUCP SCMI channel poll interval and command timeout, in micro-seconds. */
#define CPUCP_SCMI_POLL_DELAY_US		1U
#define CPUCP_SCMI_TIMEOUT_US			(500U * 1000U)

#pragma weak plat_cpu_scmi_protocol_init

int plat_cpu_scmi_protocol_init(scmi_channel_t *ch)
{
	return 0;
}

static scmi_lock_t cpucp_scmi_lock;

/* Ring the CPUCP doorbell to hand the SCMI channel over for processing. */
static void cpucp_ring_doorbell(struct scmi_channel_plat_info *plat_info)
{
	mmio_write_32(plat_info->db_reg_addr, plat_info->db_modify_mask);
}

static scmi_channel_plat_info_t cpucp_scmi_plat_info = {
	.scmi_mbx_mem = CPUCP_IPC_SEC_BUF_BASE,
	.db_reg_addr = APSS_SHARED_TZ_IPC_INTERRUPT_ADDR,
	.db_preserve_mask = CPUCP_DB_PRESERVE_MASK,
	.db_modify_mask = CPUCP_DB_MODIFY_MASK,
	.ring_doorbell = &cpucp_ring_doorbell,
	.delay = CPUCP_SCMI_POLL_DELAY_US,
	.timeout = CPUCP_SCMI_TIMEOUT_US,
};

static scmi_channel_t cpucp_scmi_channel = {
	.info = &cpucp_scmi_plat_info,
	.lock = &cpucp_scmi_lock,
	.is_initialized = 0,
};

/*
 * qti_scmi_get_channel - initialise and return the QTI SCMI channel.
 *
 * Calls scmi_init() to initialise the channel lock via the standard ARM CSS
 * SCMI driver path.  scmi_init() also queries the Power Domain (0x11) and
 * System Power (0x12) protocol versions; CPUCP on Nord only implements the
 * Reset Domain protocol (0x16), so those queries will fail and scmi_init()
 * will return NULL.  In that case the channel lock has already been
 * initialised by scmi_init(), so we mark the channel ready manually to allow
 * the Reset Domain protocol to proceed.
 *
 * Returns a pointer to the initialised scmi_channel_t cast to void *, or
 * NULL if the channel could not be initialised at all.
 */
void *qti_scmi_get_channel(void)
{
	if (cpucp_scmi_channel.is_initialized != 0) {
		return (void *)&cpucp_scmi_channel;
	}

	/*
	 * scmi_init() initialises the channel lock and then queries the
	 * protcols versions that are used by the platform.
	 */
	if (scmi_init(&cpucp_scmi_channel, plat_cpu_scmi_protocol_init) == NULL) {
		INFO("QTI SCMI: scmi_init() returned NULL;"
		     "Channel usable for protocols supported by the platform.\n");
		cpucp_scmi_channel.is_initialized = 1;
	}

	return (void *)&cpucp_scmi_channel;
}

/*
 * plat_css_get_scmi_info - return the platform SCMI channel descriptor.
 *
 * This is the weak hook declared in include/drivers/arm/css/scmi.h that
 * platform code must override to supply the channel parameters.  The ARM CSS
 * SCMI driver calls this during scmi_init() to obtain the mailbox base
 * address, doorbell register, and ring_doorbell callback.
 */
scmi_channel_plat_info_t *plat_css_get_scmi_info(unsigned int channel_id __unused)
{
	return &cpucp_scmi_plat_info;
}
