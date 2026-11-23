/*
 * Copyright (c) 2026, Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdint.h>
#include <arch_helpers.h>
#include <lib/mmio.h>
#include <common/debug.h>
#include <qti_plat.h>
#include <drivers/arm/css/scmi.h>
#include "cpucp_shared.h"

/*
 * Reset state encoding for the NCC CL4 workaround:
 *   Bit 31 = IMPL_RESET (implementation-defined reset)
 *   Bits 30:0 = CL4 (architecture-specific sleep state 4)
 */
#define NCC_RESET_STATE_CL4	(SCMI_RESET_STATE_IMPL | 0x4U)

void *qti_scmi_get_channel(void);
/*
 * qti_ncc_cpu_reset_sync - Apply the NCC boot-core CL4 sleep-state reset
 * workaround via the ARM SCMI Reset Domain Management protocol.
 *
 * Context: single-core, called from bl31_early_platform_setup before
 * secondary cores are brought up.
 *
 * Mechanism:
 *   1. Encode the target core as domain_id = (AFF1 << 8) | AFF0.
 *   2. Send RESET_REQUEST (protocol 0x16, message 0x4) to CPUCP with:
 *        flags       = AUTONOMOUS_RESET_ACTION (synchronous, autonomous)
 *        reset_state = IMPL_RESET | CL4
 *   3. Enter WFI.  CPUCP applies the workaround and releases the core.
 *   4. Execution resumes from WFI+4 (the instruction after wfi).
 *
 * Returns 0 on success, or a negative SCMI error code on failure.
 */
int qti_ncc_cpu_reset_sync(uint64_t mpidr)
{
	void *ch;
	uint32_t domain_id;
	uint32_t flags;
	uint32_t reset_state;
	int ret;
	uint8_t core_id    = (uint8_t)MPIDR_AFFLVL0_VAL(mpidr);
	uint8_t cluster_id = (uint8_t)MPIDR_AFFLVL1_VAL(mpidr);

	ch = qti_scmi_get_channel();
	if (ch == NULL) {
		ERROR("NCC: failed to get SCMI channel\n");
		return -1;
	}

	/*
	 * Encode the target reset domain as CPUCP expects:
	 *   bits [15:8] = cluster index (MPIDR AFF1)
	 *   bits  [7:0] = core index within the cluster (MPIDR AFF0)
	 */
	domain_id = ((uint32_t)cluster_id << 8U) | (uint32_t)core_id;

	/*
	 * flags: Bit[0] = AUTONOMOUS_ACTION = 1 (platform drives the sequence),
	 *        Bit[2] = ASYNC_FLAG = 0 (synchronous).
	 * Per DEN0056F §3.8.2.6: Bit[0] = Autonomous Reset action.
	 */
	flags = SCMI_RESET_FLAG_SYNC | SCMI_RESET_FLAG_AUTONOMOUS;

	/*
	 * reset_state: IMPL_RESET | CL4.
	 * CPUCP interprets this as "apply the CL4 workaround for this core".
	 */
	reset_state = NCC_RESET_STATE_CL4;

	ret = scmi_reset_domain_request(ch, domain_id, flags, reset_state);
	if (ret != SCMI_E_SUCCESS) {
		ERROR("NCC: SCMI reset domain request failed: %d\n", ret);
		return ret;
	}

	/*
	 * SCMI command acknowledged.  Enter WFI; CPUCP applies the workaround
	 * and releases the core.  Execution resumes here (WFI+4).
	 */
	wfi();

	return 0;
}
